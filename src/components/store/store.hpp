#ifndef __HILL__STORE__STORE__
#define __HILL__STORE__STORE__
#include "indexing/indexing.hpp"
#include "remote_memory/remote_memory.hpp"
#include "memory_manager/memory_manager.hpp"
#include "read_cache/read_cache.hpp"
#include "engine/engine.hpp"
#include "rpc_wrapper/rpc_wrapper.hpp"
#include "kv_pair/kv_pair.hpp"
#include "workload/workload.hpp"
#include "config/config.hpp"
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
                std::string server_uri[Cluster::Constants::uMAX_NODE];
                Client *client;
                erpc::Rpc<erpc::CTransport> *rpcs[Cluster::Constants::uMAX_NODE];
                erpc::MsgBuffer req_bufs[Cluster::Constants::uMAX_NODE];
                erpc::MsgBuffer resp_bufs[Cluster::Constants::uMAX_NODE];
                int session;

                std::atomic_long successful_inserts;
                std::atomic_long successful_searches;

                ClientContext() {
                    thread_id = 0;
                    for (auto &u : server_uri) {
                        u = "";
                    }

                    for (auto &r : rpcs) {
                        r = nullptr;
                    }
                }
            };

            namespace Constants {
                static constexpr size_t uMAX_MSG_SIZE = 512;
            }
            
            namespace Enums {
                // no enum class
                enum RPCOperations : uint8_t {
                    // for client
                    Insert = Workload::Enums::WorkloadType::Insert,
                    Search = Workload::Enums::WorkloadType::Search,
                    Update = Workload::Enums::WorkloadType::Update,
                    Range = Workload::Enums::WorkloadType::Range,

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
         * StoreServer handles all erpc calls
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
         *
         * responses are in one of following formats
         * 1. Insert:
         *    |       first byte      |  following bytes
         *    | RPCOperations::Insert |    RPCStatus   |
         *
         * 2. Search:
         *    |       first byte      |  following bytes
         *    | RPCOperations::Search |    RPCStatus   | size_t size |  PolymorphicPointer
         *
         * 3. Update:
         *    |       first byte      |  following bytes
         *    | RPCOperations::Update |    RPCStatus   |
         *
         * 4. Scan
         *    |      first byte     | 
         *    | RPCOperations::Scan | 
         *
         * 5. CallForMemory
         *    |           first byte         | 
         *    | RPCOperations::CallForMemory | 
         *
         */
        class StoreServer {
        public:
            StoreServer() = default;
            ~StoreServer() = default;
            StoreServer(const StoreServer &) = delete;
            StoreServer(StoreServer &&) = delete;
            auto operator=(const StoreServer &) -> StoreServer & = delete;
            auto operator=(StoreServer &&) -> StoreServer & = delete;

            static auto make_server(const byte_ptr_t &base, const std::string &config, size_t cache_cap)
                -> std::unique_ptr<StoreServer>
            {
                auto ret = std::make_unique<StoreServer>();
                ret->server = Engine::make_engine(base, config);
                ret->index = Indexing::OLFIT::make_olfit(ret->server->get_allocator(), ret->server->get_logger());
                ret->cache = &ReadCache::Cache::make_cache(new byte_t[cache_cap]);
                ret->nexus = new erpc::Nexus(ret->server->get_uri(), 0, 0);
                ret->nexus->register_req_func(detail::Enums::RPCOperations::Insert, insert_handler);
                ret->nexus->register_req_func(detail::Enums::RPCOperations::Search, search_handler);
                ret->nexus->register_req_func(detail::Enums::RPCOperations::Update, update_handler);
                ret->nexus->register_req_func(detail::Enums::RPCOperations::Range, range_handler);
                ret->nexus->register_req_func(detail::Enums::RPCOperations::CallForMemory, memory_handler);
                
                ret->is_launched = false;
                return ret;
            }

            inline auto launch() -> void {
#if defined(__HILL_DEBUG__) || defined(__HILL_INFO__)
                std::cout << ">> Launching server node at " << server->get_uri() << "\n";
#endif
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

            static auto make_client(const std::string &config) -> std::unique_ptr<StoreClient> {
                auto ret = std::make_unique<StoreClient>();
                ret->client = Client::make_client(config);
                ret->nexus = new erpc::Nexus(ret->client->get_uri(), 0, 0);

                ret->is_launched = false;
                return ret;
            }

            inline auto launch() -> bool {
#if defined(__HILL_DEBUG__) || defined(__HILL_INFO__)
                std::cout << ">> Launching server node at " << client->get_uri() << "\n";
#endif
                if (client->connect_monitor()) {
                    is_launched = true;
                }
                return is_launched;
            }
            
            auto register_thread(const Workload::StringWorkload &load) noexcept -> std::optional<std::thread>;
        private:
            std::unique_ptr<Client> client;
            erpc::Nexus *nexus;
            bool is_launched;

            auto check_rpc_connection(int tid, const Workload::WorkloadItem &item, detail::ClientContext &c_ctx) -> std::optional<int>;
            auto prepare_request(int node_id, const Workload::WorkloadItem &item, detail::ClientContext &c_ctx) -> bool;
            static auto response_continuation(void *context, void *tag) -> void;            
        };
    }
}
#endif
