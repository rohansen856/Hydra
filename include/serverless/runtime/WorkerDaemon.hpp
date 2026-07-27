#pragma once

#include <string>

#include <boost/asio/io_context.hpp>
#include <nlohmann/json.hpp>

#include "serverless/common/Error.hpp"
#include "serverless/config/Config.hpp"
#include "serverless/runtime/FunctionRunner.hpp"

namespace serverless {

// Worker daemon entrypoint helpers
int run_worker_daemon(int argc, char** argv);

class WorkerDaemon {
 public:
  WorkerDaemon(boost::asio::io_context& io, const Config& config, const std::string& socket_path);

  void run();

 private:
  boost::asio::io_context& io_;
  Config config_;
  std::string socket_path_;
  std::unique_ptr<FunctionRunner> runner_;
};

}  // namespace serverless
