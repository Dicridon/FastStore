#include "engine/engine.hpp"
#include "cmd_parser/cmd_parser.hpp"

using namespace Hill;
using namespace Hill::Memory::TypeAliases;
auto run_monitor(const std::string &file) -> bool {
    auto monitor = Cluster::Monitor::make_monitor(file);
    if (monitor == nullptr) {
        return false;
    }
    monitor->dump();
    monitor->launch();
    Misc::pend();
    return true;
}

auto run_server(const std::string &file) -> bool {
    auto base = std::make_unique<byte_t[]>(1024 * 1024 * 1024UL);
    auto engine = Engine::make_engine(base.get(), file);
    if (engine == nullptr) {
        return false;
    }
    engine->dump();
    engine->launch();
    engine->register_thread();

    Misc::pend();
    return true;
}

auto run_client(const std::string &file) -> bool {
    auto client = Client::make_client(file);
    if (client == nullptr) {
        return false;
    }
    client->connect_monitor();

    uint64_t counter = 0UL;
    while(true) {

    }

    return true;
}

int main(int argc, char *argv[]) {
    CmdParser::Parser parser;
    parser.add_option("--number", "-n");
    parser.add_option<std::string>("--type", "-t", "monitor");
    parser.add_option("--config", "-c");
    parser.parse(argc, argv);

    auto type = parser.get_as<std::string>("--type");
    if (!type.has_value()) {
        std::cout << "Please specify the type of this node ('monitor', 'server' or 'client')\n";
        return -1;
    }

    auto file = parser.get_as<std::string>("--config");
    if (!file.has_value()) {
        std::cout << "Please specify a config file\n";
        return -1;
    }

    auto t = type.value();
    if (t == "monitor") {
        if (!run_monitor(file.value())) {
            std::cout << "Monitor not created\n";
            return -1;
        }
    } else if (t == "server") {
        if (!run_server(file.value())) {
            std::cout << "Server not launched\n";
            return -1;
        }
    } else if (t == "client") {
        if (!run_client(file.value())) {
            std::cout << "Client is invalid\n";
            return -1;
        }
    } else {
        std::cout << "Unkown type " << t << "\n";
        return -1;
    }
    return 0;
}
