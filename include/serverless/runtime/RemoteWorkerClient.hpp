#pragma once

#include <functional>
#include <string>

#include <boost/asio/io_context.hpp>
#include <nlohmann/json.hpp>

#include "serverless/common/Error.hpp"
#include "serverless/storage/SqliteStore.hpp"

namespace serverless {

class RemoteWorkerClient {
 public:
  RemoteWorkerClient(boost::asio::io_context& io, const std::string& host, int port);

  void invoke_async(const FunctionRecord& fn, const std::string& request_id,
                    const nlohmann::json& payload,
                    std::function<void(Error, nlohmann::json, double)> callback);

 private:
  boost::asio::io_context& io_;
  std::string host_;
  int port_;
};

}  // namespace serverless
