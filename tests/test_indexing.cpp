#include "indexing/indexing.hpp"
#include "workload/workload.hpp"

#include <cassert>
using namespace Hill;
using namespace Hill::Indexing;

auto generate_strings(size_t batch_size, bool reverse = false) -> std::vector<std::string> {
    uint64_t fixed = 0x1UL << 63;
    std::vector<std::string> ret;
    if (reverse) {
        for (size_t i = 0; i < batch_size; i++) {
            ret.push_back(std::to_string(fixed + batch_size - i));
        }
    } else {
        for (size_t i = 0; i < batch_size; i++) {
            ret.push_back(std::to_string(fixed + i));
        }
    }
    return ret;
}

auto register_thread(Memory::Allocator *alloc, UniqueLogger &logger) -> std::optional<int> {
    auto atid = alloc->register_thread();
    if (!atid.has_value()) {
        return {};
    }
    auto ltid = logger->register_thread();
    if (!ltid.has_value()) {
        return {};
    }
    assert(atid == ltid);
    return atid;
}

auto main() -> int {
    auto alloc = Memory::Allocator::make_allocator(new byte_t[1024 * 1024 * 1024], 1024 * 1024 * 1024);
    auto logger = WAL::Logger::make_unique_logger(new byte_t[1024 * 1024 * 128]);

    int tid;
    if (auto _tid = register_thread(alloc, logger); !_tid.has_value()) {
        return -1;
    } else {
        tid = _tid.value();
    }
    
    OLFIT olfit(tid, alloc, logger.get());
    if (logger == nullptr) {
        std::cout << ">> Logger moved\n";
    }

    auto workload = Workload::generate_simple_string_workload(100000, Workload::Enums::Insert);

    for (const auto &w : workload) {
        std::cout << ">> Inserting: " << ColorizedString(w.key, Colors::Magenta) << "\n";
        olfit.insert(tid, w.key.c_str(), w.key.size(), w.key.c_str(), w.key.size());
        // std::cout << ">> Dumping\n";
        // olfit.dump();
        // std::cout << "\n";
    }

    std::cout << ">> Insertion done\n";
    std::cout << ">> Searching begins\n";
    for (const auto &w : workload) {
        if (auto [v, _] = olfit.search(w.key.c_str(), w.key.size()); v != nullptr) {
            auto value = v.get_as<hill_value_t *>();
            if (value->compare(w.key.c_str(), w.key.size()) != 0) {
                std::cout << w.key << " should be matched\n";
                exit(-1);
            }
        } else {
            std::cout << "I'm searching for " << w.key << "\n";
            std::cout << "Can you just tell me how can you find a nullptr?\n";
            exit(-1);
        }
    }

    std::cout << ">> Good job, all done.\n";
    return 0;
}
