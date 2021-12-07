#include "store/store.hpp"
#include "cmd_parser/cmd_parser.hpp"

using namespace Hill;
using namespace Hill::Store;
using namespace Hill::Cluster;

struct ClientWorkloads {
    Workload::StringWorkload *insert_load;
    Workload::StringWorkload *search_load;

    ClientWorkloads() : insert_load(nullptr), search_load(nullptr) {};
};

auto run_monitor(const std::string &config) -> void {
    auto monitor = Monitor::make_monitor(config);

    monitor->launch();
    Misc::pend();
}

auto run_server(const std::string &config, int threads) -> void {
    auto server = StoreServer::make_server(config);
    server->launch(threads);

    std::vector<std::thread> handler_threads;
    handler_threads.resize(threads);

    for (int i = 0; i < threads; i++) {
        auto _thread = server->register_erpc_handler_thread();
        if (!_thread.has_value()) {
            std::cerr << "Can't start a server thread\n";
            return;
        }
        handler_threads[i] = std::move(_thread.value());
    }

    if (!server->launch_one_erpc_listen_thread()) {
        std::cout << "Can't launch erpc listen thread\n";
        return;
    }

    for (auto &t : handler_threads) {
        if (t.joinable()) {
            t.join();
        } else {
            std::cerr << "Can't join server thread\n";
            return;
        }
    }
}

auto run_ycsb_workload(const std::string &config, int threads, const std::string &ycsb_type) -> void {
    auto client = StoreClient::make_client(config);
    client->launch();

    std::vector<std::thread> clients;
    std::vector<Stats::SyntheticStats> stats;

    auto load_phase = "hill_load_workload" + ycsb_type + "_data";
    auto run_phase = "hill_run_workload" + ycsb_type + "_data";

    std::cout << ">> Loading workload from workload file\n";
    auto loads = Workload::read_ycsb_workload(load_phase, threads);
    auto runs = Workload::read_ycsb_workload(run_phase, threads);
    std::cout << ">> Loaded and start testing\n";

    clients.resize(threads);
    stats.resize(threads);

    for (int i = 0; i < threads; i++) {
        clients[i] = std::move(client->register_thread(loads[i], stats[i]).value());
    }

    for (auto &t : clients) {
        if (t.joinable())
            t.join();
    }

    std::cout << ">> Reporting in load phase: \n";
    for (int i = 0; i < threads; i++) {
        std::cout << "[[ Thread " << i << "]]:\n";
        std::cout << "---->> insertions: "
                  << stats[i].throughputs.suc_insert << "/" << stats[i].throughputs.num_insert
                  << "\n";
        std::cout << "---->> throughput: " << stats[i].throughputs.insert_throughput() << " Ops/second, "
                  << "average latency: " << stats[i].latencies.insert_avg_latency() << "us, "
                  << "p90: " << stats[i].latencies.insert_p90_latency() << "us, "
                  << "p99: " << stats[i].latencies.insert_p99_latency() << "us, "
                  << "p999: " << stats[i].latencies.insert_p999_latency() << "us"
                  << "\n";
    }

    std::cout << std::endl;

    for (int i = 0; i < threads; i++) {
        clients[i] = std::move(client->register_thread(runs[i], stats[i]).value());
    }

    for (auto &t : clients) {
        if (t.joinable())
            t.join();
    }

    std::cout << ">> Reporting in run phase: \n";
    for (int i = 0; i < threads; i++) {
        std::cout << "[[ Thread " << i << "]]:\n";
        std::cout << "---->> insertions: "
                  << stats[i].throughputs.suc_insert << "/" << stats[i].throughputs.num_insert
                  << "\n";
        std::cout << "---->> searches: "
                  << stats[i].throughputs.suc_search << "/" << stats[i].throughputs.num_search
                  << "\n";
        std::cout << "---->> throughput: " << stats[i].throughputs.search_throughput() << " Ops/second, "
                  << "average latency: " << stats[i].latencies.search_avg_latency() << "us, "
                  << "p90: " << stats[i].latencies.search_p90_latency() << "us, "
                  << "p99: " << stats[i].latencies.search_p99_latency() << "us, "
                  << "p999: " << stats[i].latencies.search_p999_latency() << "us"
                  << "\n";
        std::cout << "---->> cache hit ratio " << stats[i].cache_hit_ratio << "\n";
    }
}

