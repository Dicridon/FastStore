#include "workload/workload.hpp"
#include "city/city.hpp"

using namespace Hill::Workload;

auto main() -> int {
    std::vector<StringWorkload> loads;
    std::vector<size_t> counter;
    int threads = 8;
    counter.resize(threads);
    size_t batch = 1000000;
    auto begin = (1UL << 63) + (1UL << 62);
    for (int i = 0; i < threads; i++) {
        counter[i] = 0;
        loads.emplace_back(generate_simple_string_workload_with_begin(begin - i * batch, batch, Enums::WorkloadType::Insert));
    }

    for (auto &l : loads) {
        for (auto &i : l) {
            auto pos = CityHash64(i.key.c_str(), i.key.size()) % threads;
            ++counter[pos];
        }
    }

    for (int i = 0; i < threads; i++) {
        std::cout << i << ": " << counter[i] << "\n";
    }
}
