#include "store.hpp"
namespace Hill {
    namespace Store {
        auto StoreServer::launch_one_erpc_listen_thread() -> bool {
            if (!is_launched) {
                return false;
            }
            
            auto sock = Misc::make_socket(true, server->get_node()->erpc_listen_port);
            if (sock == -1) {
#ifdef __HILL__INFO__
                std::cout << ">> Unable to create socket for eRPC listening\n";
#endif
                return false;
            }

            auto flags = fcntl(sock, F_GETFL);
            fcntl(sock, flags | O_NONBLOCK);
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
            if (!tid.has_value()) {
                return {};
            }
            
            return std::thread([&] (int tid) {
                detail::ServerContext s_ctx;
                s_ctx.thread_id = tid;
                s_ctx.server = server.get();
                s_ctx.index = index.get();
#ifdef __HILL_INFO__
                std::cout << ">> Creating eRPC for thread " << tid << "\n";
#endif
                s_ctx.rpc = new erpc::Rpc<erpc::CTransport>(this->nexus, reinterpret_cast<void *>(&s_ctx),
                                                            tid, RPCWrapper::ghost_sm_handler);
                session_lock.lock();
                erpc_sessions.push_back(tid);
                session_lock.unlock();
                s_ctx.rpc->run_event_loop(10000000);
                this->server->unregister_thread(tid);
            }, tid.value());
        }

        auto StoreServer::insert_handler(erpc::ReqHandle *req_handle, void *context) -> void {
            auto ctx = reinterpret_cast<detail::ServerContext *>(context);
            auto [type, key, value] = parse_request_message(req_handle, context);
            auto status = ctx->index->insert(ctx->thread_id, key->raw_chars(), key->size(),
                                             value->raw_chars(), value->size());
            
            auto& resp = req_handle->pre_resp_msgbuf;
            auto total_msg_size = sizeof(detail::Enums::RPCOperations) + sizeof(detail::Enums::RPCStatus);
            ctx->rpc->resize_msg_buffer(&resp, total_msg_size);

            *reinterpret_cast<detail::Enums::RPCOperations *>(resp.buf) = detail::Enums::RPCOperations::Insert;
            auto offset = sizeof(detail::Enums::RPCOperations);
            switch(status){
            case Indexing::Enums::OpStatus::Ok:
                *reinterpret_cast<detail::Enums::RPCStatus *>(resp.buf + offset) = detail::Enums::RPCStatus::Ok;
                break;
            case Indexing::Enums::OpStatus::NoMemory:
                *reinterpret_cast<detail::Enums::RPCStatus *>(resp.buf + offset) = detail::Enums::RPCStatus::NoMemory;
                break;
            default:
                *reinterpret_cast<detail::Enums::RPCStatus *>(resp.buf + offset) = detail::Enums::RPCStatus::Failed;
                break;
            }

            ctx->rpc->enqueue_response(req_handle, &resp);
        }

        auto StoreServer::update_handler(erpc::ReqHandle *req_handle, void *context) -> void {
            // TODO
        }
        
        auto StoreServer::search_handler(erpc::ReqHandle *req_handle, void *context) -> void {
            auto ctx = reinterpret_cast<detail::ServerContext *>(context);
            auto [type, key, value] = parse_request_message(req_handle, context);
            auto [v, v_sz] = ctx->index->search(key->raw_chars(), key->size());
            
            auto& resp = req_handle->pre_resp_msgbuf;
            auto total_msg_size = sizeof(detail::Enums::RPCOperations) + sizeof(Memory::PolymorphicPointer) + sizeof(size_t);

            ctx->rpc->resize_msg_buffer(&resp, total_msg_size);
            *reinterpret_cast<detail::Enums::RPCOperations *>(resp.buf) = detail::Enums::RPCOperations::Search;
            
            auto offset = sizeof(detail::Enums::RPCOperations);
            if (v == nullptr) {
                *reinterpret_cast<detail::Enums::RPCStatus *>(resp.buf + offset) = detail::Enums::RPCStatus::Failed;
            } else {
                *reinterpret_cast<detail::Enums::RPCStatus *>(resp.buf + offset) = detail::Enums::RPCStatus::Ok;
                offset += sizeof(detail::Enums::RPCStatus);
                *reinterpret_cast<size_t *>(resp.buf + offset) = v_sz;
                offset += sizeof(size_t);
                *reinterpret_cast<Memory::PolymorphicPointer *>(resp.buf + offset) = v;
            }
            ctx->rpc->enqueue_response(req_handle, &resp);
        }
        
        auto StoreServer::range_handler(erpc::ReqHandle *req_handle, void *context) -> void {
            // TODO
        }
        
        auto StoreServer::memory_handler(erpc::ReqHandle *req_handle, void *context) -> void {
            // TODO
        }

        auto StoreServer::parse_request_message(const erpc::ReqHandle *req_handle, const void *context)
            -> std::tuple<detail::Enums::RPCOperations, KVPair::HillString *, KVPair::HillString *>
        {
            auto server_ctx = reinterpret_cast<detail::ServerContext *>(const_cast<void *>(context));
            auto requests = req_handle->get_req_msgbuf();
            
            auto buf = requests->buf;
            auto type = *reinterpret_cast<detail::Enums::RPCOperations *>(buf);
            KVPair::HillString *key = nullptr, *key_or_value = nullptr;
            buf += sizeof(detail::Enums::RPCOperations);
            
            switch(type){
            case detail::Enums::RPCOperations::Insert:
                [[fallthrough]];
            case detail::Enums::RPCOperations::Range:
                [[fallthrough]];
            case detail::Enums::RPCOperations::Update:
                key = reinterpret_cast<hill_key_t *>(buf);
                buf += key->object_size();
                key_or_value = reinterpret_cast<hill_value_t *>(buf);
                break;
            case detail::Enums::RPCOperations::Search:
                key = reinterpret_cast<hill_key_t *>(buf);
                break;
            case detail::Enums::RPCOperations::CallForMemory:
                break;
            default:
                type = detail::Enums::RPCOperations::Unknown;
                break;
            }

            return {type, key, key_or_value};
        }

