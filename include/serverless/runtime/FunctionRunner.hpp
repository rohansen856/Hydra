#pragma once

#include <functional>
#include <memory>
#include <string>

#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>
#include <nlohmann/json.hpp>

#include "serverless/common/Error.hpp"
#include "serverless/config/Config.hpp"
#include "serverless/runtime/Process.hpp"
#include "serverless/storage/SqliteStore.hpp"

namespace serverless {

class FunctionRunner {
 public:
  FunctionRunner(boost::asio::io_context& io, const Config& config);

  void run_async(const FunctionRecord& fn, const std::string& request_id,
                 const nlohmann::json& payload,
                 std::function<void(Error, nlohmann::json, double)> callback);

 private:
  boost::asio::io_context& io_;
  Config config_;
};

}  // namespace serverless
