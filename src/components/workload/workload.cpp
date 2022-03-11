#include "workload.hpp"

namespace Hill {
    namespace Workload {
        auto generate_simple_string_workload(size_t batch_size, const Enums::WorkloadType &t, bool reverse) -> StringWorkload {
            return generate_simple_string_workload_with_begin((1UL << 63) + (1UL << 62), batch_size, t, reverse);
        }

        auto generate_simple_string_workload_with_begin(size_t begin, size_t batch_size, const Enums::WorkloadType &t, bool reverse) -> StringWorkload {
            StringWorkload ret;
            if (reverse) {
                for (size_t i = 0; i < batch_size; i++) {
                    auto v = std::to_string(begin - i);
                    if (t == Enums::WorkloadType::Search)
                        ret.emplace_back(WorkloadItem::make_workload_item(t, v));
                    else
                        ret.emplace_back(WorkloadItem::make_workload_item(t, v, v));
                }
            } else {
                for (size_t i = 0; i < batch_size; i++) {
                    auto v = std::to_string(begin + i);
                    if (t == Enums::WorkloadType::Search)
                        ret.emplace_back(WorkloadItem::make_workload_item(t, v));
                    else
                        ret.emplace_back(WorkloadItem::make_workload_item(t, v, v));
                }
            }

            return ret;
        }

        auto read_ycsb_workload(const std::string &filename, size_t num_thread)
            -> std::vector<StringWorkload>
        {
            std::vector<StringWorkload> ret;
            ret.resize(num_thread);

            std::ifstream file(filename);
            if (!file.is_open()) {
                throw std::runtime_error("Can't open file " + filename);
            }

            std::string buf;
            std::regex ycsb_pattern("([[:upper:]]+)\\suser(\\d+)");
            std::smatch load;

            size_t counter = 0;
            std::string op;
            std::string key;
            std::string value;
            WorkloadItem item;
            
            while(std::getline(file, buf)) {
                if (!std::regex_search(buf, load, ycsb_pattern)) {
                    continue;
                }

                op = load[1].str();
                key = load[2].str();
                if (op == "INSERT") {
                    item = WorkloadItem::make_workload_item(Enums::WorkloadType::Insert, key, key);
                } else if (op == "READ") {
                    item = WorkloadItem::make_workload_item(Enums::WorkloadType::Search, key);
                } else if (op == "UPDATE") {
                    item = WorkloadItem::make_workload_item(Enums::WorkloadType::Update, key, key);
                } else if (op == "DELETE") {
                    continue;
                } else if (op == "RANGE") {
                    item = WorkloadItem::make_workload_item(Enums::WorkloadType::Range, key);
                } else {
                    continue;
                }
                ret[(counter++) % num_thread].push_back(std::move(item));
            }
            return ret;
        }
    }
}
