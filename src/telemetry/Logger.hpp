#pragma once

#include <memory>
#include <spdlog/spdlog.h>

namespace SpexronDPI::Telemetry {

class Logger {
public:

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    static void Init();
    static std::shared_ptr<spdlog::logger>& GetCoreLogger();

private:
    Logger() = default;
    static std::shared_ptr<spdlog::logger> s_CoreLogger;
};

} 

#define LOG_TRACE(...)    SpexronDPI::Telemetry::Logger::GetCoreLogger()->trace(__VA_ARGS__)
#define LOG_DEBUG(...)    SpexronDPI::Telemetry::Logger::GetCoreLogger()->debug(__VA_ARGS__)
#define LOG_INFO(...)     SpexronDPI::Telemetry::Logger::GetCoreLogger()->info(__VA_ARGS__)
#define LOG_WARN(...)     SpexronDPI::Telemetry::Logger::GetCoreLogger()->warn(__VA_ARGS__)
#define LOG_ERROR(...)    SpexronDPI::Telemetry::Logger::GetCoreLogger()->error(__VA_ARGS__)
#define LOG_CRITICAL(...) SpexronDPI::Telemetry::Logger::GetCoreLogger()->critical(__VA_ARGS__)
