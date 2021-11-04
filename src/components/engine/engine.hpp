#ifndef __HILL__ENGINE__ENGINE__
#define __HILL__ENGINE__ENGINE__
#include "wal/wal.hpp"
#include "memory_manager/memory_manager.hpp"
#include "remote_memory/remote_memory.hpp"
#include "cluster/cluster.hpp"
#include "rdma/rdma.hpp"
#include "misc/misc.hpp"

#include <shared_mutex>
#include <atomic>

#include <fcntl.h>
namespace Hill {
    /*
     * This engine manages all RDMA connections, communications with monitor and the whole PM resource on one node
     * I don't use Server as the name because both Monitor and Node are servers, but Engine only represents working
     * Nodes in cluster.
     *
     * PM is divided as follows
     * |----------------------------|
     * |                            |
     * |      Thread-local WAL      |
     * |                            |
     * |----------------------------|
     * |                            |
     * |   Local Memory Allocator   |
     * |                            |
     * |  ------------------------  |
     * |    Remote Memory Agents    |
     * |----------------------------|
     * |                            |
     * |        Data Region         |
     * |                            |
     * |                            |
     * |          ......            |
     * |                            |
     * |                            |
     * |----------------------------|
     *
     * The read cache is placed in DRAM
     */
    using namespace Memory::TypeAliases;
    using namespace RDMAUtil;
    class Engine {
    public:
        Engine() = default;
        ~Engine() = default;
        Engine(const Engine &) = delete;
        Engine(Engine &&) = delete;
        auto operator=(const Engine &) = delete;
        auto operator=(Engine &&) = delete;

        /* pm_cap is the size of PM intended for storing data instead of the whole size of PM because
         * some PM is used by WAL and remote memory agent.
         */
        static auto make_engine(const byte_ptr_t &base, const std::string &config) -> std::unique_ptr<Engine> {
            auto ret = std::make_unique<Engine>();
            auto offset = 0UL;
            ret->node = Cluster::Node::make_node(config);
            if (!ret->parse_ib(config)) {
                return nullptr;
            }
            ret->logger = WAL::Logger::make_unique_logger(base);
            // regions are the data part
            offset += sizeof(WAL::LogRegions);
            ret->allocator = Memory::Allocator::make_allocator(base + offset, ret->node->available_pm);
            offset += sizeof(Memory::Allocator);
            for (int i = 0; i < Memory::Constants::iTHREAD_LIST_NUM; i++) {
                ret->agents[i] = Memory::RemoteMemoryAgent::make_agent(base + offset, &ret->peer_connections[i]);                
            }

            ret->sock = 0;
            ret->base = base;
            ret->run = false;
            return ret;
        }

        /*
         * Launch this engine and establish connectin with the monitor
         */
        auto launch() noexcept -> bool;
        auto stop() noexcept -> void;

        auto register_thread() -> std::optional<int>;
        auto unregister_thread(int tid) -> void;
        /*
         * Established connections are recorded so that one node can find the RDMA connection with a specific node.
         */
        auto check_rdma_request(int tid) noexcept -> int;
        auto connect_server(int tid, int node_id) -> bool;

        inline auto get_node() const noexcept -> const Cluster::Node * {
            return node.get();
        }
        
        inline auto get_logger() noexcept -> WAL::Logger * {
            return logger.get();
        }

        inline auto get_allocator() noexcept -> Memory::Allocator * {
            return allocator;
        }

        inline auto get_rpc_uri() const noexcept -> const std::string & {
            return node->rpc_uri;
        }

        inline auto get_addr_uri() const noexcept -> std::string {
            return node->addr.to_string() + ":" + std::to_string(node->port);
        }
        
        auto dump() const noexcept -> void;

    private:
        std::unique_ptr<Cluster::Node> node;        
        
        // logger has some runtime data, thus is a smart pointer
        std::unique_ptr<WAL::Logger> logger;
        Memory::Allocator *allocator;

        std::string rdma_dev_name;
        int ib_port;
        int gid_idx;
        byte_ptr_t base;
        bool run;

        int sock;
        std::array<RDMA::RDMAPtr, Cluster::Constants::uMAX_NODE> peer_connections[Memory::Constants::iTHREAD_LIST_NUM];
        std::vector<RDMA::RDMAPtr> client_connections[Memory::Constants::iTHREAD_LIST_NUM];
        Memory::RemoteMemoryAgent *agents[Memory::Constants::iTHREAD_LIST_NUM];
        
