#include "stats/stats.hpp"

using namespace Hill::Stats;
auto main() -> int {
    LatencyStats lat;
    for (int i = 0; i < 100; i++) {
        lat.record(i);
    }

    std::cout << lat.avg_latency() << "\n";
    std::cout << lat.p90_latency() << "\n";
    std::cout << lat.p99_latency() << "\n";
    std::cout << lat.p999_latency() << "\n";    
}
