#ifndef __HILL__ENGINE__ENGINE__
#define __HILL__ENGINE__ENGINE__
#include "wal/wal.hpp"
#include "memory_manager/memory_manager.hpp"
#include "remote_memory/remote_memory.hpp"
#include "cluster/cluster.hpp"
#include "rdma/rdma.hpp"
#include "misc/misc.hpp"
#include "config_reader/config_reader.hpp"

#include <shared_mutex>
#include <atomic>
#include <cstring>

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

    namespace Constants {
        constexpr size_t uLOCAL_BUF_SIZE = 16 * 1024;
    }
    
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
        static auto make_engine(const std::string &config) -> std::unique_ptr<Engine> {
            auto ret = std::make_unique<Engine>();
            auto offset = 0UL;
            ret->node = Cluster::Node::make_node(config);
            if (!ret->parse_ib(config)) {
                return nullptr;
            }

            if (!ret->parse_pmem(config)) {
                std::cout << ">> Pmem is not specified, using DRAM instead\n";
                ret->base = new byte_t[ret->node->available_pm];
            } else {
                size_t mapped_size;
                ret->base = reinterpret_cast<byte_ptr_t>(pmem_map_file(ret->pmem_file.c_str(),
                                                                       ret->node->available_pm,
                                                                       PMEM_FILE_CREATE, 0666,
                                                                       &mapped_size, nullptr));
                if (ret->base == nullptr) {
                    std::cout << ">> Unable to map pmem file " << ret->pmem_file << "\n";
                    std::cout << ">> Errno is " << errno << ": " << strerror(errno) << "\n";
                    return nullptr;
                } else {
                    std::cout << ">> " << mapped_size / 1024 / 1024 / 1024.0 << "GB pmem is mapped at "
                              << reinterpret_cast<void *>(ret->base) << "\n";
                }
            }

            ret->logger = WAL::Logger::make_unique_logger(ret->base);
            // regions are the data part
            offset += sizeof(WAL::LogRegions);
            ret->agent = Memory::RemoteMemoryAgent::make_agent(ret->base + offset, &ret->peer_connections[0]);
            offset += sizeof(Memory::RemoteMemoryAgent);
            ret->node->available_pm -= offset;
            std::cout << ">> " << ret->node->available_pm / 1024 / 1024 / 1024.0 << "GB pmem is available\n";
            ret->allocator = Memory::Allocator::make_allocator(ret->base + offset, ret->node->available_pm);

            auto [rdma_device, status] = RDMADevice::make_rdma(ret->rdma_dev_name, ret->ib_port, ret->gid_idx);
            if (status != Status::Ok) {
                return nullptr;
            }
            
            ret->rdma_device = std::move(rdma_device);

            for (auto &t : ret->peer_connections) {
                for (auto &c : t) {
                    c = nullptr;
                }
            }
            
            ret->sock = 0;
            ret->run = false;
            ret->tids = 0;
            return ret;
        }

        /*
         * Launch this engine and establish connectin with the monitor
         */
        auto launch() noexcept -> bool;
        auto stop() noexcept -> void;

        auto register_thread() -> int;
        auto unregister_thread(int tid) -> void;
        /*
         * Established connections are recorded so that one node can find the RDMA connection with a specific node.
         */
        auto check_rdma_request(int tid) noexcept -> int;
        auto connect_server(int tid, int node_id) -> bool;
        inline auto server_connected(int tid, int node_id) -> bool {
            return peer_connections[tid][node_id] != nullptr;
        }

        inline auto get_node() noexcept -> Cluster::Node * {
            return node.get();
        }

        inline auto get_logger() noexcept -> WAL::Logger * {
            return logger.get();
        }

        inline auto get_allocator() noexcept -> Memory::Allocator * {
            return allocator;
        }

        inline auto get_agent() noexcept -> Memory::RemoteMemoryAgent * {
            return agent;
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

        std::unique_ptr<RDMADevice> rdma_device;
        std::string rdma_dev_name;
        int ib_port;
        int gid_idx;
        std::string pmem_file;
        byte_ptr_t base;
        bool run;
        std::atomic_int tids;

        int sock;
        // std::array<std::unique_ptr<RDMAContext>, Cluster::Constants::uMAX_NODE> peer_connections[Memory::Constants::iTHREAD_LIST_NUM];
        std::vector<std::unique_ptr<RDMAContext>> peer_connections[Memory::Constants::iTHREAD_LIST_NUM];
        std::vector<std::unique_ptr<RDMAContext>> client_connections[Memory::Constants::iTHREAD_LIST_NUM];
        Memory::RemoteMemoryAgent *agent;

        auto parse_ib(const std::string &config) noexcept -> bool;
        auto parse_pmem(const std::string &config) noexcept -> bool;
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

            auto ret = std::make_unique<Client>();
            ret->monitor_addr = Cluster::IPV4Addr::make_ipv4_addr(ConfigReader::read_monitor_addr(content).value()).value();
            ret->monitor_port = ConfigReader::read_monitor_port(content).value();
            ret->addr = Cluster::IPV4Addr::make_ipv4_addr(ConfigReader::read_ip_addr(content).value()).value();
            ret->port = ConfigReader::read_ip_port(content).value();
            ret->rpc_uri = ConfigReader::read_rpc_uri(content).value();
            ret->run = false;
            ret->parse_ib(config);

            auto [rdma_device, status] = RDMADevice::make_rdma(ret->rdma_dev_name, ret->ib_port, ret->gid_idx);
            if (status != Status::Ok) {
                return nullptr;
            }
            ret->rdma_device = std::move(rdma_device);
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
        auto poll_completion_once(int tid, int node_id) noexcept -> void;
        inline auto rdma_buf_as_char(int tid, int node_id) -> const char * {
            return server_connections[tid][node_id]->get_char_buf();
        }

        inline auto rdma_buf_as_void(int tid, int node_id) -> const void * {
            return server_connections[tid][node_id]->get_buf();
        }

        inline auto get_buf(int tid, int node_id) const noexcept -> const std::unique_ptr<byte_t[]> & {
            return bufs[tid][node_id];
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

        std::unique_ptr<RDMADevice> rdma_device;
        Cluster::ClusterMeta meta;
        std::string rdma_dev_name;
        int ib_port;
        int gid_idx;

        Cluster::IPV4Addr addr;
        int port;
        std::string rpc_uri;

        std::array<std::unique_ptr<RDMAContext>, Cluster::Constants::uMAX_NODE> server_connections[Memory::Constants::iTHREAD_LIST_NUM];
        std::array<std::unique_ptr<byte_t[]>, Cluster::Constants::uMAX_NODE> bufs[Memory::Constants::iTHREAD_LIST_NUM];

        auto parse_ib(const std::string &config) noexcept -> bool;
    };
}
#endif