        auto StoreClient::register_thread(const Workload::StringWorkload &load) noexcept
            -> std::optional<std::thread>
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
                detail::ClientContext c_ctx;
                c_ctx.thread_id = tid;
                c_ctx.client = this->client.get();

                std::optional<int> _node_id;
                int node_id;
                for (auto &i : load) {
                    _node_id = check_rpc_connection(tid, i, c_ctx);
                    if (!_node_id.has_value()) {
                        continue;
                    }

                    node_id = _node_id.value();
                    prepare_request(node_id, i, c_ctx);
                    c_ctx.rpcs[node_id]->enqueue_request(c_ctx.session, i.type,
                                                         &c_ctx.req_bufs[node_id], &c_ctx.resp_bufs[node_id],
                                                         response_continuation, &node_id);

                    c_ctx.rpcs[node_id]->run_event_loop_once();
                }
                this->client->unregister_thread(tid);
#if defined(__HILL_DEBUG__) || defined(__HILL_INFO__)
                std::cout << ">> Job done, reporting stats\n";
                std::cout << ">> Insert: " << c_ctx.successful_inserts << "/" << load.size();
                std::cout << ">> Search: " << c_ctx.successful_searches << "/" << load.size();
#endif                
            }, tid.value());
        }

        auto StoreClient::check_rpc_connection(int tid, const Workload::WorkloadItem &item,
                                               detail::ClientContext &c_ctx) -> std::optional<int>
        {
            const auto &meta = this->client->get_cluster_meta();
            auto node_id = meta.filter_node(item.key);
            if (node_id == 0) {
                return {};
            }

            if (c_ctx.rpcs[node_id] != nullptr) {
                return node_id;
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
            auto remote_id = 0;
            read(socket, &remote_id, sizeof(remote_id));
            c_ctx.rpcs[node_id] = new erpc::Rpc<erpc::CTransport>(nexus, reinterpret_cast<void *>(&c_ctx),
                                                                  tid, RPCWrapper::ghost_sm_handler);
            auto &node = meta.cluster.nodes[node_id];
            auto server_uri = node.rpc_uri;
            auto rpc = c_ctx.rpcs[node_id];
            c_ctx.session = rpc->create_session(server_uri, remote_id);
            if (!rpc->is_connected(c_ctx.session)) {
#ifdef __HILL_INFO__
                std::cerr << "Client can not create session for node " << node_id << "(" << server_uri << ")"
                          << " with remote id " << remote_id << "\n";
#endif                
                return {};
            }
            c_ctx.req_bufs[node_id] = rpc->alloc_msg_buffer_or_die(128);
            c_ctx.resp_bufs[node_id] = rpc->alloc_msg_buffer_or_die(128);

            return node_id;
        }

        auto StoreClient::prepare_request(int node_id, const Workload::WorkloadItem &item,
                                          detail::ClientContext &c_ctx) -> bool
        {
            auto type = item.type;
            uint8_t *buf = c_ctx.req_bufs[node_id].buf;
            switch(type) {
            case Hill::Workload::Enums::WorkloadType::Update:
                *reinterpret_cast<detail::Enums::RPCOperations *>(buf) = detail::Enums::RPCOperations::Update;
                [[fallthrough]];
            case Hill::Workload::Enums::WorkloadType::Range:
                *reinterpret_cast<detail::Enums::RPCOperations *>(buf) = detail::Enums::RPCOperations::Range;
                [[fallthrough]];                
            case Hill::Workload::Enums::WorkloadType::Insert:
                *reinterpret_cast<detail::Enums::RPCOperations *>(buf) = detail::Enums::RPCOperations::Insert;
                buf += sizeof(detail::Enums::RPCOperations);
                KVPair::HillString::make_string(buf, item.key.c_str(), item.key.size());
                buf += reinterpret_cast<hill_key_t *>(buf)->object_size();
                KVPair::HillString::make_string(buf, item.key_or_value.c_str(), item.key_or_value.size());
                break;
            case Hill::Workload::Enums::WorkloadType::Search:
                *reinterpret_cast<detail::Enums::RPCOperations *>(buf) = detail::Enums::RPCOperations::Search;
                buf += sizeof(detail::Enums::RPCOperations);
                KVPair::HillString::make_string(buf, item.key.c_str(), item.key.size());
                break;
            default:
                return false;
            }
            return true;
        }

        auto StoreClient::response_continuation(void *context, void *tag) -> void {
            auto node_id = *reinterpret_cast<int *>(tag);
            auto ctx = reinterpret_cast<detail::ClientContext *>(context);
            auto buf = ctx->resp_bufs[node_id].buf;

            auto op = *reinterpret_cast<detail::Enums::RPCOperations *>(buf);
            buf += sizeof(detail::Enums::RPCOperations);
            auto status = *reinterpret_cast<detail::Enums::RPCStatus *>(buf);
            
            switch(op) {
            case detail::Enums::RPCOperations::Insert: {
                if (status == detail::Enums::RPCStatus::Ok) {
                    ++ctx->successful_inserts;
                }
            }

            case detail::Enums::RPCOperations::Search: {
                if (status == detail::Enums::RPCStatus::Ok) {
                    ++ctx->successful_inserts;
                }
            }
                
            case detail::Enums::RPCOperations::Update: {
                
            }
                
            case detail::Enums::RPCOperations::Range: {
                
            }
                
            default:
                return;
            }
        }
    }
}

