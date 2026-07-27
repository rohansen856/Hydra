#include "serverless/api/DashboardApi.hpp"
#include "serverless/api/HttpServer.hpp"

#include <future>
#include <regex>
#include <thread>

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

#include "serverless/common/Id.hpp"
#include "serverless/common/Json.hpp"
#include "serverless/core/InvocationManager.hpp"
#include "serverless/core/FunctionRegistry.hpp"
#include "serverless/core/NodeRegistry.hpp"
#include "serverless/core/Scheduler.hpp"
#include "serverless/core/WorkerManager.hpp"
#include "serverless/observability/Metrics.hpp"
#include "serverless/observability/StructuredLogger.hpp"
#include "serverless/runtime/Worker.hpp"
#include "serverless/storage/SqliteStore.hpp"

namespace serverless {

namespace beast = boost::beast;
namespace http = beast::http;
using tcp = boost::asio::ip::tcp;

namespace api {

nlohmann::json create_function(std::shared_ptr<FunctionRegistry> registry,
                               const nlohmann::json& body) {
  const auto err = registry->register_function(body);
  if (!err.ok()) {
    return make_error_json(err);
  }
  return {{"id", body.value("name", "") + ":" + body.value("version", "1")},
          {"name", body.value("name", "")},
          {"version", body.value("version", "1")},
          {"status", "ACTIVE"}};
}

nlohmann::json get_function(std::shared_ptr<FunctionRegistry> registry, const std::string& name) {
  auto fn = registry->get_function(name);
  if (!fn) {
    return make_error_json(Error::make(ErrorCode::FunctionNotFound, "Function does not exist"));
  }
  return {{"id", fn->id},
          {"name", fn->name},
          {"version", fn->version},
          {"command", fn->command},
          {"min_workers", fn->min_workers},
          {"max_workers", fn->max_workers},
          {"timeout_ms", fn->timeout_ms},
          {"max_concurrency", fn->max_concurrency},
          {"memory_mb", fn->memory_mb},
          {"status", fn->status}};
}

nlohmann::json delete_function(std::shared_ptr<FunctionRegistry> registry,
                               const std::string& name) {
  const auto err = registry->delete_function(name);
  if (!err.ok()) {
    return make_error_json(err);
  }
  return {{"deleted", true}, {"name", name}};
}

void invoke_function(std::shared_ptr<Scheduler> scheduler, const std::string& name,
                     const nlohmann::json& payload,
                     std::function<void(Error, nlohmann::json)> callback) {
  scheduler->submit(name, payload, std::move(callback));
}

nlohmann::json list_workers(std::shared_ptr<WorkerManager> workers) {
  nlohmann::json arr = nlohmann::json::array();
  for (const auto& w : workers->list_workers()) {
    arr.push_back({{"id", w.id},
                   {"function_id", w.function_id},
                   {"node_id", w.node_id},
                   {"state", WorkerStateMachine::to_string(w.state)},
                   {"is_remote", w.is_remote}});
  }
  return {{"workers", arr}};
}

nlohmann::json healthz() { return {{"status", "ok"}}; }

nlohmann::json readyz(std::shared_ptr<SqliteStore> store, std::shared_ptr<Scheduler> scheduler) {
  if (!store->ping() || !scheduler->ready()) {
    return make_error_json(Error::make(ErrorCode::InternalError, "Not ready"));
  }
  return {{"status", "ready"}};
}

std::string metrics_text() { return Metrics::instance().render_prometheus(); }

nlohmann::json register_node(std::shared_ptr<NodeRegistry> nodes, const nlohmann::json& body) {
  const auto id = body.value("id", generate_node_id());
  const auto err = nodes->register_node(id, body.value("host", "127.0.0.1"),
                                        body.value("port", 9090), body.value("cpu_capacity", 1),
                                        body.value("memory_mb", 512));
  if (!err.ok()) {
    return make_error_json(err);
  }
  return {{"id", id}, {"status", "REGISTERED"}};
}

nlohmann::json node_heartbeat(std::shared_ptr<NodeRegistry> nodes, const std::string& id,
                              const nlohmann::json& body) {
  const auto err =
      nodes->heartbeat(id, body.value("running_workers", 0), body.value("available_workers", 0));
  if (!err.ok()) {
    return make_error_json(err);
  }
  return {{"status", "ok"}};
}

nlohmann::json node_workers(std::shared_ptr<NodeRegistry> nodes, const std::string& id,
                            const nlohmann::json& body) {
  return node_heartbeat(nodes, id, body);
}

}  // namespace api

namespace {

HttpResponse json_response(int status, const nlohmann::json& body) {
  HttpResponse resp;
  resp.status = status;
  resp.body = body.dump();
  resp.headers["Access-Control-Allow-Origin"] = "*";
  resp.headers["Access-Control-Allow-Methods"] = "GET, POST, DELETE, OPTIONS";
  resp.headers["Access-Control-Allow-Headers"] = "Content-Type";
  return resp;
}

HttpResponse cors_preflight() {
  HttpResponse resp;
  resp.status = 204;
  resp.content_type = "text/plain";
  resp.body = "";
  resp.headers["Access-Control-Allow-Origin"] = "*";
  resp.headers["Access-Control-Allow-Methods"] = "GET, POST, DELETE, OPTIONS";
  resp.headers["Access-Control-Allow-Headers"] = "Content-Type";
  return resp;
}

std::string path_only(const std::string& target) {
  const auto qpos = target.find('?');
  return qpos == std::string::npos ? target : target.substr(0, qpos);
}

}  // namespace

HttpServer::HttpServer(boost::asio::io_context& io, const Config& config,
                       std::shared_ptr<FunctionRegistry> registry,
                       std::shared_ptr<Scheduler> scheduler,
                       std::shared_ptr<WorkerManager> workers,
                       std::shared_ptr<NodeRegistry> nodes, std::shared_ptr<SqliteStore> store,
                       std::shared_ptr<InvocationManager> invocations)
    : io_(io),
      config_(config),
      registry_(std::move(registry)),
      scheduler_(std::move(scheduler)),
      workers_(std::move(workers)),
      nodes_(std::move(nodes)),
      store_(std::move(store)),
      invocations_(std::move(invocations)) {}

void HttpServer::start() {
  std::thread([this]() {
    boost::asio::io_context io;
    tcp::acceptor acceptor(io, tcp::endpoint(tcp::v4(), config_.server.port));
    acceptor.set_option(boost::asio::socket_base::reuse_address(true));
    while (running_) {
      tcp::socket socket(io);
      boost::system::error_code ec;
      acceptor.accept(socket, ec);
      if (ec) {
        continue;
      }
      beast::flat_buffer buffer;
      http::request_parser<http::string_body> parser;
      parser.body_limit(config_.limits.max_request_bytes);
      http::read(socket, buffer, parser, ec);
      if (ec) {
        continue;
      }
      const auto req = parser.get();
      HttpRequest hreq;
      hreq.method = std::string(req.method_string());
      hreq.target = std::string(req.target());
      hreq.body = req.body();
      const auto resp = route(hreq);
      http::response<http::string_body> bresp{static_cast<http::status>(resp.status), req.version()};
      bresp.set(http::field::server, "serverless-cpp");
      bresp.set(http::field::content_type, resp.content_type);
      for (const auto& [key, value] : resp.headers) {
        bresp.set(key, value);
      }
      bresp.body() = resp.body;
      bresp.prepare_payload();
      http::write(socket, bresp, ec);
      beast::error_code ignored;
      socket.shutdown(tcp::socket::shutdown_send, ignored);
    }
  }).detach();
}

void HttpServer::stop() { running_ = false; }

HttpResponse HttpServer::route(const HttpRequest& req) {
  const std::string target = req.target;
  const std::string path = path_only(target);
  Metrics::instance().inc_counter("server_requests_total");

  if (req.method == "OPTIONS") {
    return cors_preflight();
  }

  if (req.method == "GET" && (path == "/" || path == "/docs")) {
    HttpResponse resp;
    resp.status = 200;
    resp.content_type = "text/html; charset=utf-8";
    resp.headers["Access-Control-Allow-Origin"] = "*";
    resp.body = R"HTML(<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8"/>
  <meta name="viewport" content="width=device-width, initial-scale=1"/>
  <title>Hydra  Serverless Control Plane</title>
  <style>
    :root { color-scheme: dark; }
    body { margin:0; font-family: system-ui, sans-serif; background:#0a0e12; color:#e8edf0; line-height:1.5; }
    main { max-width:720px; margin:0 auto; padding:48px 24px; }
    h1 { font-size:2rem; letter-spacing:-0.03em; margin:0 0 8px; }
    .tag { color:#c5f946; font-size:0.75rem; letter-spacing:0.12em; text-transform:uppercase; font-weight:700; }
    p { color:#8b9bb4; }
    a { color:#c5f946; }
    ul { padding-left:1.2rem; }
    li { margin:0.4rem 0; }
    code { background:#151d24; padding:0.1rem 0.35rem; border-radius:4px; font-size:0.9em; }
  </style>
</head>
<body>
  <main>
    <div class="tag">Hydra</div>
    <h1>Serverless control plane</h1>
    <p>Multi-headed workers for a C++ serverless compute platform. This process serves the HTTP API; the Hydra dashboard UI typically runs on port 3000.</p>
    <h2>Quick links</h2>
    <ul>
      <li><a href="/healthz"><code>/healthz</code></a>  liveness</li>
      <li><a href="/readyz"><code>/readyz</code></a>  readiness</li>
      <li><a href="/api/v1/stats"><code>/api/v1/stats</code></a>  platform snapshot</li>
      <li><a href="/api/v1/functions"><code>/api/v1/functions</code></a>  registered functions</li>
      <li><a href="/api/v1/invocations"><code>/api/v1/invocations</code></a>  invocation history</li>
      <li><a href="/api/v1/workers"><code>/api/v1/workers</code></a>  worker pool</li>
      <li><a href="/api/v1/metrics/json"><code>/api/v1/metrics/json</code></a>  structured metrics</li>
      <li><a href="/metrics"><code>/metrics</code></a>  Prometheus text</li>
    </ul>
    <p>Dashboard: <a href="http://localhost:3000">http://localhost:3000</a></p>
    <p>API docs live in the repo under <code>docs/api.md</code>.</p>
  </main>
</body>
</html>)HTML";
    return resp;
  }

  if (req.method == "GET" && path == "/healthz") {
    return json_response(200, api::healthz());
  }
  if (req.method == "GET" && path == "/readyz") {
    const auto body = api::readyz(store_, scheduler_);
    return json_response(body.contains("error") ? 503 : 200, body);
  }
  if (req.method == "GET" && path == "/metrics") {
    HttpResponse resp;
    resp.status = 200;
    resp.content_type = "text/plain; version=0.0.4";
    resp.body = api::metrics_text();
    resp.headers["Access-Control-Allow-Origin"] = "*";
    return resp;
  }
  if (req.method == "GET" && path == "/api/v1/metrics/json") {
    return json_response(200, api::metrics_json());
  }
  if (req.method == "GET" && path == "/api/v1/stats") {
    return json_response(200, api::platform_stats(scheduler_, workers_, invocations_, store_, started_at_));
  }
  if (req.method == "GET" && path.rfind("/api/v1/stats/functions/", 0) == 0) {
    const auto name = path.substr(std::string("/api/v1/stats/functions/").size());
    const auto out = api::function_stats(name, registry_, scheduler_, workers_);
    return json_response(out.contains("error") ? 404 : 200, out);
  }
  if (req.method == "GET" && path == "/api/v1/invocations") {
    return json_response(200, api::list_invocations(store_, api::parse_invocation_query(target)));
  }
  if (req.method == "GET" && path == "/api/v1/functions") {
    return json_response(200, api::list_functions(registry_));
  }
  if (req.method == "GET" && path == "/api/v1/nodes") {
    return json_response(200, api::list_nodes(nodes_));
  }
  if (req.method == "POST" && path == "/api/v1/functions") {
    nlohmann::json body;
    try {
      body = nlohmann::json::parse(req.body);
    } catch (...) {
      return json_response(400, make_error_json(Error::make(ErrorCode::InvalidRequest, "Invalid JSON")));
    }
    const auto out = api::create_function(registry_, body);
    return json_response(out.contains("error") ? out["status"].get<int>() : 201, out);
  }
  if (req.method == "GET" && path.rfind("/api/v1/functions/", 0) == 0 &&
      path.find("/invoke") == std::string::npos) {
    const auto name = path.substr(std::string("/api/v1/functions/").size());
    const auto out = api::get_function(registry_, name);
    return json_response(out.contains("error") ? 404 : 200, out);
  }
  if (req.method == "DELETE" && path.rfind("/api/v1/functions/", 0) == 0) {
    const auto name = path.substr(std::string("/api/v1/functions/").size());
    const auto out = api::delete_function(registry_, name);
    return json_response(out.contains("error") ? 404 : 200, out);
  }
  if (req.method == "GET" && path == "/api/v1/workers") {
    return json_response(200, api::list_workers(workers_));
  }
  if (req.method == "POST" && path.rfind("/api/v1/functions/", 0) == 0 &&
      path.ends_with("/invoke")) {
    const auto prefix = std::string("/api/v1/functions/");
    const auto suffix = std::string("/invoke");
    const auto name = path.substr(prefix.size(), path.size() - prefix.size() - suffix.size());
    nlohmann::json payload;
    try {
      payload = req.body.empty() ? nlohmann::json::object() : nlohmann::json::parse(req.body);
    } catch (...) {
      return json_response(400, make_error_json(Error::make(ErrorCode::InvalidRequest, "Invalid JSON")));
    }

    std::promise<std::pair<Error, nlohmann::json>> prom;
    auto fut = prom.get_future();
    api::invoke_function(
        scheduler_, name, payload,
        [&prom](Error err, nlohmann::json result) { prom.set_value({err, result}); });
    const auto [err, result] = fut.get();
    return json_response(err.ok() ? result.value("status", 200) : err.http_status(), result);
  }
  if (req.method == "POST" && path == "/internal/v1/nodes/register") {
    nlohmann::json body = nlohmann::json::parse(req.body);
    return json_response(201, api::register_node(nodes_, body));
  }
  if (req.method == "POST" && path.rfind("/internal/v1/nodes/", 0) == 0 &&
      path.ends_with("/heartbeat")) {
    static const std::regex re(R"(/internal/v1/nodes/([^/]+)/heartbeat)");
    std::smatch m;
    nlohmann::json body = nlohmann::json::parse(req.body);
    if (std::regex_match(path, m, re)) {
      return json_response(200, api::node_heartbeat(nodes_, m[1].str(), body));
    }
  }
  if (req.method == "POST" && path.rfind("/internal/v1/nodes/", 0) == 0 &&
      path.ends_with("/workers")) {
    static const std::regex re(R"(/internal/v1/nodes/([^/]+)/workers)");
    std::smatch m;
    nlohmann::json body = nlohmann::json::parse(req.body);
    if (std::regex_match(path, m, re)) {
      return json_response(200, api::node_workers(nodes_, m[1].str(), body));
    }
  }

  return json_response(404, make_error_json(Error::make(ErrorCode::InvalidRequest, "Route not found")));
}

}  // namespace serverless
