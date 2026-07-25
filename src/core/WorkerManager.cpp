#include "serverless/core/WorkerManager.hpp"

#include "serverless/common/Clock.hpp"
#include "serverless/common/Id.hpp"
#include "serverless/observability/Metrics.hpp"
#include "serverless/observability/StructuredLogger.hpp"

namespace serverless {

WorkerManager::WorkerManager(boost::asio::io_context& io, const Config& config,
                             std::shared_ptr<FunctionRegistry> registry,
                             std::shared_ptr<FunctionRunner> runner)
    : io_(io),
      config_(config),
      registry_(std::move(registry)),
      runner_(std::move(runner)),
      monitor_timer_(io) {}

void WorkerManager::set_callbacks(WorkerEventCallback on_idle, WorkerEventCallback on_failed) {
  on_idle_ = std::move(on_idle);
  on_failed_ = std::move(on_failed);
}

void WorkerManager::ensure_min_workers(const FunctionRecord& fn) {
  std::lock_guard lock(mutex_);
  int count = 0;
  for (const auto& [id, handle] : workers_) {
    if (handle.info.function_id == fn.id &&
        handle.info.state != WorkerState::Terminated &&
        handle.info.state != WorkerState::Failed) {
      count++;
    }
  }
  while (count < fn.min_workers) {
    start_worker(fn, true);
    count++;
  }
}

void WorkerManager::start_worker(const FunctionRecord& fn, bool cold_start_metric) {
  WorkerHandle handle;
  handle.info.id = generate_worker_id();
  handle.info.function_id = fn.id;
  handle.info.state = WorkerState::Starting;
  handle.info.started_at = Clock::steady_now();
  handle.info.idle_since = handle.info.started_at;
  handle.info.pid = -1;
  handle.local = std::make_unique<LocalWorkerConnection>(io_, -1, -1, runner_);

  WorkerStateMachine sm(handle.info.state);
  sm.transition(WorkerState::Idle);
  handle.info.state = sm.state();
  handle.info.idle_since = Clock::steady_now();

  const auto worker_id = handle.info.id;
  workers_[worker_id] = std::move(handle);
  Metrics::instance().inc_counter("worker_start_total", {{"function", fn.id}});
  if (cold_start_metric) {
    Metrics::instance().inc_counter("cold_start_total", {{"function", fn.id}});
  }
  Metrics::instance().set_gauge("worker_count", static_cast<double>(workers_.size()));
  StructuredLogger::info("Worker started: " + worker_id);
}

std::optional<std::string> WorkerManager::acquire_idle_worker(const FunctionRecord& fn) {
  std::lock_guard lock(mutex_);
  std::optional<std::string> chosen;
  std::chrono::steady_clock::time_point oldest = std::chrono::steady_clock::time_point::max();
  for (auto& [id, handle] : workers_) {
    if (handle.info.function_id != fn.id || handle.info.state != WorkerState::Idle) {
      continue;
    }
    if (handle.info.idle_since <= oldest) {
      oldest = handle.info.idle_since;
      chosen = id;
    }
  }
  if (!chosen) {
    return std::nullopt;
  }
  WorkerStateMachine sm(workers_[*chosen].info.state);
  sm.transition(WorkerState::Running);
  workers_[*chosen].info.state = sm.state();
  return chosen;
}

void WorkerManager::release_worker(const std::string& worker_id, bool failed) {
  WorkerEventCallback cb;
  {
    std::lock_guard lock(mutex_);
    auto it = workers_.find(worker_id);
    if (it == workers_.end()) {
      return;
    }
    if (failed) {
      WorkerStateMachine sm(it->second.info.state);
      sm.transition(WorkerState::Failed);
      it->second.info.state = sm.state();
      Metrics::instance().inc_counter("worker_crash_total",
                                      {{"function", it->second.info.function_id}});
      cb = on_failed_;
    } else {
      WorkerStateMachine sm(it->second.info.state);
      sm.transition(WorkerState::Idle);
      it->second.info.state = sm.state();
      it->second.info.idle_since = Clock::steady_now();
      cb = on_idle_;
    }
  }
  if (cb) {
    cb(worker_id);
  }
}

void WorkerManager::terminate_idle_workers(const FunctionRecord& fn, int count) {
  std::vector<std::string> victims;
  {
    std::lock_guard lock(mutex_);
    for (auto& [id, handle] : workers_) {
      if (count <= 0) {
        break;
      }
      if (handle.info.function_id == fn.id && handle.info.state == WorkerState::Idle) {
        WorkerStateMachine sm(handle.info.state);
        sm.transition(WorkerState::Terminating);
        handle.info.state = sm.state();
        sm.transition(WorkerState::Terminated);
        handle.info.state = sm.state();
        victims.push_back(id);
        Metrics::instance().inc_counter("autoscaler_scale_in_total", {{"function", fn.id}});
        count--;
      }
    }
    for (const auto& id : victims) {
      workers_.erase(id);
    }
    Metrics::instance().set_gauge("worker_count", static_cast<double>(workers_.size()));
  }
}

void WorkerManager::monitor_workers() { schedule_monitor(); }

void WorkerManager::schedule_monitor() {
  monitor_timer_.expires_after(std::chrono::seconds(1));
  monitor_timer_.async_wait([this](const boost::system::error_code& ec) {
    if (ec) {
      return;
    }
    schedule_monitor();
  });
}

std::vector<WorkerInfo> WorkerManager::list_workers() const {
  std::lock_guard lock(mutex_);
  std::vector<WorkerInfo> out;
  out.reserve(workers_.size());
  for (const auto& [id, handle] : workers_) {
    out.push_back(handle.info);
  }
  return out;
}

WorkerInfo* WorkerManager::find_worker(const std::string& worker_id) {
  std::lock_guard lock(mutex_);
  auto it = workers_.find(worker_id);
  if (it == workers_.end()) {
    return nullptr;
  }
  return &it->second.info;
}

int WorkerManager::worker_count(const std::string& function_id) const {
  std::lock_guard lock(mutex_);
  int count = 0;
  for (const auto& [id, handle] : workers_) {
    if (handle.info.function_id == function_id &&
        handle.info.state != WorkerState::Terminated &&
        handle.info.state != WorkerState::Failed) {
      count++;
    }
  }
  return count;
}

int WorkerManager::idle_count(const std::string& function_id) const {
  std::lock_guard lock(mutex_);
  int count = 0;
  for (const auto& [id, handle] : workers_) {
    if (handle.info.function_id == function_id && handle.info.state == WorkerState::Idle) {
      count++;
    }
  }
  return count;
}

int WorkerManager::busy_count(const std::string& function_id) const {
  std::lock_guard lock(mutex_);
  int count = 0;
  for (const auto& [id, handle] : workers_) {
    if (handle.info.function_id == function_id && handle.info.state == WorkerState::Running) {
      count++;
    }
  }
  return count;
}

void WorkerManager::register_remote_worker(const std::string& node_id, const std::string& worker_id,
                                           const std::string& function_id) {
  std::lock_guard lock(mutex_);
  WorkerHandle handle;
  handle.info.id = worker_id;
  handle.info.function_id = function_id;
  handle.info.node_id = node_id;
  handle.info.is_remote = true;
  handle.info.state = WorkerState::Idle;
  handle.info.idle_since = Clock::steady_now();
  workers_[worker_id] = std::move(handle);
}

void WorkerManager::invoke_on_worker(
    const std::string& worker_id, const FunctionRecord& fn, const std::string& request_id,
    const nlohmann::json& payload, std::function<void(Error, nlohmann::json, double)> callback) {
  std::lock_guard lock(mutex_);
  auto it = workers_.find(worker_id);
  if (it == workers_.end() || !it->second.local) {
    callback(Error::make(ErrorCode::WorkerStartFailed, "Worker unavailable"), {}, 0);
    return;
  }
  it->second.local->invoke(fn, request_id, payload, std::move(callback));
}

void WorkerManager::unregister_node_workers(const std::string& node_id) {
  std::lock_guard lock(mutex_);
  for (auto it = workers_.begin(); it != workers_.end();) {
    if (it->second.info.node_id == node_id) {
      it = workers_.erase(it);
    } else {
      ++it;
    }
  }
}

}  // namespace serverless
