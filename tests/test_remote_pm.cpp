#include "rdma/rdma.hpp"
#include "cmd_parser/cmd_parser.hpp"
#include "misc/misc.hpp"
#include "memory_manager/memory_manager.hpp"

#include <iostream>
#include <chrono>
#include <random>
#include <thread>

#include <libpmem.h>

using namespace Hill;
using namespace Hill::Memory::TypeAliases;
using namespace Hill::Misc;
using namespace Hill::RDMAUtil;
using namespace std::chrono;

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

auto run_server(int port, std::unique_ptr<RDMAContext> &rdma_ctx) -> void {
    auto sock = Misc::make_socket(false, port);

    while (true) {
        auto socket = Misc::accept_blocking(sock);

        if (socket == -1) {
            std::cout << ">> Server socket not usable\n";
            return;
        }

        if (auto err = rdma_ctx->default_connect(socket); err != 0) {
            std::cerr << "RDMA connection failed\n";
            return;
        }
    }
}

auto run_client(int tid, std::unique_ptr<RDMAContext> &rdma_ctx, size_t pmem_size,
                std::mutex &lock, std::string &server) -> void
{
    auto socket = socket_connect(false, 2333, server.c_str());
    if (socket == -1) {
        return;
    }

    if (rdma_ctx->default_connect(socket) != 0) {
        return;
    }

    int batch = 500000;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib(0, pmem_size - 32);
    std::vector<size_t> offsets;
    std::vector<double> latencies;
    uint8_t buf[32] = {""};

    for (int i = 0; i < batch; i++) {
        offsets.push_back(distrib(gen));
    }

    auto s = steady_clock::now();
    for (int i = 0; i < batch; i++) {
        rdma_ctx->post_write(buf, 19, 0, offsets[i]);
        rdma_ctx->poll_one_completion();
        if (i > 0 && i % 1000 == 0) {
            auto e = steady_clock::now();
            latencies.push_back(duration_cast<microseconds>(e - s).count() / 1000);
            s = steady_clock::now();
        }
    }
    
    std::sort(latencies.begin(), latencies.end(), [&](double a, double b) {
        return a > b;
    });
    auto average = accumulate(latencies.begin(), latencies.end(), 0) / latencies.size();
    auto p90 = Misc::p90(latencies);
    auto p99 = Misc::p99(latencies);
    auto p999 = Misc::p999(latencies);

    lock.lock();
    std::cout << ">> Thread " << tid << ":\n";
    std::cout << "---->> ";
    std::cout << "average: " << average << " ns, ";
    std::cout << "p90: " << p90 << " ns, ";
    std::cout << "p99: " << p99 << " ns, ";
    std::cout << "p999: " << p999 << " ns\n";
    lock.unlock();
}

auto main(int argc, char *argv[]) -> int {
    CmdParser::Parser parser;
    parser.add_option<std::string>("--pmem_file", "-p", "/mnt/pmem2/pm");
    parser.add_option<size_t>("--pmem_size", "-z", 1024 * 1024 * 1024);
    parser.add_option("--server", "-s");
    parser.add_option<std::string>("--device", "-d", "mlx5_1");
    parser.add_option<int>("--ib_port", "-p", 1);
    parser.add_option<int>("--socket_port", "-P", 2333);
    parser.add_option<int>("--gid_idx", "-g", 2);
    parser.add_option<size_t>("--batch", "-b", 10);

    parser.parse(argc, argv);

    auto pmem_file = parser.get_as<std::string>("--pmem_file").value();
    auto pmem_size = parser.get_as<size_t>("--pmem_size").value();

    auto server = parser.get_as<std::string>("--server");
    auto is_server = !server.has_value();

    auto dev_name = parser.get_as<std::string>("--device").value();
    auto ib_port = parser.get_as<int>("--ib_port").value();
    auto socket_port = parser.get_as<int>("--socket_port").value();
    auto gid_idx = parser.get_as<int>("--gid_idx").value();

    auto [rdma_device, dstatus] = RDMADevice::make_rdma(dev_name, ib_port, gid_idx);
    if (!rdma_device) {
        std::cerr << "Failed to create RDMA, error code: " << decode_rdma_status(dstatus) << "\n";
        return -1;
    }


    if (is_server) {
        auto buf = set_up_pmem(pmem_file.c_str(), pmem_size);
        auto [rdma_ctx, cstatus] = rdma_device->open(buf, pmem_size, 12, RDMADevice::get_default_mr_access(),
                                                     *RDMADevice::get_default_qp_init_attr());
        if (!rdma_ctx) {
            std::cerr << "Failed to open RDMA device, error code: " << decode_rdma_status(cstatus) << "\n";
            return -1;
        }

        run_server(socket_port, rdma_ctx);
    } else {
        size_t num_thread = 2;
        std::thread threads[num_thread];
        std::mutex output_lock;
        std::vector<std::unique_ptr<RDMAContext>> contexts;

        for (size_t i = 0; i < num_thread; i++) {
            auto buf = new byte_t[1024];
            auto [rdma_ctx, cstatus] = rdma_device->open(buf, 1024, 12, RDMADevice::get_default_mr_access(),
                                                         *RDMADevice::get_default_qp_init_attr());
            if (!rdma_ctx) {
                std::cerr << "Failed to open RDMA device, error code: " << decode_rdma_status(cstatus) << "\n";
                return -1;
            }

            contexts.push_back(std::move(rdma_ctx));
        }

        for (size_t i = 0; i < num_thread; i++) {
            threads[i] = std::thread([&](int tid) {
                run_client(tid, contexts[i], pmem_size, output_lock, server.value());
            }, i);
        }

        for (auto &t : threads) {
            t.join();
        }
    }

    return 0;
}
