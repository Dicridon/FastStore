#ifndef __HILL__DEBUG_LOGGER__DEBUG_LOGGER__
#define __HILL__DEBUG_LOGGER__DEBUG_LOGGER__

#include <string>
#include <fstream>
#include <memory>
#include <chrono>
#include <thread>
#include <vector>
#include <mutex>
#include <unordered_map>
namespace DebugLogger {
    class MultithreadLogger;
    class Logger {
    public:
        friend class MultithreadLogger;
        Logger() = default;
        ~Logger() {
            fstream.flush();
            fstream.close();
        }
        Logger(const Logger &) = delete;
        Logger(Logger &&) = delete;
        auto operator=(const Logger &) = delete;
        auto operator=(Logger &&) = delete;

        static auto make_logger(const std::string &log_file) -> std::unique_ptr<Logger> {
            auto ret = std::make_unique<Logger>();
            ret->fstream.open(log_file);
            if (!ret->fstream.good()) {
                return nullptr;
            }
            ret->start_time = std::chrono::steady_clock::now();
            return ret;
        }

        auto log_info(const std::string &msg) -> void;
    private:
        std::ofstream fstream;
        std::chrono::time_point<std::chrono::steady_clock> start_time;
    };

    class MultithreadLogger {
    public:
        MultithreadLogger() = default;
        ~MultithreadLogger() = default;
        MultithreadLogger(const MultithreadLogger &) = delete;
        MultithreadLogger(MultithreadLogger &&) = delete;
        auto operator=(const MultithreadLogger &) = delete;
        auto operator=(MultithreadLogger &&) = delete;

        inline static auto make_logger() -> std::unique_ptr<MultithreadLogger> {
            auto ret =  std::make_unique<MultithreadLogger>();
            ret->start_time = std::chrono::steady_clock::now();
            return ret;
        }

        auto open_log(const std::string &log_file) -> bool;
        auto log_info(const std::string &msg) -> void;

    private:
        std::mutex m;
        std::unordered_map<std::thread::id, std::unique_ptr<Logger>> loggers;
        std::chrono::time_point<std::chrono::steady_clock> start_time;
    };
}
#endif
