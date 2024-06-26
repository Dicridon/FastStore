#ifndef __HILL__WORKLOAD__WORKLOAD__
#define __HILL__WORKLOAD__WORKLOAD__

#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <regex>

namespace Hill {
    namespace Workload {
        namespace Enums {
            // just for store.hpp::Enums::RPCOperations
            enum WorkloadType : uint8_t {
                Insert,
                Search,                
                Update,
                Range,                

                Unknownk,
            };
        }

        struct WorkloadItem {
            Enums::WorkloadType type;
            std::string key;
            std::string key_or_value;

            WorkloadItem() = default;
            WorkloadItem(const WorkloadItem &r) = default;
            WorkloadItem(WorkloadItem &&) = default;
            auto operator=(const WorkloadItem &) -> WorkloadItem & = default;
            auto operator=(WorkloadItem &&) -> WorkloadItem & = default;
            
            static auto make_workload_item(const Enums::WorkloadType &type, const std::string &key)
                -> WorkloadItem
            {
                if (type != Enums::WorkloadType::Search && type != Enums::WorkloadType::Range) {
                    throw std::invalid_argument("WorkloadItem should be search");
                }
                
                WorkloadItem item;
                item.type = type;
                item.key = key;
                return item;
            }

            static auto make_workload_item(const Enums::WorkloadType &type, const std::string &key,
                                           const std::string &key_or_value) -> WorkloadItem
            {
                if (type == Enums::WorkloadType::Search) {
                    throw std::invalid_argument("WorkloadItem should not be search");
                }
                
                WorkloadItem item;
                item.type = type;
                item.key = key;
                item.key_or_value = key_or_value;
                return item;
            }
        };

        using StringWorkload = std::vector<WorkloadItem>;

        auto generate_simple_string_workload(size_t batch_size, const Enums::WorkloadType &t, bool reverse = false) -> StringWorkload;
        auto generate_simple_string_workload_with_begin(size_t begin, size_t batch_size, const Enums::WorkloadType &t, bool reverse = false) -> StringWorkload;

        // dispatching one ycsb workload to different threads by round robin
        auto read_ycsb_workload(const std::string &filename, size_t num_thread = 1) -> std::vector<StringWorkload>;
    }
}
#endif
