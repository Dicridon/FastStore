#ifndef __HILL__CLUSTER__CLUSTER__
#define __HILL__CLUSTER__CLUSTER__
#include "memory_manager/memory_manager.hpp"

#include <sstream>
#include <memory>

namespace Hill {
    namespace Cluster {
        namespace Constants {
            static constexpr size_t uMAX_NODE = 63;
        }
        // just for simplicity, I don't wnat those Linux stuff
        struct IPV4Addr {
            uint8_t content[4];
            
            auto to_string() -> std::string {
                std::stringstream stream;
                stream << content[0];
                for (int i = 1; i < 4; i++) {
                    stream << "." << content[i];
                }
                return stream.str();
            }
        };
        
        struct __attribute__((packed)) NodeInfo {
            // starting from 1, 0 is reserved for the monitor
            int node_id;
            size_t total_pm;
            size_t available_pm;
            float cpu_usage;
            IPV4Addr addr;
            bool is_active;
        };

        struct __attribute__((packed)) Heartbeat {
            int node_id;
            size_t available_pm;
            float cpu_usage;
        };
        
        struct ClusterMeta {
            size_t node_num;
            // cope with remote pointer, 64 at most
            NodeInfo nodes[Constants::uMAX_NODE];
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
                ret->meta.node_num = node_num;
                for (size_t i = 0; i < Constants::uMAX_NODE; i++) {
                    ret->meta.nodes[i].node_id = 0;
                    ret->meta.nodes[i].is_active = false;
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
    }
}
#endif
