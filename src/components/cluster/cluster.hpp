#ifndef __HILL__CLUSTER__CLUSTER__
#define __HILL__CLUSTER__CLUSTER__
#include "memory_manager/memory_manager.hpp"

#include <sstream>
#include <memory>

namespace Hill {
    namespace Cluster {
        namespace Constants {
            // including the monitor
            static constexpr size_t uMAX_NODE = 64;
        }

        // just for simplicity, I don't wnat those Linux stuff
        struct IPV4Addr {
            uint8_t content[4];

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
            int node_id;
            size_t total_pm;
            size_t available_pm;
            float cpu_usage;
            IPV4Addr addr;
            bool is_active;
        } __attribute__((packed));

        // serialization required to send over network
        // ranges never overlap
        // nodes[0] is the main server for this range
        struct RangeInfo {
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
        struct __attribute__((packed)) RangeGroup {
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
        };

        struct ClusterMeta {
            struct {
                size_t node_num;
                // cope with remote pointer, 64 at most
                NodeInfo nodes[Constants::uMAX_NODE];
            } cluster;
            RangeGroup group;

            auto serialize() const noexcept -> std::unique_ptr<uint8_t[]>;
            // update current ClusterMeta with this serialized buf
            auto deserialize(const uint8_t *buf) -> void;

            auto dump() const noexcept -> void;
        } __attribute__((packed));

        class Node {
        public:
            Node() = default;
            ~Node() = default;
            Node(const Node &) = default;
            Node(Node &&) = default;
            auto operator=(const Node &) -> Node & = default;
            auto operator=(Node &&) -> Node & = default;

            static auto make_node(int node_id, const IPV4Addr &addr, size_t total_pm) -> std::unique_ptr<Node> {
                auto ret = std::make_unique<Node>();
                ret->node_id = node_id;
                ret->addr = addr;
                ret->total_pm = total_pm;
                ret->available_pm = total_pm;
                ret->cpu_usage = 0;
                return ret;
            }

            /*
             * Activate this node and start connecting monitor listed in configuration file
             */
            auto launch(const std::string &configure_file) noexcept -> bool;

            /*
             * Launch a background thread that periodically send heartbeat to the monitor
             * This thread will also receive an update of current cluster metainfo
             */
            auto keepalive() const noexcept -> void;
        private:
            int node_id;
            size_t total_pm;
            size_t available_pm;
            float cpu_usage;
            IPV4Addr addr;
            ClusterMeta cluster_status;
        };


        class Monitor {
        public:
            Monitor() = default;
            ~Monitor() = default;
            Monitor(const Monitor &) = default;
            Monitor(Monitor &&) = default;
            auto operator=(const Monitor &) -> Monitor & = default;
            auto operator=(Monitor &&) -> Monitor & = default;

            static auto make_monitor(const IPV4Addr &addr, size_t node_num) -> std::unique_ptr<Monitor> {
                auto ret = std::make_unique<Monitor>();
                ret->addr = addr;
                ret->meta.cluster.node_num = node_num;
                for (size_t i = 0; i < Constants::uMAX_NODE; i++) {
                    ret->meta.cluster.nodes[i].node_id = 0;
                    ret->meta.cluster.nodes[i].is_active = false;
                }
                return ret;
            }

            /*
             * Activiate this monitor.
             * Monitor will start listening for income connect request and start monitoring cluster status
             */
            auto launch() noexcept -> bool;

            /*
             * Broadcast current cluster status including node liveness, node pm usage, etc. to all active nodes
             */
            auto broadcast_cluster_meta() noexcept -> bool;

        private:
            ClusterMeta meta;
            IPV4Addr addr;
        };


    }
}
#endif
