#include "store.hpp"
#include "store/range_merger/range_merger.hpp"

#include <chrono>

namespace Hill {
    namespace Store {
        auto StoreServer::launch(int num_threads) -> bool {
#if defined(__HILL_DEBUG__) || defined(__HILL_INFO__)
            std::cout << ">> Launching server node at " << server->get_addr_uri() << "\n";
            std::cout << ">> B+ Tree degree is " << Indexing::Constants::iDEGREE << "\n";
#endif
            is_launched = server->launch();
            if (!is_launched) {
                return false;
            }

            num_launched_threads = num_threads;
            int i;
            for (i = 0; i < num_threads; i++) {
                std::thread([&](int btid) {
                    tid_lock.lock();
                    auto atid = server->get_allocator()->register_thread();
                    if (!atid.has_value()) {
                        throw std::runtime_error("Failed to register memeory allocator during server launching");
                    }

                    auto ltid = server->get_logger()->register_thread();
                    if (!ltid.has_value()) {
                        throw std::runtime_error("Failed to register memeory allocator during server launching");
                    }
                    tid_lock.unlock();

                    if (atid.value() != ltid.value()) {
                        throw std::runtime_error("Tids differ in logger and memory allocator");
                    }

                    auto tid = atid.value();
#if defined(__HILL_DEBUG__) || defined(__HILL_INFO__)
                    std::cout << ">> Launching background thread " << btid << "\n";
#endif

                    Indexing::OLFIT olfit(atid.value(), server->get_allocator(), server->get_logger());
                    leaves[btid] = olfit.get_root().get_as<Indexing::LeafNode *>();
                    while (is_launched) {
                        IncomeMessage *msg;
                        if (req_queues[btid].pop(msg)) {
                            switch (msg->input.op) {
                            case Enums::RPCOperations::Update: {
                                auto [status, value_ptr] = olfit.update(tid, msg->input.key, msg->input.key_size,
                                                                        msg->input.value, msg->input.value_size);
                                msg->output.value = value_ptr;
                                msg->output.status.store(status);
                                // update here is not atomic but it's ok,
                                // because we just send temporal values to other servers and get_consumed is atomic
                                // so we wouldn't have INCORRECT values
                                server->get_node()->available_pm = server->get_node()->total_pm - server->get_allocator()->get_consumed();
                            }
                                break;
                            case Enums::RPCOperations::Insert: {
                                auto [status, value_ptr] = olfit.insert(tid, msg->input.key, msg->input.key_size,
                                                                        msg->input.value, msg->input.value_size,
                                                                        msg->input.hkey, msg->input.hvalue);
                                msg->output.value = value_ptr;
                                msg->output.status.store(status);

                                server->get_node()->available_pm = server->get_node()->total_pm - server->get_allocator()->get_consumed();
                            }
                                break;
                            case Enums::RPCOperations::Search: {
                                auto [v, v_sz] = olfit.search(msg->input.key, msg->input.key_size);
                                if (v == nullptr) {
                                    msg->output.value = nullptr;
                                    msg->output.status.store(Indexing::Enums::OpStatus::Failed);
                                    break;
                                }
                                msg->output.value = v;
                                msg->output.value_size = v_sz;
                                msg->output.status.store(Indexing::Enums::OpStatus::Ok);
                            }
                                break;
                            case Enums::RPCOperations::Range: {
                                auto vec = olfit.scan(msg->input.key, msg->input.key_size, msg->input.value_size);
                                msg->output.values = std::move(vec);
                                if (vec.size() != 0) {
                                    msg->output.status.store(Indexing::Enums::OpStatus::Ok);
                                } else {
                                    msg->output.status.store(Indexing::Enums::OpStatus::Failed);
                                }
                            }
                                break;
                            case Enums::RPCOperations::CallForMemory:
                                olfit.enable_agent(msg->input.agent);
                                msg->output.status.store(Indexing::Enums::OpStatus::Ok);
                                break;
                            default:
                                msg->output.status.store(Indexing::Enums::OpStatus::Failed);
                                break;
                            }
                        }
                    }
                }, i).detach();
            }

            return true;
        }

        auto StoreServer::launch_one_erpc_listen_thread() -> bool {
            if (!is_launched) {
                return false;
            }

            auto sock = Misc::make_async_socket(true, server->get_node()->erpc_listen_port);
            if (sock == -1) {
#ifdef __HILL__INFO__
                std::cout << ">> Unable to create socket for eRPC listening\n";
#endif
                return false;
            }

            std::thread t([&, sock]() {
                while(is_launched) {
                    auto socket = Misc::accept_blocking(sock);

                    if (socket != -1) {
                        auto tmp = erpc_ids[(erpc_id_cursor++) % erpc_ids.size()];
                        write(socket, &tmp, sizeof(tmp));

                        // check if there are any possible succeeding requests
                        shutdown(socket, 0);
                        continue;
                    }
                    sleep(1);
                }
            });
            t.detach();

            return true;
        }

        auto StoreServer::launch_one_memory_monitor_thread() -> bool {
            std::thread t([&] {
                auto rm_rpc =  new erpc::Rpc<erpc::CTransport>(this->nexus, reinterpret_cast<void *>(this),
                                                               Memory::Constants::iTHREAD_LIST_NUM,
                                                               RPCWrapper::ghost_sm_handler);
                IncomeMessage msg;
                while(this->is_launched) {
                    for (auto &i : this->contexts) {
                        if (i == nullptr)
                            continue;

                        if (i->need_memory.load() == true) {
                            auto ptr = check_available_mem(rm_rpc, *i, this->index_ids[i->thread_id]);
                            server->get_agent()->add_region(this->index_ids[i->thread_id], ptr);

                            msg.input.op = Enums::RPCOperations::CallForMemory;
                            msg.input.agent = server->get_agent();
                            while(!i->queues[this->index_ids[i->thread_id]].push(&msg));

                            while(msg.output.status.load() == Indexing::Enums::OpStatus::Unkown);

                            i->need_memory.store(false);
                        }
                    }
                    sleep(1);
                }
            });
            t.detach();

            return true;
        }

