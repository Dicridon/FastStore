#ifndef __HILL__STORE__RANGE_MERGER__RANGE_MERGER__
#define __HILL__STORE__RANGE_MERGER__RANGE_MERGER__

#include "kv_pair/kv_pair.hpp"
#include "indexing/indexing.hpp"

#include <queue>

namespace Hill {
    namespace Store {
        class Merger {
        public:
            Merger() = default;
            ~Merger() = default;
            Merger(const Merger &) = delete;
            Merger(Merger &&) = delete;
            auto operator=(const Merger &) -> Merger& = delete;
            auto operator=(Merger &&) -> Merger& = delete;

            static auto make_merger(std::vector<std::vector<Indexing::ScanHolder>> &ranges)
                -> std::unique_ptr<Merger>
            {
                auto ret = std::make_unique<Merger>();
                
                for (auto &vec : ranges) {
                    ret->iters.push_back(vec.begin());
                    ret->ends.push_back(vec.end());
                }

                return ret;
            }

            auto merger(size_t total) -> std::vector<Indexing::ScanHolder>;
            
        private:
            using scanholder_iter_ptr = std::vector<Indexing::ScanHolder>::iterator *;
            using scanholder_iter_ptr_pair = std::pair<std::vector<Indexing::ScanHolder>::iterator *,
                std::vector<Indexing::ScanHolder>::iterator *>;
            
            std::vector<std::vector<Indexing::ScanHolder>::iterator> iters;
            std::vector<std::vector<Indexing::ScanHolder>::iterator> ends;
        };
    }
}
#endif
