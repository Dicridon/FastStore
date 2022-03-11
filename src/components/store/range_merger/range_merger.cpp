#include "range_merger.hpp"
namespace Hill {
    namespace Store {
        auto Merger::merge(size_t total) -> std::vector<Indexing::ScanHolder> {
            auto cmp = [](scanholder_iter_ptr_pair &lhs, scanholder_iter_ptr_pair &rhs) -> bool {
                return *(*lhs.first)->key >= *(*rhs.first)->key;
            };
            
            std::priority_queue<scanholder_iter_ptr_pair,
                                std::vector<scanholder_iter_ptr_pair>,
                                decltype(cmp)> heap(cmp);

            for (auto i = 0UL; i < iters.size(); i++) {
                if (iters[i] == ends[i])
                    continue;
                heap.push({&iters[i], &ends[i]});
            }

            std::vector<Indexing::ScanHolder> ret;

            scanholder_iter_ptr p;
            while(total > 0) {
                p = heap.top().first;
                if (*p == *heap.top().second) {
                    heap.pop();
                    continue;
                }
                ret.push_back(**p);
                heap.pop();
                ++(*p);
                --total;
            }

            return ret;
        }
    }
}
