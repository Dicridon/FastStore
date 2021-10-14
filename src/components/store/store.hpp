#ifndef __HILL__STORE__STORE__
#define __HILL__STORE__STORE__
#include "indexing/indexing.hpp"
#include "remote_memory/remote_memory.hpp"
#include "memory_manager/memory_manager.hpp"
#include "read_cache/read_cache.hpp"
#include "engine/engine.hpp"
#include "rpc_wrapper/rpc_wrapper.hpp"
#include "kv_pair/kv_pair.hpp"
// #include "rpc_wrapper/rpc_wrapper.hpp"

/*
 * The complete implementation of Hill is here.
 *
 * Hill store consists of an engine, an index, an eRPC processing unit. 
 * - The engine offers inter-node communication utility
 * - The index manages PM and offers both point query and range query functionality
 * - The eRPC processing unit handles requests from clients
 *
 * In a hill cluster, a monitor runs on a specific machine. All other servers should 
 * launch a Store instance and connect to that monitor. Clients should also connect to
 * this monitor
 */
namespace Hill {
    namespace Store {
        using namespace Memory::TypeAliases;
        using namespace KVPair::TypeAliases;
        using namespace WAL::TypeAliases;
        
        class StoreServer;
        class StoreClient;
        namespace detail {
            struct ServerContext {
                int thread_id;
                Engine *server;
                Indexing::OLFIT *index;
                erpc::Rpc<erpc::CTransport> *rpc;
            };

            struct ClientContext {
                int thread_id;
                Engine *client;
                erpc::Rpc<erpc::CTransport> *rpc;
                erpc::MsgBuffer req_buf;
                erpc::MsgBuffer resp_buf;
            };
            
            namespace Enums {
                // no enum class
                enum RPCOperations : uint8_t {
                    // for client
                    Insert,
                    Search,
                    Update,
                    Range,

                    // for peer server
                    CallForMemory,

                    // guardian
                    Unknown,
                };

                enum RPCStatus : uint8_t {
                    Ok = 0,
                    NoMemory,
                    Failed,
                };
            }            
        }

        /*
         * StoreServer handles all 
         * an income message is in one of following formats
         * 1. Insert:
         *    |       first byte      | following bytes
         *    | RPCOperations::Insert | hill_key_t key| hill_value_t value |
         *
         * 2. Search:
         *    |       first byte      | following bytes
         *    | RPCOperations::Search | hill_key_t key |
         *
         * 3. Update:
         *    |       first byte      | following bytes
         *    | RPCOperations::Update | hill_key_t key | hill_value_t new_value |
         *
         * 4. Scan
         *    |      first byte     | following bytes
         *    | RPCOperations::Scan | hill_key_t start | hill_key_t end |
         *
         * 5. CallForMemory
         *    |           first byte         | 
         *    | RPCOperations::CallForMemory | 
         */
        class StoreServer {
        public:
            StoreServer() = default;
            ~StoreServer() = default;
            StoreServer(const StoreServer &) = delete;
            StoreServer(StoreServer &&) = delete;
            auto operator=(const StoreServer &) -> StoreServer & = delete;
            auto operator=(StoreServer &&) -> StoreServer & = delete;

            static auto make_server(const byte_ptr_t &base, const std::string &config, size_t cache_cap,
                                   const std::string &uri) -> std::unique_ptr<StoreServer> {
                auto ret = std::make_unique<StoreServer>();
                ret->server = Engine::make_engine(base, config);
                ret->index = Indexing::OLFIT::make_olfit(ret->server->get_allocator(), ret->server->get_logger());
                ret->cache = &ReadCache::Cache::make_cache(new byte_t[cache_cap]);
                ret->nexus = new erpc::Nexus(uri, 0, 0);
                ret->nexus->register_req_func(detail::Enums::RPCOperations::Insert, insert_handler);
                ret->nexus->register_req_func(detail::Enums::RPCOperations::Search, search_handler);
                ret->nexus->register_req_func(detail::Enums::RPCOperations::Update, update_handler);
                ret->nexus->register_req_func(detail::Enums::RPCOperations::Range, range_handler);
                ret->nexus->register_req_func(detail::Enums::RPCOperations::CallForMemory, memory_handler);
                
                ret->is_launched = false;
                return ret;
            }

            inline auto launch() -> void {
                server->launch();
                is_launched = true;
            }
            /*
             * If a thread is successfully registered, a background thread would be launched handling
             * income eRPC requests. 
             */
            auto register_thread() noexcept -> std::optional<std::thread>;

        private:
            // server represents all servers that are not a monitor
            std::unique_ptr<Engine> server;
            std::unique_ptr<Indexing::OLFIT> index;
            ReadCache::Cache *cache;
            erpc::Nexus *nexus;
            bool is_launched;

            static auto insert_handler(erpc::ReqHandle *req_handle, void *context) -> void;
            static auto update_handler(erpc::ReqHandle *req_handle, void *context) -> void;        
            static auto search_handler(erpc::ReqHandle *req_handle, void *context) -> void;
            static auto range_handler(erpc::ReqHandle *req_handle, void *context) -> void;
            static auto memory_handler(erpc::ReqHandle *req_handle, void *context) -> void;

            static auto parse_request_message(const erpc::ReqHandle *req_handle, const void *context) ->
                std::tuple<detail::Enums::RPCOperations, KVPair::HillString *, KVPair::HillString *>;
        };

        class StoreClient {
        public:
            StoreClient() = default;
            ~StoreClient() = default;
            StoreClient(const StoreClient &) = delete;
            StoreClient(StoreClient &&) = delete;
            auto operator=(const StoreClient &) -> StoreClient & = delete;
            auto operator=(StoreClient &&) -> StoreClient & = delete;

            static auto make_client(const std::string &uri, const std::string &config) -> std::unique_ptr<StoreClient> {
                auto ret = std::make_unique<StoreClient>();
                ret->client = Client::make_client(config);
                ret->nexus = new erpc::Nexus(uri, 0, 0);

                for (auto &id : ret->thread_ids) {
                    id = false;
                }
                return ret;
            }

            inline auto launch() -> bool {
                return client->connect_monitor();
            }
            
            auto register_thread() noexcept -> std::optional<std::thread>;
        private:
            std::unique_ptr<Client> client;
            erpc::Nexus *nexus;
            std::array<bool, Memory::Constants::iTHREAD_LIST_NUM> thread_ids;
            
            static auto client_continuation(void *context, void *tag) -> void;
        };
    }
}
#endif
