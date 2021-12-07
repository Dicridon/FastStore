#include "cmd_parser/cmd_parser.hpp"
#include "read_cache/read_cache.hpp"

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
    parser.parse(argc, argv);

    auto cap = parser.get_as<size_t>("--capacity").value();
    auto batch = parser.get_as<size_t>("--batch").value();
    
    std::vector<std::string> workload;
    for (size_t i = 0; i < batch; i++) {
        workload.emplace_back(std::to_string(i));
    }

    Cache cache(cap);

    std::default_random_engine generator;
    std::uniform_int_distribution<size_t> dist(0, batch - 1);

    for (size_t i = 0; i < cap; i++) {
        cache.insert(std::to_string(i), nullptr, 0);
    }

    for (size_t i = 0; i < batch; i++) {
        auto key = std::to_string(dist(generator));
        std::cout << "getting " << key << "\n";
        cache.dump();
        if (cache.get(key) == nullptr) {
            cache.insert(key, nullptr, 0);
        }
        cache.dump();
        std::cout << "----------------------------\n";
    }

    std::cout << "cache hit ratio: " << cache.hit_ratio() << "\n";
}