        auto StoreServer::register_erpc_handler_thread() noexcept -> std::optional<std::thread> {
            if (!is_launched) {
                return {};
            }

            auto tid = server->register_thread();

            return std::thread([&] (int tid) {
                ServerContext s_ctx;
                s_ctx.thread_id = tid;
                s_ctx.self = this;
                s_ctx.node_id = this->server->get_node()->node_id;
                s_ctx.server = this->server.get();
                s_ctx.queues = this->req_queues;
                s_ctx.num_launched_threads = this->num_launched_threads;

                s_ctx.handle_sampler = new HandleSampler(10000);
                s_ctx.handle_sampler->prepare();
#ifdef __HILL_INFO__
                std::cout << ">> Creating eRPC for thread " << tid << ", remote id is also " << tid << "\n";
#endif
                s_ctx.rpc = new erpc::Rpc<erpc::CTransport>(this->nexus, reinterpret_cast<void *>(&s_ctx),
                                                            tid, RPCWrapper::ghost_sm_handler);
                rpc_id_lock.lock();
                this->erpc_ids.push_back(tid);
                rpc_id_lock.unlock();
                s_ctx.nexus = this->nexus;

                this->contexts[tid] = &s_ctx;

                while(true) {
                    s_ctx.rpc->run_event_loop(2000);
#ifdef __HILL_INFO__
                    std::cout << ">> Insert breakdown: "; s_ctx.handle_sampler->report_insert(); std::cout << "\n";
                    std::cout << ">> Search breakdown: "; s_ctx.handle_sampler->report_search(); std::cout << "\n";
                    std::cout << ">> Update breakdown: "; s_ctx.handle_sampler->report_update(); std::cout << "\n";
                    std::cout << ">> Range breakdown: "; s_ctx.handle_sampler->report_scan(); std::cout << "\n\n";
#endif
                }

                this->server->unregister_thread(tid);
            }, tid);
        }

        auto StoreServer::use_agent() noexcept -> void {

        }

        auto StoreServer::check_available_mem(erpc::Rpc<erpc::CTransport> *rm_rpc, ServerContext &s_ctx, int tid)
            -> Memory::RemotePointer
        {
            size_t max = 0;
            size_t node_id;
            const auto &meta = s_ctx.server->get_node()->cluster_status;
            // skip monitor
            for (size_t i = 1; i <= meta.cluster.node_num; i++) {
                if (i == s_ctx.server->get_node()->node_id)
                    continue;

                if (max < meta.cluster.nodes[i].available_pm) {
                    max = meta.cluster.nodes[i].available_pm;
                    node_id = i;
                }
            }

            std::cout << "Establishing RDMA connection for Bthread " << tid << "\n";
            if (!s_ctx.server->server_connected(tid, node_id)) {
                if (!s_ctx.server->connect_server(tid, node_id)) {
                    std::cerr << ">> Error: can't connect remote server " << node_id << " for more memory\n";
                    return nullptr;
                }
            }
            std::cout << "Done\n";

            std::cout << "Establishing eRPC connection for Bthread " << tid << "\n";
            if (s_ctx.erpc_sessions[node_id] == -1) {
                if (!establish_memory_erpc(rm_rpc, s_ctx, tid, node_id)) {
                    std::cerr << ">> Error: can't connect remote server " << node_id << "'s rpc\n";
                    return nullptr;
                }
            }

            auto buf = s_ctx.req_bufs[node_id].buf;
            s_ctx.is_done = false;

#ifdef __HILL_INFO__
            std::cout << ">> Requesting remote memory\n";
#endif
            *reinterpret_cast<Enums::RPCOperations *>(buf) = Enums::RPCOperations::CallForMemory;
            rm_rpc->enqueue_request(s_ctx.erpc_sessions[node_id], Enums::RPCOperations::CallForMemory,
                                       &s_ctx.req_bufs[node_id], &s_ctx.resp_bufs[node_id],
                                       response_continuation, &s_ctx);
            while (!s_ctx.is_done) {
                rm_rpc->run_event_loop_once();
            }

            auto pbuf = s_ctx.resp_bufs[node_id].buf;
            auto offset = sizeof(Enums::RPCOperations) + sizeof(Enums::RPCStatus);
            auto ptr = *reinterpret_cast<Memory::RemotePointer *>(pbuf + offset);
            if (ptr.is_nullptr()) {
                throw std::runtime_error("Remote memory is also depleted");
            }
#ifdef __HILL_INFO__
            std::cout << ">> Got remote memory at " << ptr.void_ptr() << "\n";
#endif
            return ptr;
        }

