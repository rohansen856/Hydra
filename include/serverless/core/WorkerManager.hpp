#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>

#include "serverless/config/Config.hpp"
#include "serverless/runtime/FunctionRunner.hpp"
#include "serverless/runtime/Worker.hpp"
#include "serverless/runtime/WorkerProcess.hpp"
#include "serverless/core/FunctionRegistry.hpp"

namespace serverless {

struct WorkerHandle {
  WorkerInfo info;
  std::unique_ptr<LocalWorkerConnection> local;
};

class WorkerManager {
 public:
  using WorkerEventCallback = std::function<void(const std::string& worker_id)>;

  WorkerManager(boost::asio::io_context& io, const Config& config,
                std::shared_ptr<FunctionRegistry> registry,
                std::shared_ptr<FunctionRunner> runner);

  void set_callbacks(WorkerEventCallback on_idle, WorkerEventCallback on_failed);

  void ensure_min_workers(const FunctionRecord& fn);
  std::optional<std::string> acquire_idle_worker(const FunctionRecord& fn);
  void start_worker(const FunctionRecord& fn, bool cold_start_metric = false);
  void release_worker(const std::string& worker_id, bool failed);
  void terminate_idle_workers(const FunctionRecord& fn, int count);
  void monitor_workers();

  std::vector<WorkerInfo> list_workers() const;
  WorkerInfo* find_worker(const std::string& worker_id);
  void invoke_on_worker(const std::string& worker_id, const FunctionRecord& fn,
                        const std::string& request_id, const nlohmann::json& payload,
                        std::function<void(Error, nlohmann::json, double)> callback);
  int worker_count(const std::string& function_id) const;
  int idle_count(const std::string& function_id) const;
  int busy_count(const std::string& function_id) const;

  void register_remote_worker(const std::string& node_id, const std::string& worker_id,
                              const std::string& function_id);
  void unregister_node_workers(const std::string& node_id);

 private:
  boost::asio::io_context& io_;
  Config config_;
  std::shared_ptr<FunctionRegistry> registry_;
  std::shared_ptr<FunctionRunner> runner_;
  boost::asio::steady_timer monitor_timer_;

  mutable std::mutex mutex_;
  std::unordered_map<std::string, WorkerHandle> workers_;
  WorkerEventCallback on_idle_;
  WorkerEventCallback on_failed_;

  void assign_local_worker(const FunctionRecord& fn, WorkerHandle& handle);
  void on_local_worker_ready(const std::string& worker_id);
  void on_local_worker_done(const std::string& worker_id, Error err);
  void schedule_monitor();
  void handle_worker_exit(const std::string& worker_id, bool failed);
};

}  // namespace serverless
