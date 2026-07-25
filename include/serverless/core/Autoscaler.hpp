#pragma once

#include <functional>
#include <memory>

#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>

#include "serverless/config/Config.hpp"
#include "serverless/core/WorkerManager.hpp"

namespace serverless {

class Autoscaler {
 public:
  Autoscaler(boost::asio::io_context& io, const Config& config,
             std::shared_ptr<WorkerManager> workers,
             std::function<std::size_t(const std::string&)> queue_depth_fn,
             std::function<std::size_t()> total_queue_depth_fn,
             std::function<const FunctionRecord*(const std::string&)> get_function_fn);

  void start();
  void stop();

 private:
  boost::asio::io_context& io_;
  Config config_;
  std::shared_ptr<WorkerManager> workers_;
  boost::asio::steady_timer timer_;
  bool running_{false};

  std::function<std::size_t(const std::string&)> queue_depth_fn_;
  std::function<std::size_t()> total_queue_depth_fn_;
  std::function<const FunctionRecord*(const std::string&)> get_function_fn_;

  void tick();
  void schedule_next();
};

}  // namespace serverless