        auto StoreServer::establish_memory_erpc(erpc::Rpc<erpc::CTransport> *rm_rpc, ServerContext &s_ctx, int tid, int node_id)
            -> bool
        {
            if (s_ctx.erpc_sessions[node_id] != -1) {
                return true;
            }

            const auto &meta = s_ctx.server->get_node()->cluster_status;
#ifdef __HILL_INFO__
            std::cout << ">> Creating eRPC for thread " << tid << "\n";
            std::cout << ">> Connecting to listen port: " << meta.cluster.nodes[node_id].erpc_listen_port << "\n";
#endif
            auto socket = Misc::socket_connect(false,
                                               meta.cluster.nodes[node_id].erpc_listen_port,
                                               meta.cluster.nodes[node_id].addr.to_string().c_str());
#ifdef __HILL_INFO__
            std::cout << ">> Connected\n";
#endif
            auto remote_id = 0;
            read(socket, &remote_id, sizeof(remote_id));
#ifdef __HILL_INFO__
            std::cout << ">> Establishing eRPC connection for remote memory with ID " << remote_id << "\n";
#endif
            auto &node = meta.cluster.nodes[node_id];
            auto server_uri = node.addr.to_string() + ":" + std::to_string(node.erpc_port);

            s_ctx.erpc_sessions[node_id] = rm_rpc->create_session(server_uri, remote_id);
            if (s_ctx.erpc_sessions[node_id] == -1) {
                throw std::runtime_error("Failed to create eRPC session for server");
            }

            while (!rm_rpc->is_connected(s_ctx.erpc_sessions[node_id])) {
                rm_rpc->run_event_loop_once();
            }
#ifdef __HILL_INFO__
            std::cout << ">> eRPC connection established\n";
#endif
            s_ctx.req_bufs[node_id] = rm_rpc->alloc_msg_buffer_or_die(64);
            s_ctx.resp_bufs[node_id] = rm_rpc->alloc_msg_buffer_or_die(64);
            shutdown(socket, 0);
            return true;
        }

        auto StoreServer::response_continuation(void *context, void *tag) -> void {
            UNUSED(context);
            reinterpret_cast<ServerContext *>(tag)->is_done = true;
        }

        auto StoreServer::insert_handler(erpc::ReqHandle *req_handle, void *context) -> void {
            auto ctx = reinterpret_cast<ServerContext *>(context);
            auto server = ctx->server;
#ifdef __HILL_SAMPLE__
            auto handle_sampler = ctx->handle_sampler;
            auto &sampler = handle_sampler->insert_sampler;
#endif
            Enums::RPCOperations type; KVPair::HillString *key, *value;
#ifdef __HILL_SAMPLE__
            {
                SampleRecorder<uint64_t> _(sampler, HandleSampler::PARSE);
#endif
                auto r = parse_request_message(req_handle, context);
                type = std::get<0>(r);
                key = std::get<1>(r);
                value = std::get<2>(r);
#ifdef __HILL_SAMPLE__
            }
#endif
            IncomeMessage msg;
            msg.input.key = key->raw_chars();
            msg.input.key_size = key->size();
            msg.input.value = value->raw_chars();
            msg.input.value_size = value->size();
            msg.input.op = type;

            msg.input.hkey = key;
            msg.input.hvalue = value;

            msg.output.status = Indexing::Enums::OpStatus::Unkown;
            // this is fast we do not need to sample
            auto pos = CityHash64(msg.input.key, msg.input.key_size) % ctx->num_launched_threads;
            auto allowed = Constants::dNODE_CAPPACITY_LIMIT * server->get_node()->total_pm;
            bool insufficient = false;
#ifdef __HILL_SAMPLE__
            {
                SampleRecorder<uint64_t> _(sampler, HandleSampler::CAP_CHECK);
#endif
                insufficient = server->get_allocator()->get_consumed() >= allowed &&
                    !server->get_agent()->available(pos);
                if (insufficient) {
                    ctx->self->agent_locks[pos].lock();
                    insufficient = server->get_allocator()->get_consumed() >= allowed &&
                        !server->get_agent()->available(pos);

                    if (insufficient) {
#ifdef __HILL_INFO__
                        std::cout << "Fthread " << ctx->thread_id << " asking for remote memory for Bthread " << pos << "\n";
#endif
                        ctx->self->index_ids[ctx->thread_id] = pos;
                        ctx->self->contexts[ctx->thread_id]->need_memory = true;

                        while(ctx->self->contexts[ctx->thread_id]->need_memory);

                    }
                    ctx->self->agent_locks[pos].unlock();
                }
#ifdef __HILL_SAMPLE__
            }
#endif
        retry:
#ifdef __HILL_SAMPLE__
            {
                SampleRecorder<uint64_t> _(sampler, HandleSampler::INDEXING);
#endif
                while(!ctx->queues[pos].push(&msg));

                while(msg.output.status.load() == Indexing::Enums::OpStatus::Unkown);
#ifdef __HILL_SAMPLE__
            }
#endif
            auto &resp = req_handle->pre_resp_msgbuf;
            constexpr auto total_msg_size = sizeof(Enums::RPCOperations) + sizeof(Enums::RPCStatus) + sizeof(Memory::PolymorphicPointer);
            ctx->rpc->resize_msg_buffer(&resp, total_msg_size);

#ifdef __HILL_SAMPLE__
            {
                SampleRecorder<uint64_t> _(sampler, HandleSampler::RESP_MSG);
#endif

                *reinterpret_cast<Enums::RPCOperations *>(resp.buf) = Enums::RPCOperations::Insert;
                auto offset = sizeof(Enums::RPCOperations);
                switch(msg.output.status.load()){
                case Indexing::Enums::OpStatus::Ok:
                    *reinterpret_cast<Enums::RPCStatus *>(resp.buf + offset) = Enums::RPCStatus::Ok;
                    break;
                case Indexing::Enums::OpStatus::NoMemory:
                    *reinterpret_cast<Enums::RPCStatus *>(resp.buf + offset) = Enums::RPCStatus::NoMemory;
                    // agent's memory is available but not sufficient;
                    ctx->self->agent_locks[pos].lock();
                    ctx->self->index_ids[ctx->thread_id] = pos;
                    ctx->self->contexts[ctx->thread_id]->need_memory = true;

                    while(ctx->self->contexts[ctx->thread_id]->need_memory);
                    ctx->self->agent_locks[pos].unlock();

                    goto retry;
                default:
                    *reinterpret_cast<Enums::RPCStatus *>(resp.buf + offset) = Enums::RPCStatus::Failed;
                    break;
                }

                offset += sizeof(Enums::RPCStatus);
                *reinterpret_cast<Memory::PolymorphicPointer *>(resp.buf + offset) = msg.output.value;
#ifdef __HILL_SAMPLE__
            }
#endif
#ifdef __HILL_SAMPLE__
            {
                SampleRecorder<uint64_t> _(sampler, HandleSampler::RESP);
#endif
                ctx->rpc->enqueue_response(req_handle, &resp);
#ifdef __HILL_SAMPLE__
            }
#endif
            if (msg.output.status.load() == Indexing::Enums::OpStatus::Failed) {
                std::cout << "Inserting " << key->to_string() << " failed\n";
            }
        }

