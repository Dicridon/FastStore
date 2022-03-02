#ifndef __HILL__SAMPLER__SAMPLER__
#define __HILL__SAMPLER__SAMPLER__
#include "memory_manager/memory_manager.hpp"
#include "misc/misc.hpp"

#include <vector>
#include <cstddef>
#include <unordered_map>
#include <algorithm>
#include <chrono>
#include <iostream>

namespace Hill {
    namespace Sampling {
        template<typename T, typename = typename std::enable_if_t<std::is_arithmetic_v<T>>>
        class Sample {
        public:
            Sample() : batch_size(0), counter(0), tmp_batch(0) {};
            Sample(size_t b_sz) : batch_size(b_sz), counter(0), tmp_batch(0) {};
            ~Sample() = default;
            Sample(const Sample &) = default;
            Sample(Sample &&) = default;
            auto operator=(const Sample &) -> Sample & = delete;
            auto operator=(Sample &&) -> Sample & = delete;

            // float exception on uninitialized sample
            auto record(const T &in) -> void {
                tmp_batch += in;
                ++counter;

                if (counter == batch_size) {
                    samples.push_back(tmp_batch / batch_size);
                    counter = 0;
                    tmp_batch = 0;
                }
            }

            auto set_batch(size_t b) noexcept -> void {
                if (b > batch_size) {
                    batch_size = b;
                } else {
                    samples.push_back(tmp_batch / counter);
                    counter = 0;
                    tmp_batch = 0;
                }
            }

            auto content() const noexcept -> const std::vector<T> & {
                return samples;
            }

            // ascending
            auto ordered() const noexcept -> const std::vector<T> & {
                check_sorted();
                std::sort(sorted.begin(), sorted.end(), std::less<T>{});
                return sorted;
            }

            // descending
            auto reverse_ordered() const noexcept -> const std::vector<T> & {
                check_sorted();
                std::sort(sorted.begin(), sorted.end(), std::greater<T>{});
                return sorted;
            }

            auto inspect() const noexcept -> void {
                std::cout << "batch_size: " << batch_size << "\n";
                std::cout << "counter: " << counter << "\n";
                std::cout << "tmp_batch: " << tmp_batch / 1000 << "\n";
            }

        private:
            size_t batch_size;
            size_t counter;
            T tmp_batch;
            std::vector<T> samples;
            mutable std::vector<T> sorted;

            auto check_sorted() const noexcept {
                if (sorted.size() < samples.size()) {
                    sorted.reserve(samples.size());

                    for (const auto i : samples) {
                        sorted.push_back(i);
                    }
                }
            }
        };

        using SampleType = int;

        template<typename T>
        class Sampler {
        public:
            Sampler() : cursor(0) {};
            ~Sampler() = default;
            Sampler(const Sampler &) = default;
            Sampler(Sampler &&) = default;
            auto operator=(const Sampler &) -> Sampler & = delete;
            auto operator=(Sampler &&) -> Sampler & = delete;

            auto new_sampling_type(size_t batch_size) -> std::optional<SampleType> {
                if (cursor < Memory::Constants::iTHREAD_LIST_NUM) {
                    samples[cursor].set_batch(batch_size);
                    return cursor++;
                }

                return {};
            }

            auto record(const T& in, const SampleType &type) -> void {
               samples[type].record(in);
            }

            auto get_sample(const SampleType &type) const noexcept -> const Sample<T> & {
                return samples[type];
            }

        private:
            size_t cursor;
            Sample<T> samples[Memory::Constants::iTHREAD_LIST_NUM];
        };

        /*
         * An RAII class that takes a Sampler and a SampleType,
         * it will record the time consumed in a certain scope
         *
         * This class is like std::scoped_lock
         */
        template<typename  T>
        class SampleRecorder {
        public:
            SampleRecorder(Sampler<T> &s, SampleType ty)
                : sampler(&s), type(ty),
                  start(std::chrono::steady_clock::now()) {};

            ~SampleRecorder() {
                auto end = std::chrono::steady_clock::now();
                auto period = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
                auto in = T(period);
                sampler->record(in, type);
            }
        private:
            Sampler<T> *sampler;
            SampleType type;
            std::chrono::time_point<std::chrono::steady_clock> start;
        };

        class HandleSampler {
        public:
            HandleSampler(size_t bs) : batch_size(bs) {};
            ~HandleSampler() = default;
            HandleSampler(const HandleSampler &) = delete;
            HandleSampler(HandleSampler &&) = delete;
            auto operator=(const HandleSampler &) -> HandleSampler & = delete;
            auto operator=(HandleSampler &&) -> HandleSampler & = delete;

            // insert, search, update
            static const std::string PARSE;
            static const std::string CAP_CHECK;
            static const std::string CAP_RECHECK;
            static const std::string INDEXING;
            static const std::string RESP_MSG;
            static const std::string RESP;

            // range
            static const std::string MERGE;

            Sampler<uint64_t> insert_sampler;
            Sampler<uint64_t> search_sampler;
            Sampler<uint64_t> update_sampler;
            Sampler<uint64_t> remove_sampler;
            Sampler<uint64_t> scan_sampler;

            auto prepare() -> void;
            inline auto to_sample_type(const std::string &in) const noexcept -> SampleType {
                return sample_map.find(in)->second;
            }
            auto report_insert() const noexcept -> void;
            auto report_search() const noexcept -> void;
            auto report_update() const noexcept -> void;
            auto report_remove() const noexcept -> void;
            auto report_scan() const noexcept -> void;

        private:
            size_t batch_size;
            std::unordered_map<std::string, SampleType> sample_map;
        };

        class ClientSampler {
        public:
            ClientSampler(size_t b) : batch_size(b) {};
            ~ClientSampler() = default;
            ClientSampler(const ClientSampler &) = delete;
            ClientSampler(ClientSampler &&) = delete;
            auto operator=(const ClientSampler &) -> ClientSampler & = delete;
            auto operator=(ClientSampler &&) -> ClientSampler & = delete;

            static const std::string RDMA;
            static const std::string CACHE;
            static const std::string CACHE_RDMA;
            static const std::string CHECK_RPC;
            static const std::string PRE_REQ;
            static const std::string RPC;
            static const std::string CONTI;

            Sampler<uint64_t> insert_sampler;
            Sampler<uint64_t> search_sampler;
            Sampler<uint64_t> update_sampler;
            Sampler<uint64_t> remove_sampler;
            Sampler<uint64_t> scan_sampler;
            Sampler<uint64_t> common_sampler;

            auto prepare() -> void;
            inline auto to_sample_type(const std::string &in) const noexcept -> SampleType {
                return sample_map.find(in)->second;
            }
            auto report_insert() const noexcept -> void;
            auto report_search() const noexcept -> void;
            auto report_update() const noexcept -> void;
            auto report_remove() const noexcept -> void;
            auto report_scan() const noexcept -> void;

        private:
            size_t batch_size;
            std::unordered_map<std::string, SampleType> sample_map;
        };
    }
}
#endif
