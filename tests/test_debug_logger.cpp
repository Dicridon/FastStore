#include "debug_logger/debug_logger.hpp"
#include "cmd_parser/cmd_parser.hpp"

using namespace DebugLogger;
using namespace CmdParser;
auto main(int argc, char *argv[]) -> int {
    Parser parser;
    parser.add_option<int>("--threads", "-t", 2);
    parser.parse(argc, argv);

    auto threads = parser.get_as<int>("--threads").value();

    auto logger = MultithreadLogger::make_logger();
    
    std::vector<std::thread> testers;
    for (int i = 0; i < threads; i++) {
        testers.emplace_back([&](int tid) {
            logger->open_log("thread_" + std::to_string(tid) + ".log");
            logger->log_info("Message from thread " + std::to_string(tid));
        }, i);
    }

    for (auto &t : testers) {
        t.join();
    }
}
