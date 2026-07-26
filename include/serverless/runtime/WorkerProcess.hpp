#pragma once

#include <functional>
#include <memory>
#include <string>

#include <boost/asio/io_context.hpp>
#include <nlohmann/json.hpp>

#include "serverless/common/Error.hpp"
#include "serverless/runtime/FunctionRunner.hpp"
#include "serverless/storage/SqliteStore.hpp"

namespace serverless {

class LocalWorkerConnection {
 public:
  LocalWorkerConnection(boost::asio::io_context& io, pid_t pid, int control_fd,
                        std::shared_ptr<FunctionRunner> runner);

  void invoke(const FunctionRecord& fn, const std::string& request_id,
              const nlohmann::json& payload,
              std::function<void(Error, nlohmann::json, double)> callback);

  bool running() const;
  void shutdown();
  pid_t pid() const { return pid_; }

 private:
  boost::asio::io_context& io_;
  pid_t pid_;
  int control_fd_;
  std::shared_ptr<FunctionRunner> runner_;
};

class WorkerProcessLauncher {
 public:
  static Error spawn_worker_daemon(const std::string& worker_binary, const std::string& socket_path,
                                   pid_t& out_pid, int& control_fd);
};

}  // namespace serverless
