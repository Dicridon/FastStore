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

namespace Hill {
    namespace Stats {
        struct ThroughputStats {
            uint64_t num_insert;
            uint64_t suc_insert;
            uint64_t num_search;
            uint64_t suc_search;
            std::chrono::time_point<std::chrono::steady_clock> start;
            std::chrono::time_point<std::chrono::steady_clock> end;

            ThroughputStats() = default;
            ThroughputStats(const ThroughputStats &) = default;
            ThroughputStats(ThroughputStats &&) = default;
            ~ThroughputStats() = default;
            auto operator=(const ThroughputStats &) -> ThroughputStats & = default;
            auto operator=(ThroughputStats &&) -> ThroughputStats & = default;

            auto reset() -> void {
                num_insert = suc_insert = num_search = suc_search = 0;
            }

            inline auto inc_insert() noexcept -> void {
                ++num_insert;
            }

            inline auto inc_suc_insert() noexcept -> void {
                ++suc_insert;
            }

            inline auto inc_search() noexcept -> void {
                ++num_search;
            }

            inline auto inc_suc_search() noexcept -> void {
                ++suc_search;
            }

            inline auto timing_now() noexcept -> void {
                start = std::chrono::steady_clock::now();
            }

            inline auto timing_stop() noexcept -> void {
                end = std::chrono::steady_clock::now();
            }

            inline auto insert_throughput() noexcept -> double {
                double duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
                return num_insert / duration * 1000;
            }

            inline auto suc_insert_throughput() noexcept -> double {
                double duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
                return suc_insert / duration * 1000;
            }

            inline auto search_throughput() noexcept -> double {
                double duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
                return num_search / duration * 1000;
            }

            inline auto suc_search_throughput() noexcept -> double {
                double duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
                return suc_search / duration * 1000;
            }
        };

        struct LatencyStats {
            std::vector<double> insert_latencies;
            std::vector<double> search_latencies;

            LatencyStats() = default;
            LatencyStats(size_t cap) {
                insert_latencies.reserve(cap);
                search_latencies.reserve(cap);
            }
            LatencyStats(const LatencyStats &) = default;
            LatencyStats(LatencyStats &&) = default;
            ~LatencyStats() = default;
            auto operator=(const LatencyStats &) -> LatencyStats & = default;
            auto operator=(LatencyStats &&) -> LatencyStats & = default;

            auto reset() -> void {
                insert_latencies.clear();
                search_latencies.clear();
            }
            
            inline auto record_insert(double lat) -> void {
                insert_latencies.push_back(lat);
            }

            inline auto record_search(double lat) -> void {
                search_latencies.push_back(lat);
            }

            inline auto insert_avg_latency() const noexcept -> double {
                return std::accumulate(insert_latencies.cbegin(), insert_latencies.cend(), 0) / insert_latencies.size();
            }

            inline auto search_avg_latency() const noexcept -> double {
                return std::accumulate(search_latencies.cbegin(), search_latencies.cend(), 0) / search_latencies.size();
            }
            

            inline auto percentile(const std::vector<double> &vector, double percent) const noexcept -> double {
                std::vector<double> copy(vector.cbegin(), vector.cend());
                std::sort(copy.begin(), copy.end(), [&](double a, double b) {
                    return a > b;
                });

                auto partition = ceil(copy.size() * (1 - percent / 100));
                return std::accumulate(copy.begin(), copy.begin() + partition, 0) / partition;
            }
            
            
            inline auto insert_p90_latency() const noexcept -> double {
                return percentile(insert_latencies, 90);
            }

            inline auto insert_p99_latency() const noexcept -> double {
                return percentile(insert_latencies, 99);
            }
            
            inline auto insert_p999_latency() const noexcept -> double {
                return percentile(insert_latencies, 99.9);
            }

            inline auto search_p90_latency() const noexcept -> double {
                return percentile(search_latencies, 90);
            }

            inline auto search_p99_latency() const noexcept -> double {
                return percentile(search_latencies, 99);
            }
            
            inline auto search_p999_latency() const noexcept -> double {
                return percentile(search_latencies, 99.9);
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
