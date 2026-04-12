#include "logger.h"
#include <iostream>
#include <filesystem>
#include <vector>
#include <string>

#define DEFAULT_LOG_DIR "logs"
#define DEFAULT_LOG_PATH "logs/nexus.log"

namespace NEXUS
{

    Logger::Logger() : logger_(nullptr), initialized_(false)
    {
        initialize();
    }

    Logger::~Logger()
    {
    }

    Logger &Logger::instance()
    {
        static Logger instance;
        return instance;
    }

    void Logger::initialize()
    {
        if (initialized_)
        {
            return;
        }

        try
        {
            auto existing_logger = spdlog::get("main_logger");
            if (existing_logger != nullptr)
            {
                this->logger_ = existing_logger;
                this->initialized_ = true;
                return;
            }

            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            console_sink->set_level(spdlog::level::info);

            std::string log_dir = DEFAULT_LOG_DIR;
            std::string log_file_path = DEFAULT_LOG_PATH;

            std::error_code ec;
            if (!std::filesystem::exists(log_dir, ec))
            {
                std::filesystem::create_directories(log_dir, ec);
                if (ec)
                {
                    std::cerr << "Warning: Could not create log directory: " << ec.message() << std::endl;
                }
            }

            const std::size_t max_file_size = 10 * 1024 * 1024;
            const std::size_t max_files = 3;

            auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                log_file_path, max_file_size, max_files);
            file_sink->set_level(spdlog::level::trace);

            std::vector<spdlog::sink_ptr> sinks{console_sink, file_sink};

            spdlog::init_thread_pool(8192, 1);

            this->logger_ = std::make_shared<spdlog::async_logger>(
                "main_logger",
                sinks.begin(),
                sinks.end(),
                spdlog::thread_pool(),
                spdlog::async_overflow_policy::block);

            spdlog::register_logger(this->logger_);
            spdlog::set_default_logger(this->logger_);

            this->logger_->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%s:%#] %v");
            this->logger_->set_level(spdlog::level::trace);

            this->initialized_ = true;

            this->logger_->info("Logger initialized successfully. Path: {}", log_file_path);
        }
        catch (const spdlog::spdlog_ex &ex)
        {
            std::cerr << "Logger initialization failed (spdlog): " << ex.what() << std::endl;
            this->initialized_ = false;
        }
        catch (const std::exception &ex)
        {
            std::cerr << "Logger initialization failed (std): " << ex.what() << std::endl;
            this->initialized_ = false;
        }
    }

} // namespace NEXUS
