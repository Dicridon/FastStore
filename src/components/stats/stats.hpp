#ifndef __HILL__STATS__STATS__
#define __HILL__STATS__STATS__
#include <chrono>
#include <memory>
namespace Hill {
    namespace Stats {
        struct ThroughputStats {
            uint64_t num_insert;
            uint64_t suc_insert;
            uint64_t num_search;
            uint64_t suc_search;
            std::chrono::time_point<std::chrono::steady_clock> start;
            std::chrono::time_point<std::chrono::steady_clock> end;

            static auto make_stats() -> std::unique_ptr<ThroughputStats> {
                auto ret = std::make_unique<ThroughputStats>();
                ret->num_insert = ret->suc_insert = ret->num_search = ret->suc_search = 0;
                return ret;
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
            
        };
    }
}
#endif
