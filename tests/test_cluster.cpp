#include "cluster/cluster.hpp"
#include "misc/misc.hpp"
#include "cmd_parser/cmd_parser.hpp"

#include <iostream>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

using namespace Hill::Cluster;
using namespace Hill::Misc;
auto test_serialization() -> void {
    ClusterMeta meta;
    meta.cluster.node_num = 2;
    meta.version = 4321;
    for (size_t i = 0; i < meta.cluster.node_num; i++) {
        meta.cluster.nodes[i].version = 1234;
        meta.cluster.nodes[i].node_id = i + 1;
        meta.cluster.nodes[i].total_pm = 0x12345678;
        meta.cluster.nodes[i].available_pm = 0x1234;
        meta.cluster.nodes[i].is_active = true;
        meta.cluster.nodes[i].addr.content[0] = 127;
        meta.cluster.nodes[i].addr.content[1] = 0;
        meta.cluster.nodes[i].addr.content[2] = 0;
        meta.cluster.nodes[i].addr.content[3] = i + 1;
    }

    meta.group.add_main("start", 1);
    meta.group.add_main("start start", 2);
    meta.group.infos[0].version = 9999;
    meta.group.infos[1].version = 6666;
    
    meta.dump();

    ClusterMeta meta2;
    auto buf = meta.serialize();
    meta2.deserialize(buf.get());
    meta2.dump();
}

auto test_network_serialization() -> void {
    std::thread server([&]() {
        ClusterMeta meta;
        meta.cluster.node_num = 2;
        meta.version = 4321;
        for (size_t i = 0; i < meta.cluster.node_num; i++) {
            meta.cluster.nodes[i].version = 1234;
            meta.cluster.nodes[i].node_id = i + 1;
            meta.cluster.nodes[i].total_pm = 0x12345678;
            meta.cluster.nodes[i].available_pm = 0x1234;
            meta.cluster.nodes[i].is_active = true;
            meta.cluster.nodes[i].addr.content[0] = 127;
            meta.cluster.nodes[i].addr.content[1] = 0;
            meta.cluster.nodes[i].addr.content[2] = 0;
            meta.cluster.nodes[i].addr.content[3] = i + 1;
        }
        meta.group.add_main("start", 1);
        meta.group.add_main("start start", 2);
        meta.group.infos[0].version = 9999;
        meta.group.infos[1].version = 6666;
        meta.dump();
        
        auto sock = socket_connect(true, 2333, nullptr);
        auto total = meta.total_size();
        write(sock, &total, sizeof(total));
        write(sock, meta.serialize().get(), total);
        shutdown(sock, 0);
    });

    std::thread client([&]() {
        ClusterMeta meta2;
        sleep(1);
        auto sock = socket_connect(false, 2333, "127.0.0.1");
        auto total = 0UL;
        read(sock, &total, sizeof(total));
        auto buf = std::make_unique<byte_t[]>(total);
        read(sock, buf.get(), total);
        meta2.deserialize(buf.get());
        meta2.dump();
        shutdown(sock, 0);
    });

    server.join();
    client.join();
}

auto test_file_parsing() -> void {
    auto n1 = Node::make_node("./node1.info");
    n1->dump();

    auto m1 = Monitor::make_monitor("./config.moni");
    m1->dump();
}

auto launch_node(bool is_monitor, const std::string &config) -> void {
    std::unique_ptr<Monitor> monitor;
    std::unique_ptr<Node> server;
    if (is_monitor) {
        monitor = Monitor::make_monitor(config);
        std::cout << ">> Monitor Info\n";
        monitor->dump();
        monitor->launch();
    } else {
        server = Node::make_node(config);
        std::cout << ">> Node Info\n";
        server->dump();
        server->launch();
    }

    // keep monitor and server valid
    pend();
}

auto test_keepalive(int argc, char *argv[]) -> void {
    CmdParser::Parser parser;
    parser.add_option<int>("--num", "-n", 0);
    parser.parse(argc, argv);

    std::cout << "\n>> keepalive test\n";
    auto ret = parser.get_as<int>("--num").value();
    switch(ret) {
    case 0:
        {
            std::thread monitor([&]() {
                launch_node(true, "./config.moni");
            });
            
            monitor.join();
            break;
        }
    case 1:
        {
            std::thread server1([&]() {
                launch_node(false, "./node1.info");
            });
            server1.join();
            break;
        }
    case 2:
        {
            std::thread server2([&]() {
                launch_node(false, "./node2.info");
            });
            server2.join();
            break;
        }
    default:
        break;
    }
    
}

using namespace CmdParser;
auto main(int argc, char *argv[]) -> int {
    // std::cout << "local serialization\n";
    // test_serialization();
    // std::cout << "\n>> network serialization\n";
    // test_network_serialization();
    test_keepalive(argc, argv);
}
