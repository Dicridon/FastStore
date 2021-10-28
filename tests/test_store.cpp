#include "store/store.hpp"

#include "cmd_parser/cmd_parser.hpp"

using namespace Hill;
using namespace Hill::Store;
using namespace Hill::Cluster;
auto main(int argc, char *argv[]) -> int {
    CmdParser::Parser parser;
    parser.add_option<std::string>("--type", "-t", "monitor");
    parser.add_option<std::string>("--uri", "-u", "127.0.0.1:2333");
    parser.add_option<std::string>("--config", "-c", "config.moni");
    
    if (argc < 2) {
        std::cerr << "Usage: ./test_store --type [monitor, server, client] --config filename";
        return -1;
    }
    
    parser.parse(argc, argv);

    auto _type = parser.get_as<std::string>("--type");
    if (!_type.has_value()) {
        std::cerr << "Failed to parse launching type\n";
        return -1;
    }
    
    auto _config = parser.get_as<std::string>("--config");
    if (!_config.has_value()) {
        std::cerr << "Failed to parse config file\n";
        return -1;
    }

    auto type = _type.value();
    auto config = _config.value();

    if (type == "monitor") {
        auto monitor = Monitor::make_monitor(config);
        
        monitor->launch();
        Misc::pend();
    } else if (type == "server") {
        auto base = new byte_t[1024 * 1024 * 1024];

        auto server = StoreServer::make_server(base, config, 1024 * 1024);
        server->launch();

        auto _thread = server->register_thread();
        if (!_thread.has_value()) {
            std::cerr << "Can't start a server thread\n";
            return -1;
        }

        auto thread = std::move(_thread.value());
        if (thread.joinable()) {
            thread.join();            
        } else {
            std::cerr << "Can't join server thread\n";
            return -1;
        }
    } else {
        auto client = StoreClient::make_client(config);
        client->launch();

        auto put_load = Workload::generate_simple_string_workload(100000, Workload::Enums::Insert);
        auto get_load = Workload::generate_simple_string_workload(100000, Workload::Enums::Search);
        
        auto _thread = client->register_thread(put_load);
        
        if (!_thread.has_value()) {
            std::cerr << "Can't start a client thread\n";
            return -1;
        }

        auto thread = std::move(_thread.value());
        if (thread.joinable()) {
            thread.join();            
        } else {
            std::cerr << "Can't join client thread\n";
            return -1;
        }
    }
    return 0;
}
