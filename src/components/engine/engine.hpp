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

            ret->sock = Misc::make_socket(true, 2333);
            if (ret->sock == -1) {
                return nullptr;
            }
            auto flags = fcntl(ret->sock, F_GETFL);
            fcntl(ret->sock, F_SETFL, flags | O_NONBLOCK);

            ret->base = base;
            return ret;
        }

        /*
         * Here I do not record established connections for simplicity. My code ensure every two peers connect only once
         * by triggering check_rdma_request. A client may connect a server for multiple times, but in our experiment, 
         * this is impossible.
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

        auto parse_ib(const std::string &config) noexcept -> bool ;
    };

    class Client {
    public:
        auto connect_monitor() -> void;
        auto connect_server() -> void; 
    private:
        Cluster::ClusterMeta meta;
    };
}
#endif
