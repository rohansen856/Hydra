#include "serverless/observability/StructuredLogger.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace serverless {

void StructuredLogger::init(const std::string& level) {
  auto logger = spdlog::stdout_color_mt("serverless");
  if (level == "debug") {
    spdlog::set_level(spdlog::level::debug);
  } else if (level == "warn") {
    spdlog::set_level(spdlog::level::warn);
  } else if (level == "error") {
    spdlog::set_level(spdlog::level::err);
  } else {
    spdlog::set_level(spdlog::level::info);
  }
}

void StructuredLogger::log_event(const std::string& event, const nlohmann::json& fields) {
  nlohmann::json payload = fields;
  payload["event"] = event;
  payload["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                             std::chrono::system_clock::now().time_since_epoch())
                             .count();
  spdlog::info("{}", payload.dump());
}

void StructuredLogger::info(const std::string& message) { spdlog::info("{}", message); }
void StructuredLogger::warn(const std::string& message) { spdlog::warn("{}", message); }
void StructuredLogger::error(const std::string& message) { spdlog::error("{}", message); }

}  // namespace serverless
