#pragma once

#include <chrono>
#include <memory>
#include <string>

#include <nlohmann/json.hpp>

#include "serverless/storage/SqliteStore.hpp"

namespace serverless {

class FunctionRegistry;
class InvocationManager;
class NodeRegistry;
class Scheduler;
class WorkerManager;

namespace api {

nlohmann::json list_functions(std::shared_ptr<FunctionRegistry> registry);
nlohmann::json list_nodes(std::shared_ptr<NodeRegistry> nodes);
nlohmann::json metrics_json();
nlohmann::json list_invocations(std::shared_ptr<SqliteStore> store, const InvocationQuery& query);
nlohmann::json platform_stats(std::shared_ptr<Scheduler> scheduler,
                              std::shared_ptr<WorkerManager> workers,
                              std::shared_ptr<InvocationManager> invocations,
                              std::shared_ptr<SqliteStore> store,
                              std::chrono::steady_clock::time_point started_at);
nlohmann::json function_stats(const std::string& name, std::shared_ptr<FunctionRegistry> registry,
                              std::shared_ptr<Scheduler> scheduler,
                              std::shared_ptr<WorkerManager> workers);

InvocationQuery parse_invocation_query(const std::string& target);

}  // namespace api
}  // namespace serverless