        auto parse_ib(const std::string &config) noexcept -> bool;
    };

    class Client {
    public:
        Client() = default;
        ~Client() = default;
        Client(const Client &) = delete;
        Client(Client &&) = delete;
        auto operator=(const Client &) = delete;
        auto operator=(Client &&) = delete;
        
        static auto make_client(const std::string config) -> std::unique_ptr<Client> {
            auto _content = Misc::file_as_string(config);
            if (!_content.has_value()) {
                return nullptr;
            }

            auto content = _content.value();

            std::regex rmonitor("monitor:\\s+(\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}):(\\d+)");
            std::regex raddr("addr:\\s*(\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}):(\\d+)");
            std::regex rrpc_uri("rpc_uri:\\s*(\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}:\\d+)");            
            std::smatch vmonitor, vaddr, vrpc_uri;
            if (!std::regex_search(content, vmonitor, rmonitor)) {
                std::cerr << ">> Error: invalid or unspecified monitor address\n";                
                return nullptr;
            }

            if (!std::regex_search(content, vaddr, raddr)) {
                std::cerr << ">> Error: invalid or unspecified IP address\n";
                return nullptr;
            }

            if (!std::regex_search(content, vrpc_uri, rrpc_uri)) {
                std::cerr << ">> Error: invalid or unspecified uri\n";
                return nullptr;
            }            

            auto ret = std::make_unique<Client>();
            ret->monitor_addr = Cluster::IPV4Addr::make_ipv4_addr(vmonitor[1]).value();
            ret->monitor_port = atoi(vmonitor[2].str().c_str());
            ret->addr = Cluster::IPV4Addr::make_ipv4_addr(vaddr[1].str()).value();
            ret->port = atoi(vaddr[2].str().c_str());
            ret->rpc_uri = vrpc_uri[1].str();
            ret->run = false;
            ret->parse_ib(config);
            ret->buf = std::make_unique<byte_t[]>(16 * 1024);
            return ret;
        }
        auto connect_monitor() noexcept -> bool;

        auto register_thread() -> std::optional<int>;
        auto unregister_thread(int tid) -> void;
        auto connect_server(int tid, int node_id) noexcept -> bool;
        inline auto is_connected(int tid, int node_id) const noexcept -> bool {
            return server_connections[tid][node_id] != nullptr;
        }
        auto write_to(int tid, int node_id, const byte_ptr_t &remote_ptr, const byte_ptr_t &msg, size_t msg_len) noexcept -> RDMAUtil::StatusPair;
        auto read_from(int tid, int node_id, const byte_ptr_t &remote_ptr, size_t msg_len) noexcept -> RDMAUtil::StatusPair;
        inline auto rdma_buf_as_char(int tid, int node_id) -> const char * {
            return server_connections[tid][node_id]->get_char_buf();
        }

        inline auto rdma_buf_as_void(int tid, int node_id) -> const void * {
            return server_connections[tid][node_id]->get_buf();
        }
        
        inline auto get_buf() const noexcept -> const std::unique_ptr<byte_t[]> & {
            return buf;
        }
        
        inline auto get_cluster_meta() const noexcept -> const Cluster::ClusterMeta & {
            return meta;
        }

        inline auto get_rpc_uri() const noexcept -> std::string {
            return rpc_uri;
        }

        inline auto get_addr_uri() const noexcept -> std::string {
            return addr.to_string() + ":" + std::to_string(port);
        }

    private:
        bool run;
        
        Cluster::IPV4Addr monitor_addr;
        int monitor_port;
        int monitor_socket;

        Cluster::ClusterMeta meta;
        std::string rdma_dev_name;
        int ib_port;
        int gid_idx;

        Cluster::IPV4Addr addr;
        int port;
        std::string rpc_uri;

        std::array<RDMAUtil::RDMA::RDMAPtr, Cluster::Constants::uMAX_NODE> server_connections[Memory::Constants::iTHREAD_LIST_NUM];
        std::unique_ptr<byte_t[]> buf;

        auto parse_ib(const std::string &config) noexcept -> bool;        
    };
}
#endif
