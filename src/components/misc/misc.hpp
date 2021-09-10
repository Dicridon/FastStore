#ifndef __HILL__MISC__MISC__
#define __HILL__MISC__MISC__
#define UNUSED(x) (void)(x)
#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>


namespace Hill {
    namespace Misc {
        // if is_server == false, supply an IP
        int socket_connect(bool is_server, int socket_port, const char *server = nullptr);
    }
}
#endif
