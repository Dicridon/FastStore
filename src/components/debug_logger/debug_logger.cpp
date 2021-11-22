#include "debug_logger.hpp"
namespace DebugLogger {
    auto Logger::log_info(const std::string &msg, bool ret) -> void {
        auto now = std::chrono::steady_clock::now();
        auto micros = std::chrono::duration_cast<std::chrono::microseconds>(now - start_time).count();
        fstream << "[[ Time: " << micros << ", " << "thread: " << std::this_thread::get_id() << " ]] -->> " << msg;
        if (ret)
            fstream << "\n";
    }

    auto MultithreadLogger::open_log(const std::string &log_file) -> bool {
        std::scoped_lock<std::mutex> _(m);
        if (loggers.find(std::this_thread::get_id()) != loggers.end()) {
            return true;
        }

        auto logger = Logger::make_logger(log_file);
        if (!logger) {
            return false;
        }

        logger->start_time = this->start_time;
        loggers.insert({std::this_thread::get_id(), std::move(logger)});
        return true;
    }

    auto MultithreadLogger::log_info(const std::string &msg) -> void {
        auto v = loggers.find(std::this_thread::get_id());
        if (v == loggers.end()) {
            throw std::runtime_error("Logging a non-exist log file\n");
        }
        v->second->log_info(msg);
    }

    auto MultithreadLogger::flush() -> void {
        auto v = loggers.find(std::this_thread::get_id());
        if (v == loggers.end()) {
            throw std::runtime_error("Logging a non-exist log file\n");
        }
        v->second->fstream.flush();
    }
}
