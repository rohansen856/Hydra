#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include <nlohmann/json.hpp>

#include "serverless/common/Error.hpp"
#include "serverless/storage/SqliteStore.hpp"

namespace serverless {

enum class InvocationStatus {
  Queued,
  Running,
  Completed,
  Failed,
  TimedOut,
  Cancelled,
};

struct PendingInvocation {
  std::string request_id;
  std::string function_id;
  nlohmann::json payload;
  std::function<void(Error, nlohmann::json)> callback;
  std::chrono::steady_clock::time_point queued_at;
  bool cold_start{false};
};

class InvocationManager {
 public:
  explicit InvocationManager(std::shared_ptr<SqliteStore> store);

  std::string create_request_id();
  void record_terminal(const PendingInvocation& inv, InvocationStatus status, int http_status,
                       double duration_ms, const std::string& error_code = {});

  std::size_t total_accepted() const { return total_accepted_; }
  std::size_t total_completed() const { return total_completed_; }

  struct Rates {
    double invocations_per_second{0};
    double errors_per_second{0};
  };
  Rates rates() const;

 private:
  static constexpr std::size_t kRateBuckets = 60;

  void record_rate_sample(int http_status);

  std::shared_ptr<SqliteStore> store_;
  mutable std::mutex mutex_;
  std::unordered_set<std::string> terminal_ids_;
  std::size_t total_accepted_{0};
  std::size_t total_completed_{0};
  std::array<std::pair<std::uint64_t, std::uint64_t>, kRateBuckets> rate_buckets_{};
  std::int64_t last_rate_second_{0};
};

}  // namespace serverless
