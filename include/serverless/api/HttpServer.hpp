#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

#include <boost/asio/io_context.hpp>
#include <nlohmann/json.hpp>

#include "serverless/common/Error.hpp"
#include "serverless/config/Config.hpp"

namespace serverless {

class FunctionRegistry;
class Scheduler;
class WorkerManager;
class NodeRegistry;
class SqliteStore;
class InvocationManager;

struct HttpRequest {
  std::string method;
  std::string target;
  std::string body;
};

struct HttpResponse {
  int status{200};
  std::string content_type{"application/json"};
  std::string body;
  std::unordered_map<std::string, std::string> headers;
};

class HttpServer {
 public:
  HttpServer(boost::asio::io_context& io, const Config& config,
             std::shared_ptr<FunctionRegistry> registry,
             std::shared_ptr<Scheduler> scheduler,
             std::shared_ptr<WorkerManager> workers,
             std::shared_ptr<NodeRegistry> nodes,
             std::shared_ptr<SqliteStore> store,
             std::shared_ptr<InvocationManager> invocations);

  void start();
  void stop();
  HttpResponse route(const HttpRequest& req);

 private:
  std::atomic<bool> running_{true};
  boost::asio::io_context& io_;
  Config config_;
  std::shared_ptr<FunctionRegistry> registry_;
  std::shared_ptr<Scheduler> scheduler_;
  std::shared_ptr<WorkerManager> workers_;
  std::shared_ptr<NodeRegistry> nodes_;
  std::shared_ptr<SqliteStore> store_;
  std::shared_ptr<InvocationManager> invocations_;
  std::chrono::steady_clock::time_point started_at_{std::chrono::steady_clock::now()};
};

namespace api {

nlohmann::json create_function(std::shared_ptr<FunctionRegistry> registry, const nlohmann::json& body);
nlohmann::json get_function(std::shared_ptr<FunctionRegistry> registry, const std::string& name);
nlohmann::json delete_function(std::shared_ptr<FunctionRegistry> registry, const std::string& name);
void invoke_function(std::shared_ptr<Scheduler> scheduler, const std::string& name,
                     const nlohmann::json& payload,
                     std::function<void(Error, nlohmann::json)> callback);
nlohmann::json list_workers(std::shared_ptr<WorkerManager> workers);
nlohmann::json healthz();
nlohmann::json readyz(std::shared_ptr<SqliteStore> store, std::shared_ptr<Scheduler> scheduler);
std::string metrics_text();

nlohmann::json register_node(std::shared_ptr<NodeRegistry> nodes, const nlohmann::json& body);
nlohmann::json node_heartbeat(std::shared_ptr<NodeRegistry> nodes, const std::string& id,
                              const nlohmann::json& body);
nlohmann::json node_workers(std::shared_ptr<NodeRegistry> nodes, const std::string& id,
                            const nlohmann::json& body);

}  // namespace api

}  // namespace serverless
