#pragma once

#include <chrono>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "serverless/common/Error.hpp"

namespace serverless {

struct NodeRecord {
  std::string id;
  std::string host;
  int port{0};
  int cpu_capacity{1};
  int memory_mb{512};
  int running_workers{0};
  int available_workers{0};
  std::chrono::steady_clock::time_point last_heartbeat;
  bool healthy{true};
};

class NodeRegistry {
 public:
  Error register_node(const std::string& id, const std::string& host, int port, int cpu_capacity,
                      int memory_mb);
  Error heartbeat(const std::string& id, int running_workers, int available_workers);
  Error report_workers(const std::string& id, int running_workers, int available_workers);
  std::vector<NodeRecord> list_nodes() const;
  std::optional<NodeRecord> get_node(const std::string& id) const;
  void mark_unhealthy_nodes(int missed_threshold, std::chrono::milliseconds heartbeat_interval);
  std::vector<NodeRecord> healthy_nodes() const;

 private:
  mutable std::mutex mutex_;
  std::unordered_map<std::string, NodeRecord> nodes_;
};

}  // namespace serverless