        auto StoreServer::update_handler(erpc::ReqHandle *req_handle, void *context) -> void {
            auto ctx = reinterpret_cast<ServerContext *>(context);
            auto server = ctx->server;
#ifdef __HILL_SAMPLE__
            auto handle_sampler = ctx->handle_sampler;
            auto &sampler = handle_sampler->update_sampler;
#endif
            Enums::RPCOperations type; KVPair::HillString *key, *value;
#ifdef __HILL_SAMPLE__
            {
                SampleRecorder<uint64_t> _(sampler, HandleSampler::PARSE);
#endif
                auto r = parse_request_message(req_handle, context);
                type = std::get<0>(r);
                key = std::get<1>(r);
                value = std::get<2>(r);
#ifdef __HILL_SAMPLE__
            }
#endif
            IncomeMessage msg;
            msg.input.key = key->raw_chars();
            msg.input.key_size = key->size();
            msg.input.value = value->raw_chars();
            msg.input.value_size = value->size();
            msg.input.op = type;

            msg.output.status = Indexing::Enums::OpStatus::Unkown;
            auto pos = CityHash64(msg.input.key, msg.input.key_size) % ctx->num_launched_threads;
            auto allowed = Constants::dNODE_CAPPACITY_LIMIT * server->get_node()->total_pm;
            bool insufficient = false;
#ifdef __HILL_SAMPLE__
            {
                SampleRecorder<uint64_t> _(sampler, HandleSampler::CAP_CHECK);
#endif
                insufficient = server->get_allocator()->get_consumed() >= allowed &&
                    !server->get_agent()->available(pos);
                if (insufficient) {
                    ctx->self->agent_locks[pos].lock();
                    insufficient = server->get_allocator()->get_consumed() >= allowed &&
                        !server->get_agent()->available(pos);

                    if (insufficient) {
#ifdef __HILL_INFO__
                        std::cout << "Fthread " << ctx->thread_id << " asking for remote memory for Bthread " << pos << "\n";
#endif
                        ctx->self->index_ids[ctx->thread_id] = pos;
                        ctx->self->contexts[ctx->thread_id]->need_memory = true;

                        while(ctx->self->contexts[ctx->thread_id]->need_memory);

                    }
                    ctx->self->agent_locks[pos].unlock();
                }
#ifdef __HILL_SAMPLE__
            }
#endif
        retry:
#ifdef __HILL_SAMPLE__
            {
                SampleRecorder<uint64_t> _(sampler, HandleSampler::INDEXING);
#endif
                while(!ctx->queues[pos].push(&msg));

                while(msg.output.status.load() == Indexing::Enums::OpStatus::Unkown);
#ifdef __HILL_SAMPLE__
            }
#endif
            auto &resp = req_handle->pre_resp_msgbuf;
            constexpr auto total_msg_size = sizeof(Enums::RPCOperations) + sizeof(Enums::RPCStatus) + sizeof(Memory::PolymorphicPointer);
            ctx->rpc->resize_msg_buffer(&resp, total_msg_size);
#ifdef __HILL_SAMPLE__
            {
                SampleRecorder<uint64_t> _(sampler, HandleSampler::RESP_MSG);
#endif
                *reinterpret_cast<Enums::RPCOperations *>(resp.buf) = Enums::RPCOperations::Update;
                auto offset = sizeof(Enums::RPCOperations);

                switch(msg.output.status.load()){
                case Indexing::Enums::OpStatus::Ok:
                    *reinterpret_cast<Enums::RPCStatus *>(resp.buf + offset) = Enums::RPCStatus::Ok;
                    break;
                case Indexing::Enums::OpStatus::NoMemory:
                    *reinterpret_cast<Enums::RPCStatus *>(resp.buf + offset) = Enums::RPCStatus::NoMemory;
                    // agent's memory is available but not sufficient;
                    ctx->self->agent_locks[pos].lock();
                    ctx->self->index_ids[ctx->thread_id] = pos;
                    ctx->self->contexts[ctx->thread_id]->need_memory = true;

                    while(ctx->self->contexts[ctx->thread_id]->need_memory);
                    ctx->self->agent_locks[pos].unlock();

                    goto retry;
                default:
                    *reinterpret_cast<Enums::RPCStatus *>(resp.buf + offset) = Enums::RPCStatus::Failed;
                    break;
                }

                offset += sizeof(Enums::RPCStatus);
                *reinterpret_cast<Memory::PolymorphicPointer *>(resp.buf + offset) = msg.output.value;
#ifdef __HILL_SAMPLE__
            }
#endif
#ifdef __HILL_SAMPLE__
            {
                SampleRecorder<uint64_t> _(sampler, HandleSampler::RESP);
#endif
                ctx->rpc->enqueue_response(req_handle, &resp);
#ifdef __HILL_SAMPLE__
            }
#endif
            if (msg.output.status.load() == Indexing::Enums::OpStatus::Failed) {
                std::cout << "Updating " << key->to_string() << " failed\n";
            }
        }

