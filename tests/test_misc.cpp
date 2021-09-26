#include "misc/misc.hpp"
#include "cmd_parser/cmd_parser.hpp"

using namespace CmdParser;
using namespace Hill::Misc;
int main(int argc, char *argv[]) {
    Parser parser;
    parser.add_switch("--server", "-s", true);
    parser.parse(argc, argv);

    auto is_server = parser.get_as<bool>("--server").value();

    int sock = 0;
    if (is_server) {
        sock = make_socket(is_server, 2333);
        auto socket = accept_blocking(sock);
        size_t value = 0;        
        while(true) {
            write(socket, &value, sizeof(value));
            read(socket, &value, sizeof(value));
            std::cout << ">> Caught " << value << "\n";
            sleep(1);
        }
    } else {
        // sock = make_socket(is_server, 2333, "127.0.0.1");
        // auto socket = connect_socket(sock, 2333, "127.0.0.1");
        auto socket = socket_connect(false, 2333, "127.0.0.1");
        size_t value = 0;
        while(true) {
            read(socket, &value, sizeof(value));
            ++value;
            write(socket, &value, sizeof(value));
            std::cout << ">> Created: " << value << "\n";
            sleep(1);
        }
    }
}
