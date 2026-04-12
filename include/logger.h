#pragma once
#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/async.h"
#include "spdlog/common.h"
#include <string>
#include <memory>


namespace NEXUS
{
    class Logger
    {
    public:
        static Logger &instance();

        Logger(const Logger &) = delete;
        Logger &operator=(const Logger &) = delete;

        void initialize();

        template <typename... Args>
        void info(const char *file, int line, const char *func, fmt::format_string<Args...> fmt, Args &&...args);

        template <typename... Args>
        void debug(const char *file, int line, const char *func, fmt::format_string<Args...> fmt, Args &&...args);

        template <typename... Args>
        void warn(const char *file, int line, const char *func, fmt::format_string<Args...> fmt, Args &&...args);

        template <typename... Args>
        void error(const char *file, int line, const char *func, fmt::format_string<Args...> fmt, Args &&...args);

        std::shared_ptr<spdlog::logger> get_logger() const { return logger_; }

    private:
        Logger();
        ~Logger();

        std::shared_ptr<spdlog::logger> logger_;
        bool initialized_;
    };

    template <typename... Args>
    inline void Logger::info(const char *file, int line, const char *func, fmt::format_string<Args...> fmt, Args &&...args)
    {
        if (logger_ && logger_->should_log(spdlog::level::info))
        {
            auto formatted_msg = fmt::format(fmt, std::forward<Args>(args)...);
            logger_->log(spdlog::source_loc{file, line, func}, spdlog::level::info, formatted_msg);
        }
    }

    template <typename... Args>
    inline void Logger::debug(const char *file, int line, const char *func, fmt::format_string<Args...> fmt, Args &&...args)
    {
        if (logger_ && logger_->should_log(spdlog::level::debug))
        {
            auto formatted_msg = fmt::format(fmt, std::forward<Args>(args)...);
            logger_->log(spdlog::source_loc{file, line, func}, spdlog::level::debug, formatted_msg);
        }
    }

    template <typename... Args>
    inline void Logger::warn(const char *file, int line, const char *func, fmt::format_string<Args...> fmt, Args &&...args)
    {
        if (logger_ && logger_->should_log(spdlog::level::warn))
        {
            auto formatted_msg = fmt::format(fmt, std::forward<Args>(args)...);
            logger_->log(spdlog::source_loc{file, line, func}, spdlog::level::warn, formatted_msg);
        }
    }

    template <typename... Args>
    inline void Logger::error(const char *file, int line, const char *func, fmt::format_string<Args...> fmt, Args &&...args)
    {
        if (logger_ && logger_->should_log(spdlog::level::err))
        {
            auto formatted_msg = fmt::format(fmt, std::forward<Args>(args)...);
            logger_->log(spdlog::source_loc{file, line, func}, spdlog::level::err, formatted_msg);
        }
    }

} // namespace NEXUS

#define LOG_INFO(...) ::NEXUS::Logger::instance().info(__FILE__, __LINE__, SPDLOG_FUNCTION, __VA_ARGS__)
#define LOG_DEBUG(...) ::NEXUS::Logger::instance().debug(__FILE__, __LINE__, SPDLOG_FUNCTION, __VA_ARGS__)
#define LOG_WARN(...) ::NEXUS::Logger::instance().warn(__FILE__, __LINE__, SPDLOG_FUNCTION, __VA_ARGS__)
#define LOG_ERROR(...) ::NEXUS::Logger::instance().error(__FILE__, __LINE__, SPDLOG_FUNCTION, __VA_ARGS__)
