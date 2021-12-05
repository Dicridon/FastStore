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
    parser.add_option<std::string>("--ycsb", "-y", "C");
    parser.parse(argc, argv);
    
    auto alloc = Memory::Allocator::make_allocator(new byte_t[1024 * 1024 * 1024], 1024 * 1024 * 1024);
    auto logger = WAL::Logger::make_unique_logger(new byte_t[1024 * 1024 * 128]);

    auto tid = register_thread(alloc, logger).value();

    auto batch_size = parser.get_as<size_t>("--size").value();
    auto num_thread = parser.get_as<int>("--multithread").value();
    auto type = parser.get_as<std::string>("--ycsb").value();

    auto olfit = OLFIT::make_olfit(alloc, logger.get());
    if (logger == nullptr) {
        std::cout << ">> Logger moved\n";
    }

    std::cout << "Loading file\n";
    auto load = Workload::read_ycsb_workload("2M_load_" + type + "_debug.data");
    auto run = Workload::read_ycsb_workload("2M_run_" + type + "_debug.data");
    std::cout << "Done\n";
    
    for (const auto &l : load[0]) {
        if(olfit->insert(tid, l.key.c_str(), l.key.size(), l.key.c_str(), l.key.size()) != Enums::OpStatus::Ok) {
            std::cout << "Error inserting " << l.key << "\n";
            return -1;
        }
    }
    
    for (const auto &l : run[0]) {
        if(auto [v, _] = olfit->search(l.key.c_str(), l.key.size()); v == nullptr) {
            std::cout << "Error searching " << l.key << "\n";
            return -1;
        }
    }

    /*
    auto workload = Workload::generate_simple_string_workload_with_begin(8, 4, Workload::Enums::Insert);
    for (const auto &l : workload) {
        if(olfit->insert(tid, l.key.c_str(), l.key.size(), l.key.c_str(), l.key.size()) != Enums::OpStatus::Ok) {
            std::cout << "Error inserting " << l.key << "\n";
            return -1;
        }
        olfit->dump();
        std::cout << "-------------------------------------------------------------\n";
    }
    
    for (const auto &l : workload) {
        if(auto [v, _] = olfit->search(l.key.c_str(), l.key.size()); v == nullptr) {
            std::cout << "Error searching " << l.key << "\n";
            return -1;
        }
    }
    */
    
    return 0;
}
