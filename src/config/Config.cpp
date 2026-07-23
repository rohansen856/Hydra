#include "serverless/config/Config.hpp"

#include <fstream>
#include <stdexcept>

#include <yaml-cpp/yaml.h>

namespace serverless {

Config Config::load_from_file(const std::string& path) {
  Config cfg;
  YAML::Node root = YAML::LoadFile(path);
  if (auto s = root["server"]) {
    cfg.server.host = s["host"].as<std::string>(cfg.server.host);
    cfg.server.port = s["port"].as<int>(cfg.server.port);
  }
  if (auto s = root["scheduler"]) {
    cfg.scheduler.queue_limit = s["queue_limit"].as<int>(cfg.scheduler.queue_limit);
    cfg.scheduler.queue_timeout_ms = s["queue_timeout_ms"].as<int>(cfg.scheduler.queue_timeout_ms);
  }
  if (auto s = root["scaling"]) {
    cfg.scaling.interval_ms = s["interval_ms"].as<int>(cfg.scaling.interval_ms);
    cfg.scaling.busy_threshold = s["busy_threshold"].as<double>(cfg.scaling.busy_threshold);
    cfg.scaling.idle_timeout_ms = s["idle_timeout_ms"].as<int>(cfg.scaling.idle_timeout_ms);
    cfg.scaling.max_scale_step = s["max_scale_step"].as<int>(cfg.scaling.max_scale_step);
  }
  if (auto s = root["default_function"]) {
    cfg.default_function.timeout_ms = s["timeout_ms"].as<int>(cfg.default_function.timeout_ms);
    cfg.default_function.memory_mb = s["memory_mb"].as<int>(cfg.default_function.memory_mb);
    cfg.default_function.max_concurrency =
        s["max_concurrency"].as<int>(cfg.default_function.max_concurrency);
  }
  if (auto s = root["storage"]) {
    cfg.storage.db_path = s["db_path"].as<std::string>(cfg.storage.db_path);
    cfg.storage.functions_dir = s["functions_dir"].as<std::string>(cfg.storage.functions_dir);
  }
  if (auto s = root["limits"]) {
    cfg.limits.max_request_bytes = s["max_request_bytes"].as<std::size_t>(cfg.limits.max_request_bytes);
    cfg.limits.max_response_bytes =
        s["max_response_bytes"].as<std::size_t>(cfg.limits.max_response_bytes);
  }
  if (auto s = root["logging"]) {
    cfg.logging.level = s["level"].as<std::string>(cfg.logging.level);
  }
  if (auto s = root["node"]) {
    cfg.node.heartbeat_interval_ms = s["heartbeat_interval_ms"].as<int>(cfg.node.heartbeat_interval_ms);
    cfg.node.missed_heartbeats_threshold =
        s["missed_heartbeats_threshold"].as<int>(cfg.node.missed_heartbeats_threshold);
  }
  cfg.validate();
  return cfg;
}

void Config::validate() const {
  if (server.port <= 0 || server.port > 65535) {
    throw std::runtime_error("Invalid server port");
  }
  if (scheduler.queue_limit <= 0) {
    throw std::runtime_error("scheduler.queue_limit must be positive");
  }
  if (scaling.busy_threshold < 0.0 || scaling.busy_threshold > 1.0) {
    throw std::runtime_error("scaling.busy_threshold must be between 0 and 1");
  }
  if (default_function.timeout_ms <= 0) {
    throw std::runtime_error("default_function.timeout_ms must be positive");
  }
}

}  // namespace serverless
