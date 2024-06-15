#include "indexing/indexing.hpp"
#include "workload/workload.hpp"
#include "cmd_parser/cmd_parser.hpp"

#include <cassert>
#include <chrono>
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
    parser.add_option<std::string>("--ycsb", "-y", "c");
    parser.parse(argc, argv);
    
    auto alloc = Memory::Allocator::make_allocator(new byte_t[1024 * 1024 * 1024], 1024 * 1024 * 1024);
    auto logger = WAL::Logger::make_unique_logger(new byte_t[1024 * 1024 * 128]);
    std::cout << "allocator is at " << alloc << "\n";

    auto tid = register_thread(alloc, logger).value();

    auto batch_size = parser.get_as<size_t>("--size").value();
    auto num_thread = parser.get_as<int>("--multithread").value();
    auto type = parser.get_as<std::string>("--ycsb").value();

    auto olfit = OLFIT::make_olfit(alloc, logger.get());
    if (logger == nullptr) {
        std::cout << ">> Logger moved\n";
    }
    auto begin = 99999999999999UL;
    auto batch = 5000000;
    for (int i = 0; i < batch; i++) {
        auto key = std::to_string(begin - i);
        olfit->insert(tid, key.c_str(), key.size(), key.c_str(), 17);
    }

    std::cout << "Leaf size " << sizeof(Indexing::LeafNode) << "\n";
    for (int i = 0; i < batch; i++) {
        auto key = std::to_string(begin - i);
        // std::cout << "updating " << key << "\n";
        if (auto [sta, _] = olfit->update(tid, key.c_str(), key.size(), key.c_str(), 17);
            sta != Enums::OpStatus::Ok) {
            std::cout << "updating " << key << " failed\n";
            return -1;
        }
    }

    /*
    std::cout << "Loading file\n";
    auto load = Workload::read_ycsb_workload("third-party/ycsb-0.17.0/workloads/ycsb_load_" + type + "_debug.data");
    auto run = Workload::read_ycsb_workload("third-party/ycsb-0.17.0/workloads/ycsb_run_" + type + "_debug.data");
    std::cout << "Done\n";

    
    auto start = std::chrono::steady_clock::now();
    std::cout << "Loading...\n";
    for (const auto &l : load[0]) {
        if(auto [s, _] = olfit->insert(tid, l.key.c_str(), l.key.size(), l.key.c_str(), l.key.size()); s != Enums::OpStatus::Ok) {
            std::cout << "Error inserting " << l.key << "\n";
            return -1;
        }
    }
    auto end = std::chrono::steady_clock::now();
    double period = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    std::cout << "Insertion throughput " << 2000000 / period * 1000 << "\n";

    // std::cout << "tree is: \n";
    // olfit->dump();
    // std::cout << "\n";
    
    const std::string update_value = "new value";
    for (const auto &l : run[0]) {
        switch(l.type){
        case Hill::Workload::Enums::Search: {
            if (auto [v, v_sz] = olfit->search(l.key.c_str(), l.key.size()); v == nullptr) {
                std::cout << l.key << " is not found\n";
                return -1;
            }
        }
            break;
        case Hill::Workload::Enums::Update: {
            if (auto [sta, _] = olfit->update(tid, l.key.c_str(), l.key.size(), update_value.c_str(), update_value.size());
                sta != Enums::OpStatus::Ok) {
                std::cout << "updaing " << l.key << " failed\n";
                return -1;
            }

            // std::cout << "Tree after update " << l.key << ":\n";
            // std::cout << "\n";
        }
            break;
        case Hill::Workload::Enums::Insert: {
            olfit->insert(tid, l.key.c_str(), l.key.size(), l.key_or_value.c_str(), l.key_or_value.size());
        }
            break;
        default:
            break;
        }
    }
    */
    return 0;
}
