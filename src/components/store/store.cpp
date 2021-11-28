#include "store.hpp"

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

            std::mutex tid_lock;
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
                            case Enums::RPCOperations::Insert:
                                msg->output.status.store(olfit.insert(tid, msg->input.key, msg->input.key_size,
                                                                      msg->input.value, msg->input.value_size));
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
                            case Enums::RPCOperations::Update:
                                [[fallthrough]];
                                // TODO
                            case Enums::RPCOperations::Range:
                                [[fallthrough]];
                                // TODO
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
                        auto tmp = erpc_sessions[(erpc_session_cursor++) % erpc_sessions.size()];
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
                s_ctx.server = this->server.get();
                s_ctx.queues = this->req_queues;
                s_ctx.num_launched_threads = this->num_launched_threads;
#ifdef __HILL_INFO__
                std::cout << ">> Creating eRPC for thread " << tid << ", remote id is also " << tid << "\n";
#endif
                s_ctx.rpc = new erpc::Rpc<erpc::CTransport>(this->nexus, reinterpret_cast<void *>(&s_ctx),
                                                            tid, RPCWrapper::ghost_sm_handler);
                session_lock.lock();
                this->erpc_sessions.push_back(tid);
                session_lock.unlock();
                s_ctx.rpc->run_event_loop(10000000);
                this->server->unregister_thread(tid);
            }, tid);
        }

        auto StoreServer::insert_handler(erpc::ReqHandle *req_handle, void *context) -> void {
            auto ctx = reinterpret_cast<ServerContext *>(context);
            auto [type, key, value] = parse_request_message(req_handle, context);

            IncomeMessage msg;
            msg.input.key = key->raw_chars();
            msg.input.key_size = key->size();
            msg.input.op = type;
            msg.output.status = Indexing::Enums::OpStatus::Unkown;

            auto pos = CityHash64(msg.input.key, msg.input.key_size) % ctx->num_launched_threads;
            while(!ctx->queues[pos].push(&msg));

            while(msg.output.status.load() == Indexing::Enums::OpStatus::Unkown);

            auto& resp = req_handle->pre_resp_msgbuf;
            constexpr auto total_msg_size = sizeof(Enums::RPCOperations) + sizeof(Enums::RPCStatus);
            ctx->rpc->resize_msg_buffer(&resp, total_msg_size);

            *reinterpret_cast<Enums::RPCOperations *>(resp.buf) = Enums::RPCOperations::Insert;
            auto offset = sizeof(Enums::RPCOperations);
            switch(msg.output.status.load()){
            case Indexing::Enums::OpStatus::Ok:
                *reinterpret_cast<Enums::RPCStatus *>(resp.buf + offset) = Enums::RPCStatus::Ok;
                break;
            case Indexing::Enums::OpStatus::NoMemory:
                *reinterpret_cast<Enums::RPCStatus *>(resp.buf + offset) = Enums::RPCStatus::NoMemory;
                break;
            default:
                *reinterpret_cast<Enums::RPCStatus *>(resp.buf + offset) = Enums::RPCStatus::Failed;
                break;
            }

            ctx->rpc->enqueue_response(req_handle, &resp);
        }

        auto StoreServer::update_handler(erpc::ReqHandle *req_handle, void *context) -> void {
            UNUSED(req_handle);
            UNUSED(context);
            // TODO
        }

        auto StoreServer::search_handler(erpc::ReqHandle *req_handle, void *context) -> void {
            auto ctx = reinterpret_cast<ServerContext *>(context);
            auto [type, key, value] = parse_request_message(req_handle, context);
            IncomeMessage msg;
            msg.input.key = key->raw_chars();
            msg.input.key_size = key->size();
            msg.input.op = type;
            msg.output.status = Indexing::Enums::OpStatus::Unkown;

            auto pos = CityHash64(msg.input.key, msg.input.key_size) % ctx->num_launched_threads;
            while(!ctx->queues[pos].push(&msg));

            while(msg.output.status.load() == Indexing::Enums::OpStatus::Unkown);

            auto& resp = req_handle->pre_resp_msgbuf;
            constexpr auto total_msg_size = sizeof(Enums::RPCOperations) + sizeof(Memory::PolymorphicPointer)
                + sizeof(size_t) + sizeof(Enums::RPCStatus);

            ctx->rpc->resize_msg_buffer(&resp, total_msg_size);
            *reinterpret_cast<Enums::RPCOperations *>(resp.buf) = Enums::RPCOperations::Search;

            auto offset = sizeof(Enums::RPCOperations);
            if (msg.output.value == nullptr) {
                *reinterpret_cast<Enums::RPCStatus *>(resp.buf + offset) = Enums::RPCStatus::Failed;
            } else {
                *reinterpret_cast<Enums::RPCStatus *>(resp.buf + offset) = Enums::RPCStatus::Ok;
                // offset += sizeof(Enums::RPCStatus);
                // *reinterpret_cast<size_t *>(resp.buf + offset) = msg.output.value_size;
                // offset += sizeof(size_t);
                // *reinterpret_cast<Memory::PolymorphicPointer *>(resp.buf + offset) = msg.output.value;
            }
            ctx->rpc->enqueue_response(req_handle, &resp);
        }

        auto StoreServer::range_handler(erpc::ReqHandle *req_handle, void *context) -> void {
            UNUSED(req_handle);
            UNUSED(context);
            // TODO
        }

        auto StoreServer::memory_handler(erpc::ReqHandle *req_handle, void *context) -> void {
            UNUSED(req_handle);
            UNUSED(context);
            // TODO
        }

        auto StoreServer::parse_request_message(const erpc::ReqHandle *req_handle, const void *context)
            -> std::tuple<Enums::RPCOperations, KVPair::HillString *, KVPair::HillString *>
        {
            UNUSED(context);
            auto requests = req_handle->get_req_msgbuf();

            auto buf = requests->buf;
            auto type = *reinterpret_cast<Enums::RPCOperations *>(buf);
            KVPair::HillString *key = nullptr, *key_or_value = nullptr;
            buf += sizeof(Enums::RPCOperations);

            switch(type){
            case Enums::RPCOperations::Insert:
                [[fallthrough]];
            case Enums::RPCOperations::Range:
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

                stats.throughputs.timing_now();
                start = std::chrono::steady_clock::now();
                for (auto &i : load) {
                    c_ctx.is_done = false;
                    _node_id = check_rpc_connection(tid, i, c_ctx);
                    if (!_node_id.has_value()) {
                        continue;
                    }

                    node_id = _node_id.value();
                    prepare_request(node_id, i, c_ctx);
                    c_ctx.rpcs[node_id]->enqueue_request(c_ctx.session, i.type,
                                                         &c_ctx.req_bufs[node_id], &c_ctx.resp_bufs[node_id],
                                                         response_continuation, &node_id);
                    while(!c_ctx.is_done) {
                        c_ctx.rpcs[node_id]->run_event_loop_once();
                    }

                    if ((++counter) % 1000 == 0) {
                        end = std::chrono::steady_clock::now();
                        switch (load[0].type) {
                        case Workload::Enums::WorkloadType::Insert: {
                            auto t = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
                            stats.latencies.record_insert(t / 1000);
                        }
                            break;
                        case Workload::Enums::WorkloadType::Search: {
                            auto t = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
                            stats.latencies.record_search(t / 1000);
                        }
                            break;
                        case Workload::Enums::WorkloadType::Update:
                        case Workload::Enums::WorkloadType::Range:
                        default:
                            break;
                        }
                        start = std::chrono::steady_clock::now();
                    }
                }
                stats.throughputs.timing_stop();
                switch (load[0].type) {
                case Workload::Enums::WorkloadType::Insert:
                    stats.throughputs.num_insert = load.size();
                    stats.throughputs.suc_insert = c_ctx.successful_inserts;
                    break;
                case Workload::Enums::WorkloadType::Search:
                    stats.throughputs.num_search = load.size();
                    stats.throughputs.suc_search = c_ctx.successful_searches;
                    break;
                case Workload::Enums::WorkloadType::Update:
                case Workload::Enums::WorkloadType::Range:
                default:
                    break;
                }
                this->client->unregister_thread(tid);
            }, tid.value());
        }

        auto StoreClient::check_rpc_connection(int tid, const Workload::WorkloadItem &item,
                                               ClientContext &c_ctx) -> std::optional<int>
        {
            const auto &meta = this->client->get_cluster_meta();
            auto node_id = meta.filter_node(item.key);

            if (c_ctx.rpcs[node_id] != nullptr) {
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
            c_ctx.rpcs[node_id] = new erpc::Rpc<erpc::CTransport>(nexus, reinterpret_cast<void *>(&c_ctx),
                                                                  tid, RPCWrapper::ghost_sm_handler);
            auto &node = meta.cluster.nodes[node_id];
            auto server_uri = node.addr.to_string() + ":" + std::to_string(node.erpc_port);
            auto rpc = c_ctx.rpcs[node_id];
            c_ctx.session = rpc->create_session(server_uri, remote_id);
            while (!rpc->is_connected(c_ctx.session)) {
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
            switch(type) {
            case Hill::Workload::Enums::WorkloadType::Update:

                *reinterpret_cast<Enums::RPCOperations *>(buf) = Enums::RPCOperations::Update;
                [[fallthrough]];
            case Hill::Workload::Enums::WorkloadType::Range:
                *reinterpret_cast<Enums::RPCOperations *>(buf) = Enums::RPCOperations::Range;
                [[fallthrough]];
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
            default:
                return false;
            }
            return true;
        }

        auto StoreClient::response_continuation(void *context, void *tag) -> void {
            auto node_id = *reinterpret_cast<int *>(tag);
            auto ctx = reinterpret_cast<ClientContext *>(context);
            auto buf = ctx->resp_bufs[node_id].buf;

            auto op = *reinterpret_cast<Enums::RPCOperations *>(buf);
            buf += sizeof(Enums::RPCOperations);
            auto status = *reinterpret_cast<Enums::RPCStatus *>(buf);

            switch(op) {
            case Enums::RPCOperations::Insert: {
                if (status == Enums::RPCStatus::Ok) {
                    ++ctx->successful_inserts;
                }
                break;
            }

            case Enums::RPCOperations::Search: {
                if (status == Enums::RPCStatus::Ok) {
                    ++ctx->successful_searches;
                }
                break;
            }

            case Enums::RPCOperations::Update: {
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