        auto StoreServer::search_handler(erpc::ReqHandle *req_handle, void *context) -> void {
            auto ctx = reinterpret_cast<ServerContext *>(context);
#ifdef __HILL_SAMPLE__
            auto handle_sampler = ctx->handle_sampler;
            auto &sampler = handle_sampler->search_sampler;
#endif
            Enums::RPCOperations type; KVPair::HillString *key, *value;
#ifdef __HILL_SAMPLE__
            {
                SampleRecorder<uint64_t> _(sampler, HandleSampler::PARSE);
#endif
                auto t = parse_request_message(req_handle, context);
                type = std::get<0>(t);
                key = std::get<1>(t);
                value = std::get<2>(t);
#ifdef __HILL_SAMPLE__
            }
#endif
            IncomeMessage msg;
            msg.input.key = key->raw_chars();
            msg.input.key_size = key->size();
            msg.input.op = type;
            msg.output.status = Indexing::Enums::OpStatus::Unkown;

            auto pos = CityHash64(msg.input.key, msg.input.key_size) % ctx->num_launched_threads;
#ifdef __HILL_SAMPLE__
            {
                SampleRecorder<uint64_t> _(sampler, HandleSampler::INDEXING);
#endif
                while(!ctx->queues[pos].push(&msg));

                while(msg.output.status.load() == Indexing::Enums::OpStatus::Unkown);
#ifdef __HILL_SAMPLE__
            }
#endif
            auto& resp = req_handle->pre_resp_msgbuf;
            constexpr auto total_msg_size = sizeof(Enums::RPCOperations) + sizeof(Memory::PolymorphicPointer)
                + sizeof(size_t) + sizeof(Enums::RPCStatus);

#ifdef __HILL_SAMPLE__
            {
                SampleRecorder<uint64_t> _(sampler, HandleSampler::RESP_MSG);
#endif
                ctx->rpc->resize_msg_buffer(&resp, total_msg_size);
                *reinterpret_cast<Enums::RPCOperations *>(resp.buf) = Enums::RPCOperations::Search;

                auto offset = sizeof(Enums::RPCOperations);
                if (msg.output.value == nullptr) {
                    *reinterpret_cast<Enums::RPCStatus *>(resp.buf + offset) = Enums::RPCStatus::Failed;
                } else {
                    *reinterpret_cast<Enums::RPCStatus *>(resp.buf + offset) = Enums::RPCStatus::Ok;

                    offset += sizeof(Enums::RPCStatus);
                    if (msg.output.value.is_remote()) {
                        *reinterpret_cast<Memory::PolymorphicPointer *>(resp.buf + offset) = msg.output.value;
                    } else {
                        auto poly = Memory::PolymorphicPointer::make_polymorphic_pointer(Memory::RemotePointer::make_remote_pointer(ctx->node_id, msg.output.value.local_ptr()));
                        *reinterpret_cast<Memory::PolymorphicPointer *>(resp.buf + offset) = poly;
                    }

                    offset += sizeof(Memory::PolymorphicPointer);
                    *reinterpret_cast<size_t *>(resp.buf + offset) =
                        msg.output.value.is_remote() ? msg.output.value_size + 64 : msg.output.value_size;
                }
#ifdef __HILL_SAMPLE__
            }
#endif
#ifdef __HILL_SAMPLE__
            {
                SampleRecorder<uint64_t> _(sampler, HandleSampler::RESP);
#endif
                ctx->rpc->enqueue_response(req_handle, &resp);
#ifdef __HILL_SAMPLE__
            }
#endif
            if (msg.output.status.load() == Indexing::Enums::OpStatus::Failed) {
                std::cout << "Searching " << key->to_string() << " failed\n";
            }
        }

        auto StoreServer::range_handler(erpc::ReqHandle *req_handle, void *context) -> void {
            auto ctx = reinterpret_cast<ServerContext *>(context);
#ifdef __HILL_SAMPLE__
            auto handle_sampler = ctx->handle_sampler;
            auto &sampler = handle_sampler->scan_sampler;
#endif
            Enums::RPCOperations type; KVPair::HillString *key, *value;
#ifdef __HILL_SAMPLE__
            {
                SampleRecorder<uint64_t> _(sampler, HandleSampler::PARSE);
#endif
                auto t = parse_request_message(req_handle, context);
                type = std::get<0>(t);
                key = std::get<1>(t);
                value = std::get<2>(t);
#ifdef __HILL_SAMPLE__
            }
#endif

            IncomeMessage msgs[Memory::Constants::iTHREAD_LIST_NUM];
            std::vector<std::vector<Indexing::ScanHolder>> ranges;
#ifdef __HILL_SAMPLE__
            {
                SampleRecorder<uint64_t> _(sampler, HandleSampler::INDEXING);
#endif
                for (auto i = 0; i < ctx->num_launched_threads; i++) {
                    msgs[i].input.key = key->raw_chars();
                    msgs[i].input.key_size = key->size();
                    msgs[i].input.value_size = *reinterpret_cast<size_t *>(value);
                    msgs[i].input.op = type;
                    msgs[i].output.status = Indexing::Enums::OpStatus::Unkown;
                    while(!ctx->queues[i].push(&msgs[i]));
                }

                for (auto i = 0; i < ctx->num_launched_threads; i++) {
                    while(msgs[i].output.status.load() == Indexing::Enums::OpStatus::Unkown);
                    ranges.push_back(std::move(msgs[i].output.values));
                }
#ifdef __HILL_SAMPLE__
            }
#endif
            // all partitions are collected
            size_t ret;
#ifdef __HILL_SAMPLE__
            {
                SampleRecorder<uint64_t> _(sampler, HandleSampler::MERGE);
#endif
                auto merger = Merger::make_merger(ranges);
                auto holders = merger->merge(msgs[0].input.value_size);
                ret = holders.size();
#ifdef __HILL_SAMPLE__
            }
#endif

            auto& resp = req_handle->pre_resp_msgbuf;
#ifdef __HILL_SAMPLE__
            {
                SampleRecorder<uint64_t> _(sampler, HandleSampler::RESP_MSG);
#endif
                constexpr auto total_msg_size = sizeof(Enums::RPCOperations) + sizeof(Memory::PolymorphicPointer)
                    + sizeof(size_t) + sizeof(Enums::RPCStatus);

                ctx->rpc->resize_msg_buffer(&resp, total_msg_size);
                *reinterpret_cast<Enums::RPCOperations *>(resp.buf) = Enums::RPCOperations::Range;

                auto offset = sizeof(Enums::RPCOperations);
                *reinterpret_cast<Enums::RPCStatus *>(resp.buf + offset) = Enums::RPCStatus::Ok;

                offset += sizeof(Enums::RPCStatus);
                *reinterpret_cast<size_t *>(resp.buf + offset) = ret;
#ifdef __HILL_SAMPLE__
            }
#endif
#ifdef __HILL_SAMPLE__
            {
                SampleRecorder<uint64_t> _(sampler, HandleSampler::RESP);
#endif
                ctx->rpc->enqueue_response(req_handle, &resp);
#ifdef __HILL_SAMPLE__
            }
#endif
        }

