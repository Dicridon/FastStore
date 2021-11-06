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
    parser.add_option<int>("--size", "-s", 100000);
    
    if (argc < 2) {
        return -1;
    }
    
    parser.parse(argc, argv);

    auto type = parser.get_as<std::string>("--type").value();
    auto config = parser.get_as<std::string>("--config").value();

    if (type == "monitor") {
        auto monitor = Monitor::make_monitor(config);
        
        monitor->launch();
        Misc::pend();
    } else if (type == "server") {
        auto server = StoreServer::make_server(config, 1024 * 1024);
        server->launch();

        if (!server->launch_one_erpc_listen_thread()) {
            std::cout << "Can't launch erpc listen thread\n";
            return -1;
        }

        auto _thread = server->register_erpc_handler_thread();
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
        auto batch = parser.get_as<int>("--size").value();
        auto client = StoreClient::make_client(config);
        client->launch();

        auto put_load = Workload::generate_simple_string_workload(batch, Workload::Enums::Insert);
        auto get_load = Workload::generate_simple_string_workload(batch, Workload::Enums::Search);
        
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
