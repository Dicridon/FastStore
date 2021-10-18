#ifndef __HILL__CLUSTER__CLUSTER__
#define __HILL__CLUSTER__CLUSTER__
#include "memory_manager/memory_manager.hpp"
#include "misc/misc.hpp"
#include "config/config.hpp"

#include <sstream>
#include <memory>
#include <regex>
#include <thread>
#include <iostream>
#include <fstream>
#include <shared_mutex>

namespace Hill {
    namespace Cluster {
        using namespace ::Hill::Memory::TypeAliases;
        namespace Constants {
            // including the monitor
            static constexpr size_t uMAX_NODE = 64;
            static constexpr int iCLIENT_ID = 0xff;
        }

        // just for simplicity, I don't wnat those Linux stuff
        struct IPV4Addr {
            uint8_t content[4];

            static auto make_ipv4_addr(const std::string &in) -> std::optional<IPV4Addr> {
                std::regex ip_pattern("(\\d{1,3})\\.(\\d{1,3})\\.(\\d{1,3})\\.(\\d{1,3})");
                std::smatch result;
                if (!std::regex_match(in, result, ip_pattern)) {
                    return {};
                }

                IPV4Addr addr;
                for (int i = 0; i < 4; i++) {
                    addr.content[i] = atoi(result[i + 1].str().c_str());
                }
                return addr;
            }
            
            auto to_string() const -> std::string {
                std::stringstream stream;
                stream << std::to_string(content[0]);
                for (int i = 1; i < 4; i++) {
                    stream << "." << std::to_string(content[i]);
                }
                return stream.str();
            }
        } __attribute__((packed));

        struct Heartbeat {
            int node_id;
            size_t available_pm;
            float cpu_usage;
        } __attribute__((packed));

        struct NodeInfo {
            // starting from 1, 0 is reserved for the monitor
            uint64_t version;
            int node_id;
            size_t total_pm;
            size_t available_pm;
            float cpu_usage;
            IPV4Addr addr;
            int port;
            bool is_active;
        } __attribute__((packed));

        // serialization required to send over network
        // ranges never overlap
        // nodes[0] is the main server for this range
        struct RangeInfo {
            uint64_t version;
            std::string start;
            uint8_t nodes[Constants::uMAX_NODE];
            bool is_mem[Constants::uMAX_NODE];
            RangeInfo() {
                for (auto &n : nodes) {
                    n = 0;
                }

                for (auto &i : is_mem) {
                    i = true;
                }
            }
        };

        /*
         * RangeGroup can be reconstructed, so I place it on DRAM
         */
        struct  RangeGroup {
            size_t num_infos;
            std::unique_ptr<RangeInfo[]> infos;

            RangeGroup() : num_infos(0), infos(nullptr) {};
            ~RangeGroup() = default;
            RangeGroup(const RangeGroup &) = delete;
            RangeGroup(RangeGroup &&) = delete;
            auto operator=(const RangeGroup &) -> RangeGroup & = default;
            auto operator=(RangeGroup &&) -> RangeGroup & = default;

            static auto make_range_group() -> std::unique_ptr<RangeGroup> {
                return std::make_unique<RangeGroup>();
            }

            auto add_main(const std::string &s, int node_id) noexcept -> void;
            auto append_node(const std::string &s, int node_id, bool is_mem) noexcept -> void;
            auto append_cpu(const std::string &s, int node_id) noexcept -> void;
            auto append_mem(const std::string &s, int node_id) noexcept -> void;
        } __attribute__((packed));

        struct ClusterMeta {
            uint64_t version;
            struct {
                size_t node_num;
                // cope with remote pointer, 64 at most
                NodeInfo nodes[Constants::uMAX_NODE];
            } cluster;
            RangeGroup group;

            ClusterMeta() : version(0) {
                cluster.node_num = 0;
                for (size_t i = 0; i < Constants::uMAX_NODE; i++) {
                    cluster.nodes[i].node_id = 0;
                }
            }
            ~ClusterMeta() = default;

            // this is not serialized
            mutable std::shared_mutex lock;

            auto total_size() const noexcept -> size_t;
            auto serialize() const noexcept -> std::unique_ptr<byte_t[]>;
            // auto serialize() const noexcept -> byte_ptr_t;
            // update current ClusterMeta with this serialized buf
            auto deserialize(const byte_t *buf) -> void;
            auto update(const ClusterMeta &newer) -> void;
            auto dump() const noexcept -> void;

