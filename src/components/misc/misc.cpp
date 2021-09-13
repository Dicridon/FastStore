#include "misc.hpp"

namespace Hill {
    namespace Misc {
        int make_socket(bool is_server, int socket_port, const char *server) {
            struct sockaddr_in seraddr;
            auto sockfd = socket(AF_INET, SOCK_STREAM, 0);
            if (sockfd == -1) {
                std::cout << ">> Error:" << "can not open socket\n";
                exit(-1);
            }

            if (!is_server) {
                return sockfd;
            }

            memset(&seraddr, 0, sizeof(struct sockaddr));
            seraddr.sin_family = AF_INET;
            seraddr.sin_port = htons(socket_port);
            seraddr.sin_addr.s_addr = htonl(INADDR_ANY);
            
            if (bind(sockfd, (struct sockaddr *)&seraddr, sizeof(struct sockaddr)) == -1) {
                std::cout << ">> Error: " << "can not bind socket\n";
                exit(-1);
            }

            if (listen(sockfd, 1) == -1) {
                std::cout << ">> Error: " << "can not listen socket\n";
                exit(-1);
            }
            
            return sockfd;
        }

        int connect_socket(int sockfd, int socket_port, const char *server) {
            struct sockaddr_in seraddr;
            memset(&seraddr, 0, sizeof(struct sockaddr));
            seraddr.sin_family = AF_INET;
            seraddr.sin_port = htons(socket_port);
            inet_pton(AF_INET, server, &seraddr.sin_addr);
        
            if (connect(sockfd, (struct sockaddr *)&seraddr, sizeof(seraddr)) == -1) {
                return -1;
            }
            return sockfd;
        }

        int accept_blocking(int sockfd) {
            return accept(sockfd, NULL, 0);
        }

        int accept_nonblocking(int sockfd) {
            return accept4(sockfd, NULL, 0, SOCK_NONBLOCK);
        }

        int socket_connect(bool is_server, int socket_port, const char *server) {
            auto sockfd = make_socket(is_server, socket_port, server);
            if (!is_server) {
                return connect_socket(sockfd, socket_port, server);
            }
            
            auto ret = accept_blocking(sockfd);
            if (ret == -1) {
                std::cout << ">> Error: " << "accepting connection failed\n";
                exit(-1);
            }
            return ret;
        }
    }
}
