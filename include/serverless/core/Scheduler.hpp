#pragma once

#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <boost/asio/io_context.hpp>
#include <boost/asio/strand.hpp>
#include <nlohmann/json.hpp>

#include "serverless/common/Error.hpp"
#include "serverless/config/Config.hpp"
#include "serverless/core/InvocationManager.hpp"
#include "serverless/storage/SqliteStore.hpp"

namespace serverless {
class FunctionRegistry;
class WorkerManager;
class Autoscaler;
}
#include "serverless/runtime/FunctionRunner.hpp"

namespace serverless {

class Scheduler {
 public:
  Scheduler(boost::asio::io_context& io, const Config& config,
            std::shared_ptr<FunctionRegistry> registry,
            std::shared_ptr<InvocationManager> invocations,
            std::shared_ptr<WorkerManager> workers,
            std::shared_ptr<Autoscaler> autoscaler);

  void submit(const std::string& function_name, const nlohmann::json& payload,
              std::function<void(Error, nlohmann::json)> callback);

  void on_worker_idle(const std::string& worker_id);
  void on_worker_failed(const std::string& worker_id);
  void dispatch_pending();

  std::size_t queue_depth(const std::string& function_id = {}) const;
  std::size_t total_queue_depth() const;
  bool ready() const { return ready_; }
  void set_ready(bool ready) { ready_ = ready; }

  struct Stats {
    std::size_t queue_depth{0};
    std::size_t workers{0};
    std::size_t idle_workers{0};
    std::size_t busy_workers{0};
  };
  Stats stats() const;

 private:
  boost::asio::io_context& io_;
  Config config_;
  std::shared_ptr<FunctionRegistry> registry_;
  std::shared_ptr<InvocationManager> invocations_;
  std::shared_ptr<WorkerManager> workers_;
  std::shared_ptr<Autoscaler> autoscaler_;
  boost::asio::strand<boost::asio::io_context::executor_type> strand_;

  mutable std::mutex mutex_;
  bool ready_{false};
  std::unordered_map<std::string, std::deque<PendingInvocation>> queues_;
  std::size_t total_queued_{0};

  void try_dispatch_locked(const FunctionRecord& fn);
  void complete_invocation(PendingInvocation inv, Error err, nlohmann::json result,
                           double duration_ms, bool cold_start);
  void check_queue_timeouts();
};

}  // namespace serverless
