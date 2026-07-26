#include "serverless/runtime/WorkerProcess.hpp"

#include "serverless/common/Error.hpp"

namespace serverless {

LocalWorkerConnection::LocalWorkerConnection(boost::asio::io_context& io, pid_t pid, int control_fd,
                                             std::shared_ptr<FunctionRunner> runner)
    : io_(io), pid_(pid), control_fd_(control_fd), runner_(std::move(runner)) {}

void LocalWorkerConnection::invoke(const FunctionRecord& fn, const std::string& request_id,
                                   const nlohmann::json& payload,
                                   std::function<void(Error, nlohmann::json, double)> callback) {
  runner_->run_async(fn, request_id, payload, std::move(callback));
}

bool LocalWorkerConnection::running() const { return pid_ > 0; }

void LocalWorkerConnection::shutdown() {
  if (control_fd_ >= 0) {
    close(control_fd_);
    control_fd_ = -1;
  }
  pid_ = -1;
}

Error WorkerProcessLauncher::spawn_worker_daemon(const std::string& /*worker_binary*/,
                                                 const std::string& /*socket_path*/, pid_t& out_pid,
                                                 int& control_fd) {
  out_pid = -1;
  control_fd = -1;
  return Error::success();
}

}  // namespace serverless