        auto StoreServer::memory_handler(erpc::ReqHandle *req_handle, void *context) -> void {
            auto ctx = reinterpret_cast<ServerContext *>(context);
            auto tid = ctx->thread_id;
            auto allocator = ctx->server->get_allocator();
            auto logger = ctx->server->get_logger();
            auto &ptr = logger->make_log(tid, WAL::Enums::Ops::RemoteMemory);

            allocator->allocate_for_remote(ptr);

            auto &resp = req_handle->pre_resp_msgbuf;
            constexpr auto total_msg_size = sizeof(Enums::RPCOperations) + sizeof(Enums::RPCStatus) + sizeof(Memory::RemotePointer);

            ctx->rpc->resize_msg_buffer(&resp, total_msg_size);
            *reinterpret_cast<Enums::RPCOperations *>(resp.buf) = Enums::RPCOperations::CallForMemory;
            auto offset = sizeof(Enums::RPCOperations);

            if (ptr) {
                *reinterpret_cast<Enums::RPCStatus *>(resp.buf + offset) = Enums::RPCStatus::Ok;
            } else {
                *reinterpret_cast<Enums::RPCStatus *>(resp.buf + offset) = Enums::RPCStatus::NoMemory;
            }
            offset += sizeof(Enums::RPCStatus);

            auto rptr = Memory::RemotePointer::make_remote_pointer(ctx->node_id, ptr);
            *reinterpret_cast<Memory::RemotePointer *>(resp.buf + offset) = rptr;
#ifdef __HILL_INFO__
            std::cout << ">> Offering remote memory " << rptr.void_ptr() << "\n";
#endif

            ctx->rpc->enqueue_response(req_handle, &resp);
            logger->commit(tid);
        }

        auto StoreServer::parse_request_message(const erpc::ReqHandle *req_handle, const void *ctx)
            -> std::tuple<Enums::RPCOperations, KVPair::HillString *, KVPair::HillString *>
        {
            UNUSED(ctx);
            auto requests = req_handle->get_req_msgbuf();

            auto buf = requests->buf;
            auto type = *reinterpret_cast<Enums::RPCOperations *>(buf);
            KVPair::HillString *key = nullptr, *key_or_value = nullptr;
            buf += sizeof(Enums::RPCOperations);

            switch(type){
            case Enums::RPCOperations::Insert:
                [[fallthrough]];
            case Enums::RPCOperations::Range:
                // range here gets a HillString * for value,
                // we should cast it to size_t outside this function
                [[fallthrough]];
            case Enums::RPCOperations::Update:
                key = reinterpret_cast<hill_key_t *>(buf);
                buf += key->object_size();
                key_or_value = reinterpret_cast<hill_value_t *>(buf);
                break;
            case Enums::RPCOperations::Search:
                key = reinterpret_cast<hill_key_t *>(buf);
                break;
            case Enums::RPCOperations::CallForMemory:
                break;
            default:
                type = Enums::RPCOperations::Unknown;
                break;
            }

            return {type, key, key_or_value};
        }

