#include "serverless/runtime/WorkerDaemon.hpp"

#include "serverless/config/Config.hpp"
#include "serverless/runtime/FunctionRunner.hpp"

namespace serverless {

WorkerDaemon::WorkerDaemon(boost::asio::io_context& io, const Config& config,
                           const std::string& socket_path)
    : io_(io), config_(config), socket_path_(socket_path) {}

void WorkerDaemon::run() {
  runner_ = std::make_unique<FunctionRunner>(io_, config_);
  io_.run();
}

int run_worker_daemon(int /*argc*/, char** /*argv*/) { return 0; }

}  // namespace serverless
