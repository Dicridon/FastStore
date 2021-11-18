#include "indexing/indexing.hpp"
#include "workload/workload.hpp"
#include "cmd_parser/cmd_parser.hpp"

#include <cassert>
using namespace Hill;
using namespace Hill::Indexing;
using namespace CmdParser;

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

auto main(int argc, char *argv[]) -> int {
    Parser parser;
    parser.add_option<size_t>("--size", "-s", 100000);
    parser.add_option<int>("--multithread", "-m", 1);
    parser.parse(argc, argv);
    
    auto alloc = Memory::Allocator::make_allocator(new byte_t[1024 * 1024 * 1024], 1024 * 1024 * 1024);
    auto logger = WAL::Logger::make_unique_logger(new byte_t[1024 * 1024 * 128]);

    auto batch_size = parser.get_as<size_t>("--size").value();
    auto num_thread = parser.get_as<int>("--multithread").value();

    auto olfit = OLFIT::make_olfit(alloc, logger.get());
    if (logger == nullptr) {
        std::cout << ">> Logger moved\n";
    }

    auto workload = Workload::generate_simple_string_workload(batch_size, Workload::Enums::Insert);

    std::vector<std::thread> threads;
    std::vector<int> tids;
    auto partition = batch_size / num_thread;
    for (int i = 0; i < num_thread; i++) {
          tids.push_back(register_thread(alloc, logger).value());
    }
    
    for (int i = 0; i < num_thread; i++) {
        threads.emplace_back([&](int tid, size_t begin, size_t end) {
            if (!olfit->open_log("thread_" + std::to_string(tid) + ".log")) {
                std::cout << "Thread " << tid << " failed to open log file\n";
                exit(-1);
            }
            for (size_t i = begin; i < end; i++) {
                auto &w = workload[i];
                if (olfit->insert(tid, w.key.c_str(), w.key.size(), w.key.c_str(), w.key.size()) != Enums::OpStatus::Ok) {
                    std::cout << ">> Insertion should be successful\n";
                    exit(-1);
                }
            }
        }, tids[i], i * partition, (i + 1) * partition);
    }

    for (auto &t : threads) {
        t.join();
    }

    std::cout << "Checking insertions\n";
    for (const auto &w : workload) {
        if (auto [ptr, s] = olfit->search(w.key.c_str(), w.key.size()); ptr != nullptr) {
            auto value = ptr.get_as<hill_value_t *>();
            if (value->compare(w.key.c_str(), w.key.size()) != 0) {
                std::cout << w.key << " should match\n";
                return -1;
            }
        } else {
            std::cout << "I'm searching for " << w.key << "\n";
            std::cout << "Can you just tell me how can you find a nullptr?\n";
            std::cout << "Dumping the tree\n";
            olfit->dump();
            return -1;
        }
    }

    std::cout << ">> Good job, all done.\n";
    return 0;
}