            inline auto atomic_read_begin() const noexcept -> const ClusterMeta & {
                lock.lock_shared();
                return *this;
            }
            
            inline auto atomic_read_end() const noexcept -> void {
                lock.unlock_shared();
            }

            // TO BE REFINED
            auto filter_node_no_lock(const std::string &key) const noexcept -> int {
                for (size_t i = 0; i < group.num_infos; i++) {
                    if (group.infos[i].start > key) {
                        atomic_read_end();
                        return i;
                    }
                }
                return 0;
            }
            
            auto filter_node(const std::string &key) const noexcept -> int {
                atomic_read_begin();
                filter_node_no_lock(key);
                atomic_read_end();
                return 0;
            }
        } __attribute__((packed));


        /*
         * struct Node
         * This struct is a high-level abstraction of a node in a cluster. Its only responsibility is to maintain heartbeat 
         * and inform monitor of its latest resource information. To track the change of a resource, Hill::Engine will do.
         *
         * Node is not a class because all its information is used exposed to any other classes for viewing a node's status
         */
        struct Node {
            Node() = default;
            ~Node() = default;
            Node(const Node &) = default;
            Node(Node &&) = default;
            auto operator=(const Node &) -> Node & = default;
            auto operator=(Node &&) -> Node & = default;

            static auto make_node(const std::string &config) -> std::unique_ptr<Node> {
                auto ret = std::make_unique<Node>();
                ret->prepare(config);
                ret->cpu_usage = 0;
                // this version seems useless
                ret->cluster_status.version = 0;
                for (size_t i = 0; i < Constants::uMAX_NODE; i++) {
                    ret->cluster_status.cluster.nodes[i].node_id = 0;
                }

                return ret;
            }

            // read configuration file to initialize
            auto prepare(const std::string &configure_file) -> bool;

            /*
             * Activate this node and start connecting monitor listed in configuration file
             * calling object should outlive the background thread
             */
            auto launch() -> bool;
            auto stop() -> void;

            /*
             * Launch a background thread that periodically send heartbeat to the monitor
             * This thread will also receive an update of current cluster metainfo
             */
            auto keepalive(int socket) noexcept -> bool;

            auto dump() const noexcept -> void;

            int node_id;
            size_t total_pm;
            size_t available_pm;
            float cpu_usage;
            IPV4Addr addr;
            int port;
            std::string rpc_uri;
            IPV4Addr monitor_addr;
            int monitor_port;
            ClusterMeta cluster_status;
            bool run;
        };


        /*
         * class Monitor
         * This class manage is an abstraction of cluster resources. It also manages heartbeat and keep resource
         * metadata update-to-date
         */
        class Monitor {
        public:
            Monitor() = default;
            ~Monitor() = default;
            Monitor(const Monitor &) = default;
            Monitor(Monitor &&) = default;
            auto operator=(const Monitor &) -> Monitor & = default;
            auto operator=(Monitor &&) -> Monitor & = default;

            static auto make_monitor(const std::string &config) -> std::unique_ptr<Monitor> {
                auto ret = std::make_unique<Monitor>();
                ret->prepare(config);
                
                for (size_t i = 0; i < Constants::uMAX_NODE; i++) {
                    ret->meta.cluster.nodes[i].node_id = 0;
                    ret->meta.cluster.nodes[i].version = 0;
                    ret->meta.cluster.nodes[i].is_active = false;

                    for (size_t i = 0; i < ret->meta.group.num_infos; i++) {
                        // Monitor holds the latest range group info on start
                        ret->meta.group.infos[i].version = 1;
                    }
                }
                return ret;
            }

            auto prepare(const std::string &configure) -> bool;
            /*
             * Activiate this monitor.
             * Monitor will start listening for income connect request and start monitoring cluster status
             */
            auto launch() -> bool;
            auto stop() -> void;
            auto check_income_connection(int sock) -> void;

            /*
             * Broadcast current cluster status including node liveness, node pm usage, etc. to all active nodes
             */
            auto return_cluster_meta(int socket) noexcept -> void;

            auto dump() const noexcept -> void;

        private:
            ClusterMeta meta;
            IPV4Addr addr;
            int port;
            bool run;
        };
    }
}
#endif