auto run_simple_workload(const std::string &config, int threads, int batch) -> void {
    auto client = StoreClient::make_client(config);
    client->launch();

    std::vector<std::thread> clients;
    std::vector<Stats::SyntheticStats> stats;
    std::vector<Workload::StringWorkload> insert_loads;
    std::vector<Workload::StringWorkload> search_loads;

    auto begin = (1UL << 63) + (1UL << 62);
    clients.resize(threads);
    stats.resize(threads);

    for (int i = 0; i < threads; i++) {
        insert_loads.emplace_back(Workload::generate_simple_string_workload_with_begin(begin - i * batch, batch, Workload::Enums::WorkloadType::Insert));
        search_loads.emplace_back(Workload::generate_simple_string_workload_with_begin(begin - i * batch, batch, Workload::Enums::WorkloadType::Search));
    }

    for (int i = 0; i < threads; i++) {
        clients[i] = std::move(client->register_thread(insert_loads[i], stats[i]).value());
    }

    for (auto &t : clients) {
        if (t.joinable())
            t.join();
    }

    std::cout << ">> Reporting inserts: \n";
    for (int i = 0; i < threads; i++) {
        std::cout << "[[ Thread " << i << "]]:\n";
        std::cout << "---->> insertions: "
                  << stats[i].throughputs.suc_insert << "/" << stats[i].throughputs.num_insert
                  << "\n";
        std::cout << "---->> searches: "
                  << stats[i].throughputs.suc_search << "/" << stats[i].throughputs.num_search
                  << "\n";
        std::cout << "---->> throughput: " << stats[i].throughputs.insert_throughput() << " Ops/second, "
                  << "average latency: " << stats[i].latencies.insert_avg_latency() << "us, "
                  << "p90: " << stats[i].latencies.insert_p90_latency() << "us, "
                  << "p99: " << stats[i].latencies.insert_p99_latency() << "us, "
                  << "p999: " << stats[i].latencies.insert_p999_latency() << "us"
                  << "\n";
    }

    std::cout << std::endl;

    for (int i = 0; i < threads; i++) {
        clients[i] = std::move(client->register_thread(search_loads[i], stats[i]).value());
    }

    for (auto &t : clients) {
        if (t.joinable())
            t.join();
    }

    std::cout << ">> Reporting searches: \n";
    for (int i = 0; i < threads; i++) {
        std::cout << "[[ Thread " << i << "]]:\n";
        std::cout << "---->> insertions: "
                  << stats[i].throughputs.suc_insert << "/" << stats[i].throughputs.num_insert
                  << "\n";
        std::cout << "---->> searches: "
                  << stats[i].throughputs.suc_search << "/" << stats[i].throughputs.num_search
                  << "\n";
        std::cout << "---->> throughput: " << stats[i].throughputs.search_throughput() << " Ops/second, "
                  << "average latency: " << stats[i].latencies.search_avg_latency() << "us, "
                  << "p90: " << stats[i].latencies.search_p90_latency() << "us, "
                  << "p99: " << stats[i].latencies.search_p99_latency() << "us, "
                  << "p999: " << stats[i].latencies.search_p999_latency() << "us"
                  << "\n";
        std::cout << "---->> cache hit ratio " << stats[i].cache_hit_ratio << "\n";
    }
}

auto run_client(const std::string &config, int threads, CmdParser::Parser &parser) -> void {
    auto ycsb = parser.get_as<std::string>("--ycsb");
    if (ycsb.has_value()) {
        run_ycsb_workload(config, threads, ycsb.value());
    } else {
        auto batch = parser.get_as<int>("--size").value();
        run_simple_workload(config, threads, batch);
    }
}

auto main(int argc, char *argv[]) -> int {
    CmdParser::Parser parser;
    parser.add_option<std::string>("--type", "-t", "monitor");
    parser.add_option<std::string>("--uri", "-u", "127.0.0.1:2333");
    parser.add_option<std::string>("--config", "-c", "config.moni");
    parser.add_option<int>("--size", "-s", 100000);
    parser.add_option<int>("--multithread", "-m", 1);
    parser.add_option("--ycsb", "-y");

    if (argc < 2) {
        return -1;
    }

    parser.parse(argc, argv);

    auto type = parser.get_as<std::string>("--type").value();
    auto config = parser.get_as<std::string>("--config").value();
    auto threads = parser.get_as<int>("--multithread").value();

    if (type == "monitor") {
        run_monitor(config);
    } else if (type == "server") {
        run_server(config, threads);
    } else {
        run_client(config, threads, parser);
    }
    return 0;
}
