#pragma once

#include <string>

#include <nlohmann/json.hpp>

namespace serverless {

class StructuredLogger {
 public:
  static void init(const std::string& level);
  static void log_event(const std::string& event, const nlohmann::json& fields);
  static void info(const std::string& message);
  static void warn(const std::string& message);
  static void error(const std::string& message);
};

}  // namespace serverless
