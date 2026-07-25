#include "serverless/core/Autoscaler.hpp"

#include <cmath>

#include "serverless/core/FunctionRegistry.hpp"
#include "serverless/observability/Metrics.hpp"

namespace serverless {

Autoscaler::Autoscaler(boost::asio::io_context& io, const Config& config,
                       std::shared_ptr<WorkerManager> workers,
                       std::function<std::size_t(const std::string&)> queue_depth_fn,
                       std::function<std::size_t()> total_queue_depth_fn,
                       std::function<const FunctionRecord*(const std::string&)> get_function_fn)
    : io_(io),
      config_(config),
      workers_(std::move(workers)),
      timer_(io),
      queue_depth_fn_(std::move(queue_depth_fn)),
      total_queue_depth_fn_(std::move(total_queue_depth_fn)),
      get_function_fn_(std::move(get_function_fn)) {}

void Autoscaler::start() {
  running_ = true;
  schedule_next();
}

void Autoscaler::stop() {
  running_ = false;
  timer_.cancel();
}

void Autoscaler::schedule_next() {
  if (!running_) {
    return;
  }
  timer_.expires_after(std::chrono::milliseconds(config_.scaling.interval_ms));
  timer_.async_wait([this](const boost::system::error_code& ec) {
    if (ec) {
      return;
    }
    tick();
    schedule_next();
  });
}

void Autoscaler::tick() {
  Metrics::instance().set_gauge("queue_depth", static_cast<double>(total_queue_depth_fn_()));
}

}  // namespace serverless