        auto StoreClient::register_thread(const Workload::StringWorkload &load, Stats::SyntheticStats &stats)
            noexcept -> std::optional<std::thread>
        {
            if (!is_launched) {
                return {};
            }

            auto tid = client->register_thread();
            if (!tid.has_value()) {
                return {};
            }

            return std::thread([&](int tid) {
#if defined(__HILL_DEBUG__) || defined(__HILL_INFO__)
                std::cout << ">> Client thread launched\n";
#endif
                ClientContext c_ctx;
                c_ctx.thread_id = tid;
                c_ctx.client = this->client.get();

                std::optional<int> _node_id;
                int node_id;
                stats.reset();
                size_t counter = 0;
                std::chrono::time_point<std::chrono::steady_clock> start, end;
                c_ctx.rpc = new erpc::Rpc<erpc::CTransport>(nexus, reinterpret_cast<void *>(&c_ctx),
                                                            tid, RPCWrapper::ghost_sm_handler);

                c_ctx.client_sampler = new Sampling::ClientSampler(10000);
                c_ctx.client_sampler->prepare();

                if (!connect_all_servers(tid, c_ctx)) {
                    std::cerr << ">> Failed to connect all servers\n";
                    return ;
                }

                stats.throughputs.timing_now();
                start = std::chrono::steady_clock::now();
                Sampling::Sampler<uint64_t> *sampler = nullptr;
                for (auto &i : load) {
#ifdef __HILL_SAMPLE__
                    switch (i.type) {
                    case Workload::Enums::Insert:
                        sampler = &c_ctx.client_sampler->insert_sampler;
                        break;
                    case Workload::Enums::Search:
                        sampler = &c_ctx.client_sampler->search_sampler;
                        break;
                    case Workload::Enums::Update:
                        sampler = &c_ctx.client_sampler->update_sampler;
                        break;
                    case Workload::Enums::Range:
                        sampler = &c_ctx.client_sampler->scan_sampler;
                        break;
                    default:
                        break;
                    }
#endif
                    if (i.type == Workload::Enums::Search) {
#ifdef __HILL_SAMPLE__
                        {
                            SampleRecorder<size_t> _(*sampler, ClientSampler::CACHE);
#endif
                            auto ret = c_ctx.cache.get(i.key);
                            if (ret != nullptr) {
#ifdef __HILL_FETCH_VALUE__
#ifdef __HILL_SAMPLE__
                                {
                                    SampleRecorder<size_t> _(*sampler, ClientSampler::CACHE_RDMA);
#endif
                                    auto re_ptr = ret->value_ptr.remote_ptr();
                                    c_ctx.client->read_from(c_ctx.thread_id, re_ptr.get_node(),
                                                            re_ptr.get_as<byte_ptr_t>(), ret->value_size);
                                    c_ctx.client->poll_completion_once(c_ctx.thread_id, re_ptr.get_node());
#ifdef __HILL_SAMPLE__
                                }
#endif
#endif
                                ++c_ctx.num_search;
                                ++c_ctx.suc_search;
                                ++c_ctx.RTTs[1];
                                goto sample;
                            }
#ifdef __HILL_SAMPLE__
                        }
#endif
                    }

                    c_ctx.is_done = false;
#ifdef __HILL_SAMPLE__
                    {
                        SampleRecorder<size_t> _(*sampler, ClientSampler::CHECK_RPC);
#endif
                        _node_id = c_ctx.client->get_cluster_meta().filter_node(i.key);
#ifdef __HILL_SAMPLE__
                    }
#endif
                    if (!_node_id.has_value()) {
                        continue;
                    }

                    node_id = _node_id.value();

#ifdef __HILL_SAMPLE__
                    {
                        SampleRecorder<size_t> _(*sampler, ClientSampler::PRE_REQ);
#endif
                        prepare_request(node_id, i, c_ctx);
#ifdef __HILL_SAMPLE__
                    }
#endif
                    // cache is updated in the response_continuation
#ifdef __HILL_SAMPLE__
                    {
                        SampleRecorder<size_t> _(*sampler, ClientSampler::RPC);
#endif
                        c_ctx.rpc->enqueue_request(c_ctx.erpc_sessions[node_id], i.type,
                                                   &c_ctx.req_bufs[node_id], &c_ctx.resp_bufs[node_id],
                                                   response_continuation, &node_id);
                        while(!c_ctx.is_done) {
                            c_ctx.rpc->run_event_loop_once();
                        }
#ifdef __HILL_SAMPLE__
                    }
#endif
                sample:
                    if ((++counter) % 10000 == 0) {
                        end = std::chrono::steady_clock::now();
                        double t = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
                        stats.latencies.record(t / 10000);
                        start = std::chrono::steady_clock::now();
                    }
                }
                stats.throughputs.timing_stop();
                stats.throughputs.num_ops = c_ctx.num_insert + c_ctx.num_search + c_ctx.num_update + c_ctx.num_range;
                stats.throughputs.suc_ops = c_ctx.suc_insert + c_ctx.suc_search + c_ctx.suc_update + c_ctx.suc_range;
                stats.cache_hit_ratio = c_ctx.cache.hit_ratio();
                this->client->unregister_thread(tid);

                std::cout << ">> Correctness report:\n";
                std::cout << "-->> insert: " << c_ctx.suc_insert << "/" << c_ctx.num_insert << "\n";
                std::cout << "-->> search: " << c_ctx.suc_search << "/" << c_ctx.num_search << "\n";
                std::cout << "-->> update: " << c_ctx.suc_update << "/" << c_ctx.num_update << "\n";
                std::cout << "-->> range: " << c_ctx.suc_range << "/" << c_ctx.num_range << "\n";
#ifdef __HILL_SAMPLE__
                std::cout << ">> Insert breakdown: "; c_ctx.client_sampler->report_insert(); std::cout << "\n";
                std::cout << ">> Search breakdown: "; c_ctx.client_sampler->report_search(); std::cout << "\n";
                std::cout << ">> Update breakdown: "; c_ctx.client_sampler->report_update(); std::cout << "\n";
                std::cout << ">> Range breakdown: "; c_ctx.client_sampler->report_scan(); std::cout << "\n\n";
#endif
            }, tid.value());
        }

        auto StoreClient::connect_all_servers(int tid, ClientContext &c_ctx) -> bool {
            const auto &meta = this->client->get_cluster_meta();
            const auto &cluster = meta.cluster;

            // skip the monitor
            for (int i = 1; i <= cluster.node_num; i++) {
                auto node_id = cluster.nodes[i].node_id;
                if (!client->is_connected(tid, node_id)) {
                    if (!client->connect_server(tid, node_id)) {
                        std::cerr << "Client can not connect to server " << node_id << "\n";
                        return false;
                    }
                }

#ifdef __HILL_INFO__
                std::cout << ">> Creating eRPC for thread " << tid << "\n";
                std::cout << ">> Connecting to node " << node_id << " at listen port: "
                          << meta.cluster.nodes[node_id].erpc_listen_port << "\n";
#endif
                auto socket = Misc::socket_connect(false,
                                                   meta.cluster.nodes[node_id].erpc_listen_port,
                                                   meta.cluster.nodes[node_id].addr.to_string().c_str());
#ifdef __HILL_INFO__
                std::cout << ">> Connected\n";
#endif
                auto remote_id = 0;
                read(socket, &remote_id, sizeof(remote_id));

                auto &node = meta.cluster.nodes[node_id];
                auto server_uri = node.addr.to_string() + ":" + std::to_string(node.erpc_port);
                auto rpc = c_ctx.rpc;
                c_ctx.erpc_sessions[node_id] = rpc->create_session(server_uri, remote_id);
                while (!rpc->is_connected(c_ctx.erpc_sessions[node_id])) {
                    rpc->run_event_loop_once();
                }

                c_ctx.req_bufs[node_id] = rpc->alloc_msg_buffer_or_die(64);
                c_ctx.resp_bufs[node_id] = rpc->alloc_msg_buffer_or_die(64);
                shutdown(socket, 0);
            }
            return true;
        }

