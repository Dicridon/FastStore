#ifndef __HILL__STATS__STATS__
#define __HILL__STATS__STATS__
#include <chrono>
#include <memory>
#include <vector>
#include <algorithm>
#include <functional>
#include <numeric>
#include <iostream>
#include <cmath>

#include "misc/misc.hpp"

namespace Hill {
    namespace Stats {
        struct ThroughputStats {
            uint64_t num_ops;
            uint64_t suc_ops;
            std::chrono::time_point<std::chrono::steady_clock> start;
            std::chrono::time_point<std::chrono::steady_clock> end;

            ThroughputStats() = default;
            ThroughputStats(const ThroughputStats &) = default;
            ThroughputStats(ThroughputStats &&) = default;
            ~ThroughputStats() = default;
            auto operator=(const ThroughputStats &) -> ThroughputStats & = default;
            auto operator=(ThroughputStats &&) -> ThroughputStats & = default;

            auto inline timing_now() noexcept -> void {
                start = std::chrono::steady_clock::now();
            }

            auto inline timing_stop() noexcept -> void {
                end = std::chrono::steady_clock::now();
            }
            
            auto reset() -> void {
                num_ops = suc_ops = 0;
            }

            auto inline inc_ops() -> void {
                ++num_ops;
            }

            auto inline inc_suc_ops() -> void {
                ++suc_ops;
            }

            inline auto throughput() noexcept -> double {
                double duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
                return suc_ops / duration * 1000;
            }
        };

        struct LatencyStats {
            std::vector<double> latencies;

            LatencyStats() = default;
            LatencyStats(size_t cap) {
                latencies.reserve(cap);
            }
            LatencyStats(const LatencyStats &) = default;
            LatencyStats(LatencyStats &&) = default;
            ~LatencyStats() = default;
            auto operator=(const LatencyStats &) -> LatencyStats & = default;
            auto operator=(LatencyStats &&) -> LatencyStats & = default;

            auto reset() -> void {
                latencies.clear();
            }
            
            inline auto record(double lat) -> void {
                latencies.push_back(lat);
            }

            inline auto avg_latency() const noexcept -> double {
                return std::accumulate(latencies.cbegin(), latencies.cend(), 0) / latencies.size();
            }

            inline auto percentile(const std::vector<double> &vector, double percent) const noexcept -> double {
                std::vector<double> copy(vector.cbegin(), vector.cend());
                std::sort(copy.begin(), copy.end(), [&](double a, double b) {
                    return a > b;
                });

                auto partition = ceil(copy.size() * (1 - percent / 100));
                return std::accumulate(copy.begin(), copy.begin() + partition, 0) / partition;
            }
            
            inline auto p90_latency() const noexcept -> double {
                return percentile(latencies, 90);
            }

            inline auto p99_latency() const noexcept -> double {
                return percentile(latencies, 99);
            }
            
            inline auto p999_latency() const noexcept -> double {
                return percentile(latencies, 99.9);
            }
        };

        struct SyntheticStats {
            ThroughputStats throughputs;
            LatencyStats latencies;
            double cache_hit_ratio;
            auto reset() -> void {
                throughputs.reset();
                latencies.reset();
            }
        };
    }
}
#endif
