#include "cmd_parser/cmd_parser.hpp"
#include "read_cache/read_cache.hpp"
#include "workload/workload.hpp"

#include <iostream>
#include <fstream>
#include <unordered_map>

#include <random>

using namespace Hill;
using namespace Hill::ReadCache;
int main(int argc, char *argv[]) {
    CmdParser::Parser parser;

    parser.add_option<size_t>("--capacity", "-c", 100);
    parser.add_option<size_t>("--batch", "-b", 100);
    parser.add_option<std::string>("--ycsb", "-y", "C");
    parser.parse(argc, argv);


    auto load_type = parser.get_as<std::string>("--ycsb").value();
    Cache cache(1000000);
    // auto load = Workload::read_ycsb_workload("2M_load_" + load_type + "_debug.data");
    auto run = Workload::read_ycsb_workload("2M_run_" + load_type + "_debug.data");

    for (const auto &r : run[0]) {
        if (cache.get(r.key) == nullptr) {
            cache.insert(r.key, nullptr, 0);
        }
    }
    
    for (const auto &r : run[0]) {
        if (cache.get(r.key) == nullptr) {
            cache.insert(r.key, nullptr, 0);
        }
    }

    std::cout << "Hit ratio: " << cache.hit_ratio() << "\n";
}
