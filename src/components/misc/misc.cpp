#include "misc.hpp"

namespace Hill {
    namespace Misc {
        int socket_connect(bool is_server, int socket_port, const char *server) {
            struct sockaddr_in seraddr;
            auto sockfd = socket(AF_INET, SOCK_STREAM, 0);
            if (sockfd == -1) {
                std::cout << ">> Error:" << "can not open socket\n";
                exit(-1);
            }

            memset(&seraddr, 0, sizeof(struct sockaddr));
            seraddr.sin_family = AF_INET;
            seraddr.sin_port = htons(socket_port);

            if (is_server) {
                seraddr.sin_addr.s_addr = htonl(INADDR_ANY);
            
                if (bind(sockfd, (struct sockaddr *)&seraddr, sizeof(struct sockaddr)) == -1) {
                    std::cout << ">> Error: " << "can not bind socket\n";
                    exit(-1);
                }

                if (listen(sockfd, 1) == -1) {
                    std::cout << ">> Error: " << "can not listen socket\n";
                    exit(-1);
                }

                auto ret = accept(sockfd, NULL, 0);
                if (ret == -1) {
                    std::cout << ">> Error: " << "accepting connection failed\n";
                    exit(-1);
                }
                return ret;
            } else {
                inet_pton(AF_INET, server, &seraddr.sin_addr);
        
                if (connect(sockfd, (struct sockaddr *)&seraddr, sizeof(seraddr)) == -1) {
                    std::cout << ">> Error: " << "connecting to server failed\n";
                    exit(-1);
                }
                return sockfd;
            }
        }

    }
}
