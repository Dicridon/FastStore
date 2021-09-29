#ifndef __HILL__ENGINE__ENGINE__
#define __HILL__ENGINE__ENGINE__
#include "wal/wal.hpp"
#include "memory_manager/memory_manager.hpp"
#include "remote_memory/remote_memory.hpp"
#include "cluster/cluster.hpp"
#include "rdma/rdma.hpp"
#include "misc/misc.hpp"

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
     * |    Remote Memory Agent     |
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

        /* pm_cap is the size of PM intended for storing data not the whole size of PM because
         * some PM is used by WAL and remote memory agent.
         */
        
        static auto make_engine(const byte_ptr_t &base, const std::string & config) -> std::unique_ptr<Engine> {
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
            ret->agent = Memory::RemoteMemoryAgent::make_agent(base + offset);

            ret->sock = Misc::make_socket(true, ret->node->port);
            if (ret->sock == -1) {
                return nullptr;
            }
            auto flags = fcntl(ret->sock, F_GETFL);
            fcntl(ret->sock, F_SETFL, flags | O_NONBLOCK);

            ret->base = base;
            ret->run = false;
            return ret;
        }

        /*
         * Established connections are recorded so that one node can find the RDMA connection with a specific node.
         */
        auto check_rdma_request() noexcept -> int;
        auto launch() noexcept -> void;
        auto stop() noexcept -> void;

        auto dump() const noexcept -> void;
        
    private:
        // logger has some runtime data, thus is a smart pointer
        std::unique_ptr<WAL::Logger> logger;
        Memory::Allocator *allocator;
        Memory::RemoteMemoryAgent *agent;
        std::unique_ptr<Cluster::Node> node;
        int sock;
        std::string rdma_dev_name;
        int ib_port;
        int gid_idx;
        byte_ptr_t base;
        bool run;
        std::array<RDMA::RDMAPtr, Cluster::Constants::uMAX_NODE> peer_connections;
        std::vector<RDMA::RDMAPtr> client_connections;
        
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
            auto content_ = Misc::file_as_string(config);
            if (!content_.has_value()) {
                return nullptr;
            }

            std::regex rmonitor("monitor:\\s+(\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}):(\\d+)");
            std::smatch vmonitor;
            if (!std::regex_search(content_.value(), vmonitor, rmonitor)) {
                return nullptr;
            }

            auto ret = std::make_unique<Client>();
            ret->monitor_addr = Cluster::IPV4Addr::make_ipv4_addr(vmonitor[1]).value();
            ret->monitor_port = atoi(vmonitor[2].str().c_str());
            ret->run = false;
            ret->parse_ib(config);
            ret->buf = std::make_unique<byte_t[]>(16 * 1024);
            return ret;
        }
        auto connect_monitor() noexcept -> bool;
        auto connect_server(int node_id) noexcept -> bool;
        auto write_to(int node_id, const byte_ptr_t &remote_ptr, const byte_ptr_t &msg, size_t msg_len) noexcept -> RDMAUtil::StatusPair;
        auto read_from(int node_id, const byte_ptr_t &remote_ptr, size_t msg_len) noexcept -> RDMAUtil::StatusPair;
        auto get_buf() const noexcept -> const std::unique_ptr<byte_t[]> &;
    private:
        bool run;
        
        Cluster::IPV4Addr monitor_addr;
        int monitor_port;
        int monitor_socket;
        std::array<RDMAUtil::RDMA::RDMAPtr, Cluster::Constants::uMAX_NODE> server_connections;
        Cluster::ClusterMeta meta;
        std::string rdma_dev_name;
        int ib_port;
        int gid_idx;
        std::unique_ptr<byte_t[]> buf;

        auto parse_ib(const std::string &config) noexcept -> bool;        
    };
}
#endif
