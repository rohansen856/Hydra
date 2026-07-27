#include "serverless/api/DashboardApi.hpp"

#include <chrono>
#include <sstream>

#include "serverless/common/Clock.hpp"
#include "serverless/common/Json.hpp"
#include "serverless/core/FunctionRegistry.hpp"
#include "serverless/core/InvocationManager.hpp"
#include "serverless/core/NodeRegistry.hpp"
#include "serverless/core/Scheduler.hpp"
#include "serverless/core/WorkerManager.hpp"
#include "serverless/observability/Metrics.hpp"
#include "serverless/runtime/Worker.hpp"

namespace serverless {
namespace api {

namespace {

std::string url_decode(const std::string& value) {
  std::string out;
  out.reserve(value.size());
  for (std::size_t i = 0; i < value.size(); ++i) {
    if (value[i] == '%' && i + 2 < value.size()) {
      const int hi = value[i + 1] >= 'A' ? value[i + 1] - 'A' + 10 : value[i + 1] - '0';
      const int lo = value[i + 2] >= 'A' ? value[i + 2] - 'A' + 10 : value[i + 2] - '0';
      out.push_back(static_cast<char>((hi << 4) | lo));
      i += 2;
    } else if (value[i] == '+') {
      out.push_back(' ');
    } else {
      out.push_back(value[i]);
    }
  }
  return out;
}

nlohmann::json function_to_json(const FunctionRecord& fn) {
  return {{"id", fn.id},
          {"name", fn.name},
          {"version", fn.version},
          {"command", fn.command},
          {"min_workers", fn.min_workers},
          {"max_workers", fn.max_workers},
          {"timeout_ms", fn.timeout_ms},
          {"max_concurrency", fn.max_concurrency},
          {"memory_mb", fn.memory_mb},
          {"status", fn.status},
          {"created_at", fn.created_at}};
}

nlohmann::json invocation_to_json(const InvocationRecord& inv) {
  return {{"request_id", inv.request_id},
          {"function_id", inv.function_id},
          {"status", inv.status},
          {"duration_ms", inv.duration_ms},
          {"error_code", inv.error_code},
          {"started_at", inv.started_at},
          {"finished_at", inv.finished_at}};
}

}  // namespace

InvocationQuery parse_invocation_query(const std::string& target) {
  InvocationQuery query;
  const auto qpos = target.find('?');
  if (qpos == std::string::npos) {
    return query;
  }
  const std::string qs = target.substr(qpos + 1);
  std::istringstream stream(qs);
  std::string pair;
  while (std::getline(stream, pair, '&')) {
    const auto eq = pair.find('=');
    if (eq == std::string::npos) {
      continue;
    }
    const auto key = pair.substr(0, eq);
    const auto value = url_decode(pair.substr(eq + 1));
    if (key == "limit") {
      query.limit = std::max(1, std::min(500, std::stoi(value)));
    } else if (key == "offset") {
      query.offset = std::max(0, std::stoi(value));
    } else if (key == "function") {
      query.function = value;
    } else if (key == "status") {
      query.status = value;
    } else if (key == "since_ms") {
      query.since_ms = std::stoll(value);
    }
  }
  return query;
}

nlohmann::json list_functions(std::shared_ptr<FunctionRegistry> registry) {
  nlohmann::json arr = nlohmann::json::array();
  for (const auto& fn : registry->list_functions()) {
    arr.push_back(function_to_json(fn));
  }
  return {{"functions", arr}};
}

nlohmann::json list_nodes(std::shared_ptr<NodeRegistry> nodes) {
  nlohmann::json arr = nlohmann::json::array();
  for (const auto& node : nodes->list_nodes()) {
    arr.push_back({{"id", node.id},
                   {"host", node.host},
                   {"port", node.port},
                   {"cpu_capacity", node.cpu_capacity},
                   {"memory_mb", node.memory_mb},
                   {"running_workers", node.running_workers},
                   {"available_workers", node.available_workers},
                   {"healthy", node.healthy}});
  }
  return {{"nodes", arr}};
}

nlohmann::json metrics_json() { return Metrics::instance().render_json(); }

nlohmann::json list_invocations(std::shared_ptr<SqliteStore> store, const InvocationQuery& query) {
  InvocationQuery q = query;
  q.limit = std::max(1, std::min(500, q.limit));
  nlohmann::json arr = nlohmann::json::array();
  for (const auto& inv : store->list_invocations(q)) {
    arr.push_back(invocation_to_json(inv));
  }
  return {{"invocations", arr}, {"total", store->count_invocations(q)}};
}

nlohmann::json platform_stats(std::shared_ptr<Scheduler> scheduler,
                              std::shared_ptr<WorkerManager> workers,
                              std::shared_ptr<InvocationManager> invocations,
                              std::shared_ptr<SqliteStore> store,
                              std::chrono::steady_clock::time_point started_at) {
  int idle = 0;
  int busy = 0;
  for (const auto& w : workers->list_workers()) {
    if (w.state == WorkerState::Idle) {
      idle++;
    } else if (w.state == WorkerState::Running) {
      busy++;
    }
  }
  const auto uptime_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                             std::chrono::steady_clock::now() - started_at)
                             .count();
  const auto rates = invocations->rates();
  return {{"platform",
           {{"ready", scheduler->ready()},
            {"sqlite_ok", store->ping()},
            {"uptime_ms", uptime_ms}}},
          {"invocations",
           {{"accepted", invocations->total_accepted()},
            {"completed", invocations->total_completed()}}},
          {"workers",
           {{"total", idle + busy}, {"idle", idle}, {"busy", busy}}},
          {"queue", {{"total_depth", scheduler->total_queue_depth()}}},
          {"rates",
           {{"invocations_per_second", rates.invocations_per_second},
            {"errors_per_second", rates.errors_per_second}}}};
}

nlohmann::json function_stats(const std::string& name, std::shared_ptr<FunctionRegistry> registry,
                              std::shared_ptr<Scheduler> scheduler,
                              std::shared_ptr<WorkerManager> workers) {
  auto fn = registry->get_function(name);
  if (!fn) {
    return make_error_json(Error::make(ErrorCode::FunctionNotFound, "Function does not exist"));
  }
  return {{"name", fn->name},
          {"function_id", fn->id},
          {"queue_depth", scheduler->queue_depth(fn->id)},
          {"workers",
           {{"total", workers->worker_count(fn->id)},
            {"idle", workers->idle_count(fn->id)},
            {"busy", workers->busy_count(fn->id)}}}};
}

}  // namespace api
}  // namespace serverless
