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
                                                                        msg->input.value, msg->input.value_size);
                                msg->output.value = value_ptr;
                                msg->output.status.store(status);

                                server->get_node()->available_pm = server->get_node()->total_pm - server->get_allocator()->get_consumed();
                            }
                                break;
                            case Enums::RPCOperations::Search: {
                                auto [v, v_sz] = olfit.search(msg->input.key, msg->input.key_size);
                                if (v == nullptr) {
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

                s_ctx.handle_sampler = new HandleSampler(1000);
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
                s_ctx.rpc->run_event_loop(10000000);
                this->server->unregister_thread(tid);
            }, tid);
        }

        auto StoreServer::use_agent() noexcept -> void {

        }

        auto StoreServer::check_available_mem(ServerContext &s_ctx, int tid) -> Memory::RemotePointer {
            size_t max = 0;
            size_t node_id;
            const auto &meta = s_ctx.server->get_node()->cluster_status;
            // skip monitor
            for (size_t i = 1; i <= meta.cluster.node_num; i++) {
                if (max < meta.cluster.nodes[i].available_pm) {
                    max = meta.cluster.nodes[i].available_pm;
                    node_id = i;
                }
            }

            if (s_ctx.erpc_sessions[node_id] == -1) {
                if (!s_ctx.server->connect_server(tid, node_id)) {
                    std::cerr << ">> Error: can't connect remote server " << node_id << " for more memory\n";
                    return nullptr;
                }

                if (!establish_erpc(s_ctx, tid, node_id)) {
                    std::cerr << ">> Error: can't connect remote server " << node_id << "'s rpc\n";
                    return nullptr;
                }
            }

            auto buf = s_ctx.req_bufs[node_id].buf;
            s_ctx.is_done = false;
            *reinterpret_cast<Enums::RPCOperations *>(buf) = Enums::RPCOperations::CallForMemory;
            s_ctx.rpc->enqueue_request(s_ctx.erpc_sessions[node_id], Enums::RPCOperations::CallForMemory,
                                       &s_ctx.req_bufs[node_id], &s_ctx.resp_bufs[node_id],
                                       response_continuation, &node_id);
            while (!s_ctx.is_done) {
                s_ctx.rpc->run_event_loop_once();
            }

            auto pbuf = s_ctx.resp_bufs[node_id].buf;
            auto offset = sizeof(Enums::RPCOperations) + sizeof(Enums::RPCStatus);
            auto ptr = *reinterpret_cast<Memory::RemotePointer *>(pbuf + offset);
            if (ptr.is_nullptr()) {
                throw std::runtime_error("Remote memory is also depleted");
            }

            return ptr;
        }

        auto StoreServer::establish_erpc(ServerContext &s_ctx, int tid, int node_id) -> bool {
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

            auto &node = meta.cluster.nodes[node_id];
            auto server_uri = node.addr.to_string() + ":" + std::to_string(node.erpc_port);
            auto rpc = s_ctx.rpc;
            s_ctx.erpc_sessions[node_id] = rpc->create_session(server_uri, remote_id);
            while (!rpc->is_connected(s_ctx.erpc_sessions[node_id])) {
                rpc->run_event_loop_once();
            }

            s_ctx.req_bufs[node_id] = rpc->alloc_msg_buffer_or_die(64);
            s_ctx.resp_bufs[node_id] = rpc->alloc_msg_buffer_or_die(64);
            shutdown(socket, 0);
            return true;
        }

        auto StoreServer::response_continuation(void *context, void *tag) -> void {
            auto node_id = *reinterpret_cast<int *>(tag);
            auto ctx = reinterpret_cast<ServerContext *>(context);
            auto buf = ctx->resp_bufs[node_id].buf;

            auto op = *reinterpret_cast<Enums::RPCOperations *>(buf);
            buf += sizeof(Enums::RPCOperations);
            auto status = *reinterpret_cast<Enums::RPCStatus *>(buf);
            buf += sizeof(Enums::RPCStatus);
            auto ptr = *reinterpret_cast<Memory::RemotePointer *>(buf);

            ctx->server->get_agent()->add_region(ctx->thread_id, ptr);
            ctx->is_done = true;
        }

        auto StoreServer::insert_handler(erpc::ReqHandle *req_handle, void *context) -> void {
            auto ctx = reinterpret_cast<ServerContext *>(context);
            auto server = ctx->server;
            auto handle_sampler = ctx->handle_sampler;
            auto &sampler = handle_sampler->insert_sampler;

            Enums::RPCOperations type; KVPair::HillString *key, *value;
            {
                SampleRecorder<uint64_t> _(sampler, handle_sampler->to_sample_type(HandleSampler::PARSE));
                auto r = parse_request_message(req_handle, context);
                type = std::get<0>(r);
                key = std::get<1>(r);
                value = std::get<2>(r);
            }

            IncomeMessage msg;
            msg.input.key = key->raw_chars();
            msg.input.key_size = key->size();
            msg.input.op = type;
        retry:
            msg.output.status = Indexing::Enums::OpStatus::Unkown;
            // this is fast we do not need to sample
            auto pos = CityHash64(msg.input.key, msg.input.key_size) % ctx->num_launched_threads;
            auto allowed = Constants::dNODE_CAPPACITY_LIMIT * server->get_node()->total_pm;
            auto insufficient = server->get_allocator()->get_consumed() >= allowed;

            {
                SampleRecorder<uint64_t> _(sampler, handle_sampler->to_sample_type(HandleSampler::CAP_CHECK));
                if (insufficient && !server->get_agent()->available(ctx->thread_id)) {

                    // a log should be used here, but for simiplicity, I omit it.
#if defined(__HILL_DEBUG__) || defined(__HILL_INFO__)
                    std::cout << ">> Trying to get remote memory...\n";
#endif
                    auto ptr = check_available_mem(*ctx, ctx->thread_id);
                    ctx->self->server->get_agent()->add_region(ctx->thread_id, ptr);
#if defined(__HILL_DEBUG__) || defined(__HILL_INFO__)
                    std::cout << ">> Got memory from node" << ptr.get_node() << "\n";;
#endif
                    msg.input.op = Enums::RPCOperations::CallForMemory;
                    msg.input.agent = ctx->self->server->get_agent();
                    while(!ctx->queues[pos].push(&msg));

                    while(msg.output.status.load() == Indexing::Enums::OpStatus::Unkown);
                    msg.output.status.store(Indexing::Enums::OpStatus::Unkown);
                    msg.input.op = Enums::RPCOperations::Insert;
                }
            }

            {
                SampleRecorder<uint64_t> _(sampler, handle_sampler->to_sample_type(HandleSampler::INDEXING));
                while(!ctx->queues[pos].push(&msg));

                while(msg.output.status.load() == Indexing::Enums::OpStatus::Unkown);
            }

            auto &resp = req_handle->pre_resp_msgbuf;
            constexpr auto total_msg_size = sizeof(Enums::RPCOperations) + sizeof(Enums::RPCStatus) + sizeof(Memory::PolymorphicPointer);
            ctx->rpc->resize_msg_buffer(&resp, total_msg_size);
            {
                SampleRecorder<uint64_t> _(sampler, handle_sampler->to_sample_type(HandleSampler::RESP_MSG));

                *reinterpret_cast<Enums::RPCOperations *>(resp.buf) = Enums::RPCOperations::Insert;
                auto offset = sizeof(Enums::RPCOperations);
                switch(msg.output.status.load()){
                case Indexing::Enums::OpStatus::Ok:
                    *reinterpret_cast<Enums::RPCStatus *>(resp.buf + offset) = Enums::RPCStatus::Ok;
                    break;
                case Indexing::Enums::OpStatus::NoMemory:
                    *reinterpret_cast<Enums::RPCStatus *>(resp.buf + offset) = Enums::RPCStatus::NoMemory;
                    // agent's memory is available but not sufficient;
                    {
                        SampleRecorder<uint64_t> _(sampler, handle_sampler->to_sample_type(HandleSampler::CAP_RECHECK));
                        ctx->self->server->get_agent()->add_region(ctx->thread_id, check_available_mem(*ctx, ctx->thread_id));
                    }
                    goto retry;
                    break;
                default:
                    *reinterpret_cast<Enums::RPCStatus *>(resp.buf + offset) = Enums::RPCStatus::Failed;
                    break;
                }

                offset += sizeof(Enums::RPCStatus);
                *reinterpret_cast<Memory::PolymorphicPointer *>(resp.buf + offset) = msg.output.value;
            }

            {
                SampleRecorder<uint64_t> _(sampler, handle_sampler->to_sample_type(HandleSampler::RESP));
                ctx->rpc->enqueue_response(req_handle, &resp);
            }

            if (msg.output.status.load() == Indexing::Enums::OpStatus::Failed) {
                std::cout << "Inserting " << key->to_string() << " failed\n";
            }
        }

        auto StoreServer::update_handler(erpc::ReqHandle *req_handle, void *context) -> void {
            auto ctx = reinterpret_cast<ServerContext *>(context);
            auto server = ctx->server;
            auto handle_sampler = ctx->handle_sampler;
            auto &sampler = handle_sampler->update_sampler;

            Enums::RPCOperations type; KVPair::HillString *key, *value;
            {
                SampleRecorder<uint64_t> _(sampler, handle_sampler->to_sample_type(HandleSampler::PARSE));
                auto r = parse_request_message(req_handle, context);
                type = std::get<0>(r);
                key = std::get<1>(r);
                value = std::get<2>(r);
            }

            IncomeMessage msg;
            msg.input.key = key->raw_chars();
            msg.input.key_size = key->size();
            msg.input.value = value->raw_chars();
            msg.input.value_size = value->size();
            msg.input.op = type;
        retry:
            msg.output.status = Indexing::Enums::OpStatus::Unkown;
            auto pos = CityHash64(msg.input.key, msg.input.key_size) % ctx->num_launched_threads;
            {
                SampleRecorder<uint64_t> _(sampler, handle_sampler->to_sample_type(HandleSampler::CAP_CHECK));
                if (server->get_allocator()->get_consumed() >= Constants::dNODE_CAPPACITY_LIMIT * server->get_node()->total_pm &&
                    !server->get_agent()->available(ctx->thread_id)) {

                    // a log should be used here, but for simiplicity, I omit it.
                    auto ptr = check_available_mem(*ctx, ctx->thread_id);
                    ctx->self->server->get_agent()->add_region(ctx->thread_id, ptr);
                    msg.input.op = Enums::RPCOperations::CallForMemory;
                    msg.input.agent = ctx->self->server->get_agent();
                    while(!ctx->queues[pos].push(&msg));

                    while(msg.output.status.load() == Indexing::Enums::OpStatus::Unkown);
                    msg.output.status.store(Indexing::Enums::OpStatus::Unkown);
                }
            }

            {
                SampleRecorder<uint64_t> _(sampler, handle_sampler->to_sample_type(HandleSampler::INDEXING));

                while(!ctx->queues[pos].push(&msg));

                while(msg.output.status.load() == Indexing::Enums::OpStatus::Unkown);
            }

            auto &resp = req_handle->pre_resp_msgbuf;
            constexpr auto total_msg_size = sizeof(Enums::RPCOperations) + sizeof(Enums::RPCStatus) + sizeof(Memory::PolymorphicPointer);
            ctx->rpc->resize_msg_buffer(&resp, total_msg_size);
            {
                SampleRecorder<uint64_t> _(sampler, handle_sampler->to_sample_type(HandleSampler::RESP_MSG));

                *reinterpret_cast<Enums::RPCOperations *>(resp.buf) = Enums::RPCOperations::Update;
                auto offset = sizeof(Enums::RPCOperations);

                switch(msg.output.status.load()){
                case Indexing::Enums::OpStatus::Ok:
                    *reinterpret_cast<Enums::RPCStatus *>(resp.buf + offset) = Enums::RPCStatus::Ok;
                    break;
                case Indexing::Enums::OpStatus::NoMemory:
                    *reinterpret_cast<Enums::RPCStatus *>(resp.buf + offset) = Enums::RPCStatus::NoMemory;
                    // agent's memory is available but not sufficient
                    ctx->self->server->get_agent()->add_region(ctx->thread_id, check_available_mem(*ctx, ctx->thread_id));
                    goto retry;
                    break;
                default:
                    *reinterpret_cast<Enums::RPCStatus *>(resp.buf + offset) = Enums::RPCStatus::Failed;
                    break;
                }

                offset += sizeof(Enums::RPCStatus);
                *reinterpret_cast<Memory::PolymorphicPointer *>(resp.buf + offset) = msg.output.value;
            }

            {
                SampleRecorder<uint64_t> _(sampler, handle_sampler->to_sample_type(HandleSampler::RESP));
                ctx->rpc->enqueue_response(req_handle, &resp);
            }

            if (msg.output.status.load() == Indexing::Enums::OpStatus::Failed) {
                std::cout << "Updating " << key->to_string() << " failed\n";
            }
        }

        auto StoreServer::search_handler(erpc::ReqHandle *req_handle, void *context) -> void {
            auto ctx = reinterpret_cast<ServerContext *>(context);
            auto handle_sampler = ctx->handle_sampler;
            auto &sampler = handle_sampler->search_sampler;

            Enums::RPCOperations type; KVPair::HillString *key, *value;
            {
                SampleRecorder<uint64_t> _(sampler, handle_sampler->to_sample_type(HandleSampler::PARSE));
                auto t = parse_request_message(req_handle, context);
                type = std::get<0>(t);
                key = std::get<1>(t);
                value = std::get<2>(t);
            }

            IncomeMessage msg;
            msg.input.key = key->raw_chars();
            msg.input.key_size = key->size();
            msg.input.op = type;
            msg.output.status = Indexing::Enums::OpStatus::Unkown;

            auto pos = CityHash64(msg.input.key, msg.input.key_size) % ctx->num_launched_threads;
            {
                SampleRecorder<uint64_t> _(sampler, handle_sampler->to_sample_type(HandleSampler::INDEXING));

                while(!ctx->queues[pos].push(&msg));

                while(msg.output.status.load() == Indexing::Enums::OpStatus::Unkown);
            }

            auto& resp = req_handle->pre_resp_msgbuf;
            constexpr auto total_msg_size = sizeof(Enums::RPCOperations) + sizeof(Memory::PolymorphicPointer)
                + sizeof(size_t) + sizeof(Enums::RPCStatus);
            
            {
                SampleRecorder<uint64_t> _(sampler, handle_sampler->to_sample_type(HandleSampler::RESP_MSG));
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
                    *reinterpret_cast<size_t *>(resp.buf + offset) = msg.output.value_size;
                }
            }

            {
                SampleRecorder<uint64_t> _(sampler, handle_sampler->to_sample_type(HandleSampler::RESP));
                ctx->rpc->enqueue_response(req_handle, &resp);
            }

            if (msg.output.status.load() == Indexing::Enums::OpStatus::Failed) {
                std::cout << "Searching " << key->to_string() << " failed\n";
            }
        }

        auto StoreServer::range_handler(erpc::ReqHandle *req_handle, void *context) -> void {
            auto ctx = reinterpret_cast<ServerContext *>(context);
            
            auto handle_sampler = ctx->handle_sampler;
            auto &sampler = handle_sampler->scan_sampler;
            Enums::RPCOperations type; KVPair::HillString *key, *value;
            {
                SampleRecorder<uint64_t> _(sampler, handle_sampler->to_sample_type(HandleSampler::PARSE));
                auto t = parse_request_message(req_handle, context);
                type = std::get<0>(t);
                key = std::get<1>(t);
                value = std::get<2>(t);
            }

            
            IncomeMessage msgs[Memory::Constants::iTHREAD_LIST_NUM];
            std::vector<std::vector<Indexing::ScanHolder>> ranges;
            {
                SampleRecorder<uint64_t> _(sampler, handle_sampler->to_sample_type(HandleSampler::INDEXING));
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
            }
            
            // all partitions are collected
            size_t ret;
            {
                SampleRecorder<uint64_t> _(sampler, handle_sampler->to_sample_type(HandleSampler::MERGE));
                auto merger = Merger::make_merger(ranges);
                auto holders = merger->merge(msgs[0].input.value_size);
                ret = holders.size();
            }


            auto& resp = req_handle->pre_resp_msgbuf;
            {
                SampleRecorder<uint64_t> _(sampler, handle_sampler->to_sample_type(HandleSampler::RESP_MSG));
                
                constexpr auto total_msg_size = sizeof(Enums::RPCOperations) + sizeof(Memory::PolymorphicPointer)
                    + sizeof(size_t) + sizeof(Enums::RPCStatus);

                ctx->rpc->resize_msg_buffer(&resp, total_msg_size);
                *reinterpret_cast<Enums::RPCOperations *>(resp.buf) = Enums::RPCOperations::Search;

                auto offset = sizeof(Enums::RPCOperations);
                *reinterpret_cast<Enums::RPCStatus *>(resp.buf + offset) = Enums::RPCStatus::Ok;

                offset += sizeof(Enums::RPCStatus);
                *reinterpret_cast<size_t *>(resp.buf + offset) = ret;
            }

            {
                SampleRecorder<uint64_t> _(sampler, handle_sampler->to_sample_type(HandleSampler::RESP));                
                ctx->rpc->enqueue_response(req_handle, &resp);
            }
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

            *reinterpret_cast<Memory::RemotePointer *>(resp.buf + offset) = Memory::RemotePointer::make_remote_pointer(ctx->node_id, ptr);

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

                stats.throughputs.timing_now();
                start = std::chrono::steady_clock::now();
                for (auto &i : load) {
                    if (i.type == Workload::Enums::Search) {
                        auto ret = c_ctx.cache.get(i.key);
                        if (ret != nullptr) {
#ifdef __HILL_FETCH_VALUE__
                            auto re_ptr = ret->value_ptr.remote_ptr();
                            c_ctx.client->read_from(c_ctx.thread_id, re_ptr.get_node(), re_ptr.get_as<byte_ptr_t>(), ret->value_size);
                            c_ctx.client->poll_completion_once(c_ctx.thread_id, re_ptr.get_node());
#endif
                            ++c_ctx.num_search;
                            ++c_ctx.suc_search;
                            goto sample;
                        }
                    }

                    c_ctx.is_done = false;
                    _node_id = check_rpc_connection(tid, i, c_ctx);
                    if (!_node_id.has_value()) {
                        continue;
                    }

                    node_id = _node_id.value();
                    prepare_request(node_id, i, c_ctx);

                    // cache is updated in the response_continuation
                    c_ctx.rpc->enqueue_request(c_ctx.erpc_sessions[node_id], i.type,
                                               &c_ctx.req_bufs[node_id], &c_ctx.resp_bufs[node_id],
                                               response_continuation, &node_id);
                    while(!c_ctx.is_done) {
                        c_ctx.rpc->run_event_loop_once();
                    }
                sample:
                    if ((++counter) % 1000 == 0) {
                        end = std::chrono::steady_clock::now();
                        double t = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
                        stats.latencies.record(t / 1000);
                        start = std::chrono::steady_clock::now();
                    }
                }
                stats.throughputs.timing_stop();
                stats.throughputs.num_ops = c_ctx.num_insert + c_ctx.num_search + c_ctx.num_update;
                stats.throughputs.suc_ops = c_ctx.suc_insert + c_ctx.suc_search + c_ctx.suc_update;
                stats.cache_hit_ratio = c_ctx.cache.hit_ratio();
                this->client->unregister_thread(tid);

                std::cout << ">> Correctness report:\n";
                std::cout << "-->> insert: " << c_ctx.suc_insert << "/" << c_ctx.num_insert << "\n";
                std::cout << "-->> search: " << c_ctx.suc_search << "/" << c_ctx.num_search << "\n";
                std::cout << "-->> update: " << c_ctx.suc_update << "/" << c_ctx.num_update << "\n";
            }, tid.value());
        }

        auto StoreClient::check_rpc_connection(int tid, const Workload::WorkloadItem &item,
                                               ClientContext &c_ctx) -> std::optional<int>
        {
            const auto &meta = this->client->get_cluster_meta();
            auto node_id = meta.filter_node(item.key);

            if (c_ctx.erpc_sessions[node_id] != -1) {
                return node_id;
            }

            if (node_id == 0) {
                return {};
            }

            if (!client->is_connected(tid, node_id)) {
                if (!client->connect_server(tid, node_id)) {
                    std::cerr << "Client can not connect to server " << node_id << "\n";
                    return {};
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
            return node_id;
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
                ctx->client->read_from(ctx->thread_id, node_id, poly.get_as<byte_ptr_t>(), size);
                ctx->client->poll_completion_once(ctx->thread_id, node_id);
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
                break;
            }

            default:
                break;
            }
            ctx->is_done = true;
        }
    }
}
