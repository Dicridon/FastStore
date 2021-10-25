#include "workload.hpp"

namespace Hill {
    namespace Workload {
        auto generate_simple_string_workload(size_t batch_size, const Enums::WorkloadType &t, bool reverse) -> StringWorkload {
            uint64_t fixed = 0x1UL << 63;
            StringWorkload ret;
            if (reverse) {
                for (size_t i = 0; i < batch_size; i++) {
                    auto v = std::to_string(fixed - i);
                    if (t == Enums::WorkloadType::Search)
                        ret.emplace_back(WorkloadItem::make_workload_item(t, v));
                    else
                        ret.emplace_back(WorkloadItem::make_workload_item(t, v, v));
                }
            } else {
                for (size_t i = 0; i < batch_size; i++) {
                    auto v = std::to_string(fixed + i);
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
