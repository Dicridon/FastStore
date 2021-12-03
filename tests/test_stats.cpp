#include "stats/stats.hpp"

using namespace Hill::Stats;
auto main() -> int {
    LatencyStats lat;
    for (int i = 0; i < 100; i++) {
        lat.record_insert(i);
    }

    std::cout << lat.insert_avg_latency() << "\n";
    std::cout << lat.insert_p90_latency() << "\n";
    std::cout << lat.insert_p99_latency() << "\n";
    std::cout << lat.insert_p999_latency() << "\n";    
}
