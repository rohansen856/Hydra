#include "serverless/core/Scheduler.hpp"

#include "serverless/core/Autoscaler.hpp"
#include "serverless/core/FunctionRegistry.hpp"
#include "serverless/core/WorkerManager.hpp"
#include <thread>

#include "serverless/common/Clock.hpp"
#include "serverless/common/Json.hpp"
#include "serverless/observability/Metrics.hpp"
#include "serverless/observability/StructuredLogger.hpp"

namespace serverless {

Scheduler::Scheduler(boost::asio::io_context& io, const Config& config,
                     std::shared_ptr<FunctionRegistry> registry,
                     std::shared_ptr<InvocationManager> invocations,
                     std::shared_ptr<WorkerManager> workers,
                     std::shared_ptr<Autoscaler> autoscaler)
    : io_(io),
      config_(config),
      registry_(std::move(registry)),
      invocations_(std::move(invocations)),
      workers_(std::move(workers)),
      autoscaler_(std::move(autoscaler)),
      strand_(boost::asio::make_strand(io)) {}

void Scheduler::submit(const std::string& function_name, const nlohmann::json& payload,
                       std::function<void(Error, nlohmann::json)> callback) {
  boost::asio::post(strand_, [this, function_name, payload, cb = std::move(callback)]() mutable {
    auto fn = registry_->get_function(function_name);
    if (!fn) {
      cb(Error::make(ErrorCode::FunctionNotFound, "Function does not exist"), {});
      return;
    }

    PendingInvocation inv;
    inv.request_id = invocations_->create_request_id();
    inv.function_id = fn->id;
    inv.payload = payload;
    inv.callback = std::move(cb);
    inv.queued_at = Clock::steady_now();

    {
      std::lock_guard lock(mutex_);
      if (total_queued_ >= static_cast<std::size_t>(config_.scheduler.queue_limit)) {
        inv.callback(Error::make(ErrorCode::QueueFull, "Scheduler queue saturated"), {});
        return;
      }
      queues_[fn->id].push_back(std::move(inv));
      total_queued_++;
    }

    Metrics::instance().inc_counter("server_requests_total", {{"function", fn->name}});
    workers_->ensure_min_workers(*fn);
    try_dispatch_locked(*fn);
  });
}

void Scheduler::try_dispatch_locked(const FunctionRecord& fn) {
  while (true) {
    PendingInvocation current;
    {
      std::lock_guard lock(mutex_);
      auto it = queues_.find(fn.id);
      if (it == queues_.end() || it->second.empty()) {
        return;
      }
    }

    auto worker_id = workers_->acquire_idle_worker(fn);
    if (!worker_id) {
      const int current_workers = workers_->worker_count(fn.id);
      if (current_workers < fn.max_workers) {
        workers_->start_worker(fn, true);
        worker_id = workers_->acquire_idle_worker(fn);
      }
      if (!worker_id) {
        return;
      }
    }

    {
      std::lock_guard lock(mutex_);
      auto& q = queues_[fn.id];
      if (q.empty()) {
        workers_->release_worker(*worker_id, false);
        return;
      }
      current = std::move(q.front());
      q.pop_front();
      total_queued_--;
    }

    current.cold_start = workers_->worker_count(fn.id) <= fn.min_workers;
    const auto dispatch_payload = current.payload;
    const auto dispatch_request_id = current.request_id;
    workers_->invoke_on_worker(*worker_id, fn, dispatch_request_id, dispatch_payload,
                               [this, current = std::move(current), worker_id = *worker_id,
                                fn](Error err, nlohmann::json result, double duration_ms) mutable {
                                 boost::asio::post(strand_, [this, current = std::move(current),
                                                             worker_id, err, result, duration_ms,
                                                             fn]() mutable {
                                   workers_->release_worker(worker_id, !err.ok());
                                   if (!err.ok()) {
                                     complete_invocation(std::move(current), err, {}, duration_ms,
                                                         current.cold_start);
                                   } else {
                                     complete_invocation(std::move(current), Error::success(), result,
                                                         duration_ms, current.cold_start);
                                   }
                                   try_dispatch_locked(fn);
                                 });
                               });
    return;
  }
}

void Scheduler::complete_invocation(PendingInvocation inv, Error err, nlohmann::json result,
                                    double duration_ms, bool cold_start) {
  inv.cold_start = cold_start;
  InvocationStatus status = InvocationStatus::Completed;
  if (!err.ok()) {
    if (err.code == ErrorCode::InvocationTimeout) {
      status = InvocationStatus::TimedOut;
    } else {
      status = InvocationStatus::Failed;
    }
    result = make_error_json(err, inv.request_id);
  } else if (cold_start) {
    Metrics::instance().observe_histogram("cold_start_duration_seconds", duration_ms / 1000.0,
                                          {{"function", inv.function_id}});
  } else {
    Metrics::instance().inc_counter("warm_invocation_count", {{"function", inv.function_id}});
  }

  invocations_->record_terminal(inv, status, err.ok() ? result.value("status", 200) : err.http_status(),
                                duration_ms, err.ok() ? "" : err.code_string());
  inv.callback(err, result);
}

void Scheduler::on_worker_idle(const std::string& /*worker_id*/) { dispatch_pending(); }

void Scheduler::on_worker_failed(const std::string& worker_id) {
  boost::asio::post(strand_, [this, worker_id]() {
    StructuredLogger::warn("Worker failed: " + worker_id);
    dispatch_pending();
  });
}

void Scheduler::dispatch_pending() {
  boost::asio::post(strand_, [this]() {
    for (const auto& fn : registry_->list_functions()) {
      try_dispatch_locked(fn);
    }
  });
}

std::size_t Scheduler::queue_depth(const std::string& function_id) const {
  std::lock_guard lock(mutex_);
  if (function_id.empty()) {
    return total_queued_;
  }
  auto it = queues_.find(function_id);
  return it == queues_.end() ? 0 : it->second.size();
}

std::size_t Scheduler::total_queue_depth() const { return queue_depth(); }

Scheduler::Stats Scheduler::stats() const {
  Stats s;
  s.queue_depth = total_queue_depth();
  return s;
}

void Scheduler::check_queue_timeouts() {
  const auto now = Clock::steady_now();
  std::vector<PendingInvocation> expired;
  {
    std::lock_guard lock(mutex_);
    for (auto& [fid, q] : queues_) {
      while (!q.empty()) {
        const auto wait_ms =
            std::chrono::duration<double, std::milli>(now - q.front().queued_at).count();
        if (wait_ms > config_.scheduler.queue_timeout_ms) {
          expired.push_back(std::move(q.front()));
          q.pop_front();
          total_queued_--;
        } else {
          break;
        }
      }
    }
  }
  for (auto& inv : expired) {
    complete_invocation(std::move(inv),
                        Error::make(ErrorCode::QueueFull, "Queue wait timeout exceeded"), {}, 0,
                        false);
  }
}

}  // namespace serverless
