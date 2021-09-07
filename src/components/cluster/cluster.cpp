#include "cluster.hpp"

#include <iostream>

namespace Hill {
    namespace Cluster {
        auto RangeGroup::add_main(const std::string &s, int node_id) noexcept -> void {
            if (node_id == 0) {
                std::cerr << ">> Error: node 0 is not supposed to be in range group\n";
                return;
            }

            for (size_t i = 0; i < num_infos; i++) {
                if (infos[i].start == s) {
                    // duplicated, a main server already exists
                    std::cerr << ">> Warning: duplicated main server\n";
                    return;
                }
            }

            auto buf = new RangeInfo[num_infos + 1];
            for (size_t i = 0; i < num_infos; i++) {
                buf[i] = infos[i];
            }
            
            infos.reset(buf);
            infos[num_infos].nodes[0] = node_id;
            infos[num_infos].is_mem[0] = false;
            infos[num_infos].start = s;
            ++num_infos;
        }

        auto RangeGroup::append_node(const std::string &s, int node_id, bool is_mem) noexcept -> void {
            if (node_id == 0) {
                std::cerr << ">> Error: node 0 is not supposed to be in range group\n";
                return;
            }

            if (num_infos == 0) {
                std::cerr << ">> Error: add a main server first\n";
                return;
            }

            for (size_t i = 0; i < num_infos; i++) {
                if (infos[i].start == s) {
                    if (infos[i].nodes[node_id] != 0) {
                        return;
                    }

                    // this duplication is just for convenience that node[0] = main server's node_id
                    infos[i].nodes[node_id] = node_id;
                    infos[i].is_mem[node_id] = is_mem;
                    return;
                }
            }

            std::cerr << ">> Error: no main server found\n";
        }

        auto RangeGroup::append_cpu(const std::string &s, int node_id) noexcept -> void {
            append_node(s, node_id, false);
        }

        auto RangeGroup::append_mem(const std::string &s, int node_id) noexcept -> void {
            append_node(s, node_id, true);
        }

        /*
         * The protocol buffer if in following format
         * -------  Fixed Field  -------
         * 8B             |    node_num
         * sizeof(nodes)  |    nodes
         * 8B             |    num_infos
         * ------- Dynamic Field -------
         * 8B             |  string size
         * start.size()   |  string
         * sizeof(is_mem) |  is_mem
         * sizeof(nodes)  |  nodes
         */
        auto ClusterMeta::serialize() const noexcept -> std::unique_ptr<uint8_t[]> {
            // node_num + nodes
            auto total_size = sizeof(cluster);
            // num_infos
            total_size += sizeof(group.num_infos);
            // dynamic field
            for (size_t i = 0; i < group.num_infos; i++) {
                total_size += sizeof(uint64_t);
                total_size += group.infos[i].start.size();
                total_size += sizeof(group.infos[i].is_mem);
                total_size += sizeof(group.infos[i].nodes);
            }

            auto buf = new uint8_t[total_size];
            auto offset = 0UL;
            // all our machines are little-endian, no need to convert
            // I separate these fields just for easy debugging
            memcpy(buf, &cluster.node_num, sizeof(cluster.node_num));
            offset += sizeof(cluster.node_num);
            memcpy(buf + offset, &cluster.nodes, sizeof(cluster.nodes));
            offset += sizeof(cluster.nodes);
            memcpy(buf + offset, &group.num_infos, sizeof(group.num_infos));
            offset += sizeof(group.num_infos);
            for (size_t i = 0; i < group.num_infos; i++) {
                auto tmp = group.infos[i].start.size();
                // header
                memcpy(buf + offset, &tmp, sizeof(tmp));
                offset += sizeof(tmp);
                // string
                memcpy(buf + offset, group.infos[i].start.data(), group.infos[i].start.size());
                offset += group.infos[i].start.size();
                // is_mem
                memcpy(buf + offset, group.infos[i].is_mem, sizeof(group.infos[i].is_mem));
                offset += sizeof(group.infos[i].is_mem);
                // nodes
                memcpy(buf + offset, group.infos[i].nodes, sizeof(group.infos[i].nodes));
                offset += sizeof(group.infos[i].nodes);
            }

            return std::unique_ptr<uint8_t[]>(buf);
        }

        auto ClusterMeta::deserialize(const uint8_t *buf) -> void {
            auto offset = 0UL;
            memcpy(&cluster.node_num, buf, sizeof(cluster.node_num));
            offset += sizeof(cluster.node_num);
            memcpy(&cluster.nodes, buf + offset, sizeof(cluster.nodes));
            offset += sizeof(cluster.nodes);
            memcpy(&group.num_infos, buf + offset, sizeof(group.num_infos));
            offset += sizeof(group.num_infos);
            auto infos = new RangeInfo[group.num_infos];
            for (size_t i = 0; i < group.num_infos; i++) {
                auto tmp = 0ULL;
                memcpy(&tmp, buf + offset, sizeof(tmp));
                offset += sizeof(tmp);
                infos[i].start.assign(reinterpret_cast<const char *>(buf + offset), tmp);
                offset += tmp;
                memcpy(infos[i].is_mem, buf + offset, sizeof(infos[i].is_mem));
                offset += sizeof(infos[i].is_mem);
                memcpy(infos[i].nodes, buf + offset, sizeof(infos[i].nodes));
                offset += sizeof(infos[i].nodes);
            }
            group.infos.reset(infos);
        }

        auto ClusterMeta::dump() const noexcept -> void {
            std::cout << "--------------------- Meta Info --------------------- \n";
            std::cout << ">> node num: " << cluster.node_num << "\n";
            std::cout << ">> node info: \n";
            for (size_t i = 0; i < cluster.node_num; i++) {
                std::cout << ">> node " << i << "\n";
                std::cout << "-->> node id: " << cluster.nodes[i].node_id << "\n";
                std::cout << "-->> total pm: " << cluster.nodes[i].total_pm << "\n";
                std::cout << "-->> availabel pm: " << cluster.nodes[i].available_pm << "\n";
                std::cout << "-->> ip address: " << cluster.nodes[i].addr.to_string() << "\n";
            }
            std::cout << ">> range group: \n";
            for (size_t j = 0; j < group.num_infos; j++) {
                std::cout << "-->> range[" << j << "]: " << group.infos[j].start << "\n";
                std::cout << "-->> nodes: \n";
                for(size_t t = 0; t < Constants::uMAX_NODE; t++) {
                    if (group.infos[j].nodes[t] != 0) {
                        std::cout << "---->> node " << int(group.infos[j].nodes[t]) << "\n";
                        std::cout << "---->> is_mem: " << group.infos[j].is_mem[t] << "\n";
                    }
                }
            }
            
        }
    }
}
