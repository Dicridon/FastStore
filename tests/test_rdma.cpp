#include "rdma/rdma.hpp"
#include "coloring/coloring.hpp"
#include "cmd_parser/cmd_parser.hpp"
#include "misc/misc.hpp"

#include <infiniband/verbs.h>
#include <iostream>
#include <sstream>
#include <chrono>


#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

using namespace Hill;
using namespace Hill::Misc;
using namespace Hill::RDMAUtil;
using namespace std::chrono;

const ColorizedString error_msg("Error: ", Colors::Red);
const ColorizedString warning_msg("Warning: ", Colors::Magenta);

void show_connection_info(const connection_certificate &c, bool is_local = true) {
    std::string from = is_local ? "local" : "remote";
    std::cout << ">> reporting info" << "(" << from << "):\n";
    std::cout << "  [[ address: " << c.addr << "\n";
    std::cout << "  [[ rkey: " << c.rkey << "\n";
    std::cout << "  [[ lid: " << c.lid << "\n";
    std::cout << "  [[ qp num: " << c.qp_num << "\n";
    std::cout << "  [[ gid: ";
    const char *pat = ":%02x";
    for (int i = 0; i < 16; i++) {
        printf(pat + (i == 0), c.gid[i]);
    }
    std::cout << "\n";
}


using namespace CmdParser;
int main(int argc, char *argv[]) {
    Parser parser;

    parser.add_option<std::string>("--device", "-d", "mlx5_1");
    parser.add_option<int>("--ib_port", "-p", 1);
    parser.add_option<int>("--socket_port", "-P", 2333);
    parser.add_option<int>("--gid_idx", "-g", 2);
    parser.add_option<size_t>("--batch", "-b", 10);
    parser.add_switch("--is_server", "-s", true);
    parser.add_switch("--debug", "-D", true);

    parser.parse(argc, argv);
    // parser.Parse(argv, argv + argc);
    auto dev_name = parser.get_as<std::string>("--device").value();
    auto ib_port = parser.get_as<int>("--ib_port").value();
    auto socket_port = parser.get_as<int>("--socket_port").value();
    auto gid_idx = parser.get_as<int>("--gid_idx").value();    
    auto is_server = parser.get_as<bool>("--is_server").value();
    auto batch = parser.get_as<size_t>("--batch").value();
    auto debug = parser.get_as<bool>("--debug").value();
    
    auto [rdma_device, dstatus] = RDMADevice::make_rdma(dev_name, ib_port, gid_idx);
    if (!rdma_device) {
        std::cerr << "Failed to create RDMA, error code: " << decode_rdma_status(dstatus) << "\n";
        return -1;
    }

    size_t registered = 1024 * 1024 * 1024;
    auto buf = new byte_t[registered];
    auto sockfd = socket_connect(is_server, socket_port, "127.0.0.1");
    auto [rdma_ctx, cstatus] = rdma_device->open(buf, registered, 12, RDMADevice::get_default_mr_access(),
                                                 *RDMADevice::get_default_qp_init_attr());
    if (!rdma_ctx) {
        std::cerr << "Failed to open RDMA device, error code: " << decode_rdma_status(cstatus) << "\n";
        return -1;
    }
    
    if (rdma_ctx->default_connect(sockfd) != 0) {
        std::cerr << "Default RDMA connection failed\n";
        return -1;
    }

    syncop(sockfd);
    std::vector<std::string> workload;
    auto start = (1UL << 63) + (1UL << 62);
    for (auto i = 0UL; i < batch; i++) {
        workload.emplace_back(std::to_string(start - i));
    }

    auto offset = 0UL;
    if (!is_server) {
        auto s = steady_clock::now();
        for (auto &s : workload) {
            rdma_ctx->post_write((uint8_t *)s.c_str(), s.size(), offset);
            rdma_ctx->poll_completion_once();
            offset += s.size();
        }
        auto e = steady_clock::now();
        auto throughput = double(batch) / duration_cast<milliseconds>(e - s).count();
        std::cout << "Throughput of write is " << throughput * 1000 << " OPS";
        syncop(sockfd);
    } else {
        syncop(sockfd);
    }

    offset = 0;
    if (!is_server) {
        auto s = steady_clock::now();
        for (auto &s : workload) {
            rdma_ctx->post_read(s.size(), offset);
            rdma_ctx->poll_completion_once();
            if (debug) {
                for (auto i = 0UL; i < s.size(); i++) {
                    std::cout << rdma_ctx->get_char_buf()[i];
                }
                std::cout << "\n";
            }
            offset += s.size();
        }
        auto e = steady_clock::now();
        auto throughput = double(batch) / duration_cast<milliseconds>(e - s).count();
        std::cout << "Throughput of read is " << throughput * 1000 << " OPS";
        syncop(sockfd);
    } else {
        syncop(sockfd);
    }
    
    close(sockfd);
    return 0;
}
