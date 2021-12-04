#include "workload/workload.hpp"
#include "city/city.hpp"

using namespace Hill::Workload;

auto main() -> int {
    auto loads = read_ycsb_workload("workload.data", 2);

    for (const auto &w : loads) {
        std::cout << "------------------------------------------------\n";
        for (const auto &i : w) {
            std::cout << "Type: " << i.type << " with key being " << i.key << "\n";
        }
    }
}
