#include "engine/engine.hpp"
#include "indexing/indexing.hpp"
#include "wal/wal.hpp"
#include "cmd_parser/cmd_parser.hpp"
#include "workload/workload.hpp"
#include "kv_pair/kv_pair.hpp"
using namespace Hill;
using namespace Hill::Memory::TypeAliases;
using namespace Hill::KVPair::TypeAliases;
using namespace CmdParser;

auto register_thread(Memory::Allocator *alloc, WAL::Logger *logger) -> std::optional<int> {
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
    parser.add_option<std::string>("--config", "-c", "./bench_config/node1.info");
    parser.add_option<int>("--size", "-s", 100000);
    parser.parse(argc, argv);

    auto config = parser.get_as<std::string>("--config").value();
    auto workload_size = parser.get_as<int>("--size").value();

    auto server = Engine::make_engine(config);
    auto index = Indexing::OLFIT::make_olfit(server->get_allocator(), server->get_logger());
    auto _tid = register_thread(server->get_allocator(), server->get_logger());
    if (!_tid.has_value()) {
        std::cout << ">> Failed to register thread\n";
        exit(-1);
    }

    auto tid = _tid.value();

    auto workload = Workload::generate_simple_string_workload(workload_size, Workload::Enums::Insert);

    for (auto &i : workload) {
        index->insert(tid, i.key.c_str(), i.key.size(), i.key.c_str(), i.key.size());
    }

    for (const auto &w : workload) {
        if (auto [v, _] = index->search(w.key.c_str(), w.key.size()); v != nullptr) {
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
