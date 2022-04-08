#include "misc/misc.hpp"
#include "cmd_parser/cmd_parser.hpp"
#include "memory_manager/memory_manager.hpp"

#include <iostream>
#include <chrono>
#include <random>
#include <thread>

#include <libpmem.h>

using namespace Hill;
using namespace Hill::Memory::TypeAliases;
using namespace std::chrono;

static const std::string sRAND = "rnd";
static const std::string sSEQ = "seq";

auto set_up_pmem(const std::string &pmem_file, size_t size) -> void * {
    // size_t mapped_size;
    // auto base = pmem_map_file(pmem_file.c_str(),
    //                           size,
    //                           PMEM_FILE_CREATE, 0666,
    //                           &mapped_size, nullptr);
    // std::cout << ">> " << size << "GB pmem is mapped\n";
    // return base;
    return new byte_t[size];
}

auto run_rand(const std::string &pmem_file, int num_threads, size_t size) -> void {
    auto base = reinterpret_cast<byte_ptr_t>(set_up_pmem(pmem_file, size));
    std::mutex output_lock;

    std::thread threads[num_threads];
    std::vector<double> latencies;
    for (int i = 0; i < num_threads; i++) {
        threads[i] = std::thread([&](int tid) {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> distrib(0, size - 32);
            std::vector<size_t> offsets;

            int batch = 500000;
            for (int i = 0; i < batch; i++) {
                offsets.push_back(distrib(gen));
            }

            auto start = steady_clock::now();
            auto lat_s = steady_clock::now();
            for (int i = 0; i < batch; i++) {
                memset(base + offsets[i], 0x19, 19);

                if (i != 0 && i % 1000 == 0) {
                    auto lat_e = steady_clock::now();
                    latencies.push_back(duration_cast<nanoseconds>(lat_e - lat_s).count() / 1000);
                    lat_s = steady_clock::now();
                }
            }
            auto end = steady_clock::now();

            auto throughput = (double)batch / duration_cast<milliseconds>(end - start).count() * 1000.0;
            std::sort(latencies.begin(), latencies.end(), [&](double a, double b) {
                return a > b;
            });
            auto average = accumulate(latencies.begin(), latencies.end(), 0) / latencies.size();
            auto p90 = Misc::p90(latencies);
            auto p99 = Misc::p99(latencies);
            auto p999 = Misc::p999(latencies);

            output_lock.lock();
            std::cout << ">> Thread " << tid << ":\n";
            std::cout << "---->> ";
            std::cout << "throughput: " << throughput << " OPS, ";
            std::cout << "average: " << average << " ns, ";
            std::cout << "p90: " << p90 << " ns, ";
            std::cout << "p99: " << p99 << " ns, ";
            std::cout << "p999: " << p999 << " ns\n";
            output_lock.unlock();
        }, i);
    }

    for (auto &t : threads) {
        t.join();
    }
}

auto run_seq(const std::string &pmem_file, int num_threads, size_t size) -> void {
    auto base = reinterpret_cast<byte_ptr_t>(set_up_pmem(pmem_file, size));
    std::mutex output_lock;

    std::thread threads[num_threads];
    std::vector<double> latencies;
    for (int i = 0; i < num_threads; i++) {
        threads[i] = std::thread([&](int tid) {
            int batch = 500000;
            auto start = steady_clock::now();
            auto lat_s = steady_clock::now();
            for (int i = 0; i < batch; i++) {
                memset(base + i * 19, 0x19, 19);

                if (i != 0 && i % 1000 == 0) {
                    auto lat_e = steady_clock::now();
                    latencies.push_back(duration_cast<nanoseconds>(lat_e - lat_s).count() / 1000.0);
                    lat_s = steady_clock::now();
                }
            }
            auto end = steady_clock::now();

            auto throughput = (double)batch / duration_cast<milliseconds>(end - start).count() * 1000;
            std::sort(latencies.begin(), latencies.end(), [&](double a, double b) {
                return a > b;
            });
            auto average = accumulate(latencies.begin(), latencies.end(), 0) / latencies.size();
            auto p90 = Misc::p90(latencies);
            auto p99 = Misc::p99(latencies);
            auto p999 = Misc::p999(latencies);

            output_lock.lock();
            std::cout << ">> Thread " << tid << ":\n";
            std::cout << "---->> ";
            std::cout << "throughput: " << throughput << " OPS, ";
            std::cout << "average: " << average << " ns, ";
            std::cout << "p90: " << p90 << " ns, ";
            std::cout << "p99: " << p99 << " ns, ";
            std::cout << "p999: " << p999 << " ns\n";
            output_lock.unlock();
        }, i);
    }

    for (auto &t : threads) {
        t.join();
    }
}

auto main(int argc, char *argv[]) -> int {
    CmdParser::Parser parser;
    parser.add_option<std::string>("--pmem_file", "-p", "/mnt/pmem2/hill");
    parser.add_option<std::string>("--mode", "-m", sRAND);
    parser.add_option<int>("--threads", "-t", 1);
    parser.add_option<size_t>("--size", "-s", 1024 * 1024 * 1024);

    parser.parse(argc, argv);

    auto pmem_file = parser.get_as<std::string>("--pmem_file").value();
    auto mode = parser.get_as<std::string>("--mode").value();
    auto num_threads = parser.get_as<int>("--threads").value();
    auto size = parser.get_as<size_t>("--size").value();

    if (sRAND == mode) {
        run_rand(pmem_file, num_threads, size);
    } else if (sSEQ == mode) {
        run_seq(pmem_file, num_threads, size);
    } else {
        run_rand(pmem_file, num_threads, size);
    }
    return 0;
}
