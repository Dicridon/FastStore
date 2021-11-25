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
    }
}
