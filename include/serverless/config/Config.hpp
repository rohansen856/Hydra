#pragma once

#include <string>

namespace serverless {

struct ServerConfig {
  std::string host{"0.0.0.0"};
  int port{8080};
};

struct SchedulerConfig {
  int queue_limit{10000};
  int queue_timeout_ms{3000};
};

struct ScalingConfig {
  int interval_ms{1000};
  double busy_threshold{0.70};
  int idle_timeout_ms{60000};
  int max_scale_step{4};
};

struct DefaultFunctionConfig {
  int timeout_ms{5000};
  int memory_mb{128};
  int max_concurrency{10};
};

struct StorageConfig {
  std::string db_path{"data/serverless.db"};
  std::string functions_dir{"data/functions"};
};

struct LimitsConfig {
  std::size_t max_request_bytes{1048576};
  std::size_t max_response_bytes{1048576};
};

struct LoggingConfig {
  std::string level{"info"};
};

struct NodeConfig {
  int heartbeat_interval_ms{5000};
  int missed_heartbeats_threshold{3};
};

struct Config {
  ServerConfig server;
  SchedulerConfig scheduler;
  ScalingConfig scaling;
  DefaultFunctionConfig default_function;
  StorageConfig storage;
  LimitsConfig limits;
  LoggingConfig logging;
  NodeConfig node;

  static Config load_from_file(const std::string& path);
  void validate() const;
};

}  // namespace serverless