        auto StoreClient::prepare_request(int node_id, const Workload::WorkloadItem &item,
                                          ClientContext &c_ctx) -> bool
        {
            auto type = item.type;
            uint8_t *buf = c_ctx.req_bufs[node_id].buf;
            c_ctx.requesting_key = &item.key;
            switch(type) {
            case Hill::Workload::Enums::WorkloadType::Update:
                *reinterpret_cast<Enums::RPCOperations *>(buf) = Enums::RPCOperations::Update;
                buf += sizeof(Enums::RPCOperations);
                KVPair::HillString::make_string(buf, item.key.c_str(), item.key.size());
                buf += reinterpret_cast<hill_key_t *>(buf)->object_size();
                KVPair::HillString::make_string(buf, item.key_or_value.c_str(), item.key_or_value.size());
                break;
            case Hill::Workload::Enums::WorkloadType::Insert:
                *reinterpret_cast<Enums::RPCOperations *>(buf) = Enums::RPCOperations::Insert;
                buf += sizeof(Enums::RPCOperations);
                KVPair::HillString::make_string(buf, item.key.c_str(), item.key.size());
                buf += reinterpret_cast<hill_key_t *>(buf)->object_size();
                KVPair::HillString::make_string(buf, item.key_or_value.c_str(), item.key_or_value.size());
                break;
            case Hill::Workload::Enums::WorkloadType::Search:
                *reinterpret_cast<Enums::RPCOperations *>(buf) = Enums::RPCOperations::Search;
                buf += sizeof(Enums::RPCOperations);
                KVPair::HillString::make_string(buf, item.key.c_str(), item.key.size());
                break;
            case Hill::Workload::Enums::WorkloadType::Range:
                *reinterpret_cast<Enums::RPCOperations *>(buf) = Enums::RPCOperations::Range;
                buf += sizeof(Enums::RPCOperations);
                KVPair::HillString::make_string(buf, item.key.c_str(), item.key.size());
                *reinterpret_cast<size_t *>(buf) = Constants::dRANGE_SIZE;
                break;
            default:
                return false;
            }
            return true;
        }

        auto StoreClient::response_continuation(void *context, void *tag) -> void {
            auto node_id = *reinterpret_cast<int *>(tag);
            auto ctx = reinterpret_cast<ClientContext *>(context);
            auto buf = ctx->resp_bufs[node_id].buf;
            auto key = *ctx->requesting_key;

            auto op = *reinterpret_cast<Enums::RPCOperations *>(buf);
            buf += sizeof(Enums::RPCOperations);
            auto status = *reinterpret_cast<Enums::RPCStatus *>(buf);
            buf += sizeof(Enums::RPCStatus);
            auto poly = *reinterpret_cast<Memory::PolymorphicPointer *>(buf);
            buf += sizeof(Memory::PolymorphicPointer);
            auto size = *reinterpret_cast<size_t *>(buf);

#ifdef __HILL_SAMPLE__
            Sampling::Sampler<uint64_t> *sampler = nullptr;
            switch (op) {
            case Enums::RPCOperations::Insert:
                sampler = &ctx->client_sampler->insert_sampler;
                break;
            case Enums::RPCOperations::Search:
                sampler = &ctx->client_sampler->search_sampler;
                break;
            case Enums::RPCOperations::Update:
                sampler = &ctx->client_sampler->update_sampler;
                break;
            case Enums::RPCOperations::Range:
                sampler = &ctx->client_sampler->scan_sampler;
                break;
            default:
                break;
            }

            {
                SampleRecorder<uint64_t> _(*sampler, ClientSampler::CONTI);
#endif
                switch(op) {
                case Enums::RPCOperations::Insert: {
                    if (status == Enums::RPCStatus::Ok) {
                        ++ctx->suc_insert;
                        ctx->cache.insert(key, poly, size);
                    }
                    ++ctx->num_insert;
                    break;
                }

                case Enums::RPCOperations::Search: {
                    if (status == Enums::RPCStatus::Ok) {
                        ++ctx->suc_search;
                        ctx->cache.insert(key, poly, size);
                    }
#ifdef __HILL_FETCH_VALUE__
                    // value is embeded
                    if (size < 64) {
                        ++ctx->num_search;
                        ++ctx->RTTs[1];
                        break;
                    }

                    node_id = poly.remote_ptr().get_node();
                    ctx->client->read_from(ctx->thread_id, node_id, poly.get_as<byte_ptr_t>(), size);
                    ctx->client->poll_completion_once(ctx->thread_id, node_id);
                    ++ctx->RTTs[2];
#endif
                    ++ctx->num_search;
                    break;
                }

                case Enums::RPCOperations::Update: {
                    if (status == Enums::RPCStatus::Ok) {
                        ++ctx->suc_update;
                        ctx->cache.expire(key);
                    }
                    ++ctx->num_update;
                    break;
                }

                case Enums::RPCOperations::Range: {
                    if (status == Enums::RPCStatus::Ok) {
                        ++ctx->suc_range;
                    }
                    ++ctx->num_range;
                    break;
                }

                default:
                    break;
                }
                ctx->is_done = true;
#ifdef __HILL_SAMPLE__
            }
#endif
        }
    }
}
