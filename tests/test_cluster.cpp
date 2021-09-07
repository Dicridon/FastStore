#include "cluster/cluster.hpp"

#include <iostream>
#include <thread>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

using namespace Hill::Cluster;
auto test_serialization() -> void {
    ClusterMeta meta;
    meta.cluster.node_num = 2;
    for (size_t i = 0; i < meta.cluster.node_num; i++) {
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
    meta.dump();

    ClusterMeta meta2;
    meta2.deserialize(meta.serialize().get());
    meta2.dump();
}

int socket_connect(bool is_server, int socket_port) {
    struct sockaddr_in seraddr;
    auto sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        std::cout << ">> " << "can not open socket\n";
        exit(-1);
    }

    memset(&seraddr, 0, sizeof(struct sockaddr));
    seraddr.sin_family = AF_INET;
    seraddr.sin_port = htons(socket_port);

    if (is_server) {
        seraddr.sin_addr.s_addr = htonl(INADDR_ANY);
            
        if (bind(sockfd, (struct sockaddr *)&seraddr, sizeof(struct sockaddr)) == -1) {
            std::cout << ">> " << "can not bind socket\n";
            exit(-1);
        }

        if (listen(sockfd, 1) == -1) {
            std::cout << ">> " << "can not listen socket\n";
            exit(-1);
        }

        auto ret = accept(sockfd, NULL, 0);
        if (ret == -1) {
            std::cout << ">> " << "accepting connection failed\n";
            exit(-1);
        }
        return ret;
    } else {
        inet_pton(AF_INET, "127.0.0.1", &seraddr.sin_addr);
        
        if (connect(sockfd, (struct sockaddr *)&seraddr, sizeof(seraddr)) == -1) {
            std::cout << ">> " << "connecting to server failed\n";
            exit(-1);
        }
        return sockfd;
    }
}

auto test_network_serialization() -> void {
    std::thread server([&]() {
        ClusterMeta meta;
        meta.cluster.node_num = 2;
        for (size_t i = 0; i < meta.cluster.node_num; i++) {
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
        meta.dump();
        
        auto sock = socket_connect(true, 2333);
        write(sock, meta.serialize().get(), 2160);
    });

    std::thread client([&]() {
        ClusterMeta meta2;
        auto sock = socket_connect(false, 2333);
        auto buf = new uint8_t[2160];
        read(sock, buf, 2160);
        meta2.deserialize(buf);
        meta2.dump();
    });

    server.join();
    client.join();
}


auto main() -> int {
    std::cout << "local serialization\n";
    test_serialization();
    std::cout << "\n>> network serialization\n";
    test_network_serialization();
}
