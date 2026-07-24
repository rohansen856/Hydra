#include "serverless/core/InvocationManager.hpp"

#include "serverless/common/Clock.hpp"
#include "serverless/common/Id.hpp"
#include "serverless/observability/Metrics.hpp"
#include "serverless/observability/StructuredLogger.hpp"

namespace serverless {

InvocationManager::InvocationManager(std::shared_ptr<SqliteStore> store)
    : store_(std::move(store)) {}

std::string InvocationManager::create_request_id() {
  std::lock_guard lock(mutex_);
  total_accepted_++;
  return generate_request_id();
}

void InvocationManager::record_terminal(const PendingInvocation& inv, InvocationStatus status,
                                        int http_status, double duration_ms,
                                        const std::string& error_code) {
  {
    std::lock_guard lock(mutex_);
    if (terminal_ids_.contains(inv.request_id)) {
      return;
    }
    terminal_ids_.insert(inv.request_id);
    total_completed_++;
  }

  InvocationRecord rec;
  rec.request_id = inv.request_id;
  rec.function_id = inv.function_id;
  rec.started_at = Clock::now_unix_ms() - static_cast<std::int64_t>(duration_ms);
  rec.finished_at = Clock::now_unix_ms();
  rec.duration_ms = duration_ms;
  switch (status) {
    case InvocationStatus::Completed:
      rec.status = "COMPLETED";
      break;
    case InvocationStatus::TimedOut:
      rec.status = "TIMEOUT";
      break;
    case InvocationStatus::Failed:
      rec.status = "FAILED";
      break;
    case InvocationStatus::Cancelled:
      rec.status = "CANCELLED";
      break;
    default:
      rec.status = "UNKNOWN";
  }
  rec.error_code = error_code;
  store_->insert_invocation(rec);

  Metrics::instance().inc_counter("invocations_total",
                                  {{"function", inv.function_id},
                                   {"status", std::to_string(http_status)},
                                   {"error_code", error_code.empty() ? "none" : error_code}});
  Metrics::instance().observe_histogram("invocation_duration_seconds", duration_ms / 1000.0,
                                        {{"function", inv.function_id}});

  StructuredLogger::log_event("invocation_completed",
                              {{"request_id", inv.request_id},
                               {"function", inv.function_id},
                               {"duration_ms", duration_ms},
                               {"status", http_status},
                               {"cold_start", inv.cold_start}});
  record_rate_sample(http_status);
}

void InvocationManager::record_rate_sample(int http_status) {
  const auto now_sec = Clock::now_unix_ms() / 1000;
  std::lock_guard lock(mutex_);
  if (now_sec != last_rate_second_) {
    rate_buckets_[static_cast<std::size_t>(now_sec % kRateBuckets)] = {0, 0};
    last_rate_second_ = now_sec;
  }
  auto& bucket = rate_buckets_[static_cast<std::size_t>(now_sec % kRateBuckets)];
  bucket.first++;
  if (http_status >= 400) {
    bucket.second++;
  }
}

InvocationManager::Rates InvocationManager::rates() const {
  std::lock_guard lock(mutex_);
  std::uint64_t invocations = 0;
  std::uint64_t errors = 0;
  for (const auto& bucket : rate_buckets_) {
    invocations += bucket.first;
    errors += bucket.second;
  }
  Rates rates;
  rates.invocations_per_second = static_cast<double>(invocations) / kRateBuckets;
  rates.errors_per_second = static_cast<double>(errors) / kRateBuckets;
  return rates;
}

}  // namespace serverless
