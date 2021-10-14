#include "store.hpp"
namespace Hill {
    namespace Store {
        auto StoreServer::register_thread() noexcept -> std::optional<std::thread> {
            if (!is_launched) {
                return {};
            }
            auto _a_tid = server->get_allocator()->register_thread();
            if (!_a_tid.has_value()) {
                return {};
            }

            auto _l_tid = server->get_logger()->register_thread();
            if (!_l_tid.has_value()) {
                return {};
            }

            auto a_tid = _a_tid.value();
            auto l_tid = _l_tid.value();
            if (a_tid != l_tid) {
                return {};
            }

            return std::thread([&] (int tid) {
                detail::ServerContext s_ctx;
                s_ctx.thread_id = tid;
                s_ctx.server = server.get();
                s_ctx.index = index.get();
                s_ctx.rpc = new erpc::Rpc<erpc::CTransport>(this->nexus, reinterpret_cast<void *>(&s_ctx),
                                                            tid, RPCWrapper::ghost_sm_handler);
                s_ctx.rpc->run_event_loop(10000000);
            }, a_tid);
        }

        auto StoreServer::insert_handler(erpc::ReqHandle *req_handle, void *context) -> void {
            auto ctx = reinterpret_cast<detail::ServerContext *>(context);
            auto [type, key, value] = parse_request_message(req_handle, context);
            auto status = ctx->index->insert(ctx->thread_id, key->raw_chars(), key->size(),
                                             value->raw_chars(), value->size());
            
            auto& resp = req_handle->pre_resp_msgbuf_;
            ctx->rpc->resize_msg_buffer(&resp, sizeof(detail::Enums::RPCStatus));

            switch(status){
            case Indexing::Enums::OpStatus::Ok:
                *reinterpret_cast<detail::Enums::RPCStatus *>(resp.buf_) = detail::Enums::RPCStatus::Ok;
                break;
            case Indexing::Enums::OpStatus::NoMemory:
                *reinterpret_cast<detail::Enums::RPCStatus *>(resp.buf_) = detail::Enums::RPCStatus::NoMemory;
                break;
            default:
                *reinterpret_cast<detail::Enums::RPCStatus *>(resp.buf_) = detail::Enums::RPCStatus::Failed;
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
            
            auto& resp = req_handle->pre_resp_msgbuf_;
            ctx->rpc->resize_msg_buffer(&resp, sizeof(Memory::PolymorphicPointer) + sizeof(size_t));
            if (v == nullptr) {
                *reinterpret_cast<size_t *>(resp.buf_) = 0;
            } else {
                *reinterpret_cast<size_t *>(resp.buf_) = v_sz;
                *reinterpret_cast<Memory::PolymorphicPointer *>(resp.buf_ + 8) = v;
            }
            ctx->rpc->enqueue_response(req_handle, &resp);
        }
        
        auto StoreServer::range_handler(erpc::ReqHandle *req_handle, void *context) -> void {
            // TODO
        }
        
        auto StoreServer::memory_handler(erpc::ReqHandle *req_handle, void *context) -> void {
            // TODO
        }

        auto StoreServer::parse_request_message(const erpc::ReqHandle *req_handle, const void *context) ->
            std::tuple<detail::Enums::RPCOperations, KVPair::HillString *, KVPair::HillString *>
        {
            auto server_ctx = reinterpret_cast<detail::ServerContext *>(const_cast<void *>(context));
            auto requests = req_handle->get_req_msgbuf();
            
            auto buf = requests->buf_;
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

        auto StoreClient::register_thread() noexcept -> std::optional<std::thread> {
            int tid;
            for (tid = 0; tid < Memory::Constants::iTHREAD_LIST_NUM; tid++) {
                if (thread_ids[tid] == false) {
                    break;
                }
            }

            return std::thread([&](int tid) {
                // TODO
            }, tid);
        }
    }
}

