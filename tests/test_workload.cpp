#include "workload/workload.hpp"

using namespace Hill::Workload;

auto main() -> int {
    auto put_load = generate_simple_string_workload(100, Enums::WorkloadType::Insert);
    auto get_load = generate_simple_string_workload(100, Enums::WorkloadType::Search);

    for (int i = 0; i < 100; i++) {
        std::cout << "Put: " << put_load[i].key << "\n";
        std::cout << "Get: " << get_load[i].key << "\n";        
    }
}
