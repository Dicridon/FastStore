#ifndef __HILL__MISC__MISC__
#define __HILL__MISC__MISC__
#define UNUSED(x) (void)(x)
#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fstream>
#include <optional>
#include <sstream>

namespace Hill {
    namespace Misc {
        /*
         * Socket related functions. Here I don't use trailing return type because
         * these functions manipulate low-level OS interfaces.
         */
        // make a new socket file descriptor and listen
        int make_socket(bool is_server, int socket_port);
        int connect_socket(int sockfd, int socket_port, const char *server);
        int accept_blocking(int sockfd);
        int accept_nonblocking(int sockfd);
        
        /*
         * This function serves as a shortcut for establishing blocking socket connection
         * if is_server == false, supply an IP
         */
        int socket_connect(bool is_server, int socket_port, const char *server = nullptr);
        bool syncop(int sockfd);

        // Don't know why I'm writing this, perhaps because pend(); is shorter than while(true);
        auto pend() -> void;

        auto file_as_string(const std::string &file_name) -> std::optional<std::string>;

        auto check_socket_read_write(ssize_t ret, bool is_read = true) -> void;
    }
}
#endif
