#ifndef __HILL__DEBUG_LOGGER__DEBUG_LOGGER__
#define __HILL__DEBUG_LOGGER__DEBUG_LOGGER__

#include <string>
#include <fstream>
namespace DebugLogger {
    class Logger {
    public:
        Logger() = default;
        Logger(const Logger &) = delete;
        Logger(Logger &&) = delete;
        auto operator=(const Logger &) = delete;
        auto operator=(Logger &&) = delete;

        static auto make_logger(const std::string &log_file)
    private:
    };
}
#endif
