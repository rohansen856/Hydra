#include "serverless/core/NodeRegistry.hpp"

#include "serverless/common/Clock.hpp"

namespace serverless {

Error NodeRegistry::register_node(const std::string& id, const std::string& host, int port,
                                  int cpu_capacity, int memory_mb) {
  std::lock_guard lock(mutex_);
  NodeRecord rec;
  rec.id = id;
  rec.host = host;
  rec.port = port;
  rec.cpu_capacity = cpu_capacity;
  rec.memory_mb = memory_mb;
  rec.last_heartbeat = Clock::steady_now();
  rec.healthy = true;
  nodes_[id] = rec;
  return Error::success();
}

Error NodeRegistry::heartbeat(const std::string& id, int running_workers, int available_workers) {
  std::lock_guard lock(mutex_);
  auto it = nodes_.find(id);
  if (it == nodes_.end()) {
    return Error::make(ErrorCode::NodeNotFound, "Node not found");
  }
  it->second.last_heartbeat = Clock::steady_now();
  it->second.running_workers = running_workers;
  it->second.available_workers = available_workers;
  it->second.healthy = true;
  return Error::success();
}

Error NodeRegistry::report_workers(const std::string& id, int running_workers,
                                   int available_workers) {
  return heartbeat(id, running_workers, available_workers);
}

std::vector<NodeRecord> NodeRegistry::list_nodes() const {
  std::lock_guard lock(mutex_);
  std::vector<NodeRecord> out;
  for (const auto& [id, node] : nodes_) {
    out.push_back(node);
  }
  return out;
}

std::optional<NodeRecord> NodeRegistry::get_node(const std::string& id) const {
  std::lock_guard lock(mutex_);
  auto it = nodes_.find(id);
  if (it == nodes_.end()) {
    return std::nullopt;
  }
  return it->second;
}

void NodeRegistry::mark_unhealthy_nodes(int missed_threshold,
                                        std::chrono::milliseconds heartbeat_interval) {
  std::lock_guard lock(mutex_);
  const auto now = Clock::steady_now();
  for (auto& [id, node] : nodes_) {
    const auto elapsed =
        std::chrono::duration_cast<std::chrono::milliseconds>(now - node.last_heartbeat);
    if (elapsed > heartbeat_interval * missed_threshold) {
      node.healthy = false;
    }
  }
}

std::vector<NodeRecord> NodeRegistry::healthy_nodes() const {
  std::lock_guard lock(mutex_);
  std::vector<NodeRecord> out;
  for (const auto& [id, node] : nodes_) {
    if (node.healthy) {
      out.push_back(node);
    }
  }
  return out;
}

}  // namespace serverless
