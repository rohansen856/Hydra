#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <memory>

#include "serverless/api/DashboardApi.hpp"
#include "serverless/api/HttpServer.hpp"
#include "serverless/config/Config.hpp"
#include "serverless/core/FunctionRegistry.hpp"
#include "serverless/core/InvocationManager.hpp"
#include "serverless/core/NodeRegistry.hpp"
#include "serverless/core/Scheduler.hpp"
#include "serverless/core/WorkerManager.hpp"
#include "serverless/observability/Metrics.hpp"
#include "serverless/runtime/FunctionRunner.hpp"
#include "serverless/storage/SqliteStore.hpp"

TEST_CASE("SqliteStore list and count invocations") {
  const auto db = std::filesystem::temp_directory_path() / "dash_inv.db";
  std::filesystem::remove(db);
  auto store = std::make_shared<serverless::SqliteStore>(db.string());
  REQUIRE(store->init_schema().ok());

  serverless::InvocationRecord inv;
  inv.request_id = "req-1";
  inv.function_id = "hello:1";
  inv.status = "COMPLETED";
  inv.started_at = 1000;
  inv.finished_at = 1012;
  inv.duration_ms = 12.0;
  REQUIRE(store->insert_invocation(inv).ok());

  inv.request_id = "req-2";
  inv.status = "FAILED";
  inv.error_code = "TIMEOUT";
  inv.finished_at = 2000;
  REQUIRE(store->insert_invocation(inv).ok());

  serverless::InvocationQuery query;
  query.function = "hello";
  auto rows = store->list_invocations(query);
  REQUIRE(rows.size() == 2);
  REQUIRE(store->count_invocations(query) == 2);

  query.status = "COMPLETED";
  rows = store->list_invocations(query);
  REQUIRE(rows.size() == 1);
  REQUIRE(rows[0].request_id == "req-1");
}

TEST_CASE("Metrics render_json exposes structured metrics") {
  auto& metrics = serverless::Metrics::instance();
  metrics.inc_counter("test_counter", {{"label", "value"}}, 3);
  metrics.set_gauge("test_gauge", 7.5);
  metrics.observe_histogram("test_hist", 0.05);

  const auto json = metrics.render_json();
  REQUIRE(json.contains("counters"));
  REQUIRE(json.contains("gauges"));
  REQUIRE(json.contains("histograms"));
  REQUIRE(json["counters"].is_array());
  REQUIRE(json["histograms"].is_array());
}

TEST_CASE("parse_invocation_query reads query parameters") {
  const auto query =
      serverless::api::parse_invocation_query("/api/v1/invocations?limit=25&function=hello&status=COMPLETED");
  REQUIRE(query.limit == 25);
  REQUIRE(query.function.has_value());
  REQUIRE(*query.function == "hello");
  REQUIRE(query.status.has_value());
  REQUIRE(*query.status == "COMPLETED");
}

TEST_CASE("HttpServer dashboard routes") {
  const auto db = std::filesystem::temp_directory_path() / "dash_http.db";
  std::filesystem::remove(db);
  auto store = std::make_shared<serverless::SqliteStore>(db.string());
  REQUIRE(store->init_schema().ok());

  serverless::Config config;
  config.server.port = 18080;
  const auto fn_dir = std::filesystem::temp_directory_path() / "dash_fn";
  std::filesystem::remove_all(fn_dir);
  config.storage.functions_dir = fn_dir.string();

  boost::asio::io_context io;
  auto registry = std::make_shared<serverless::FunctionRegistry>(store, config);
  auto invocations = std::make_shared<serverless::InvocationManager>(store);
  auto runner = std::make_shared<serverless::FunctionRunner>(io, config);
  auto workers = std::make_shared<serverless::WorkerManager>(io, config, registry, runner);
  auto nodes = std::make_shared<serverless::NodeRegistry>();
  std::shared_ptr<serverless::Autoscaler> autoscaler;
  auto scheduler = std::make_shared<serverless::Scheduler>(io, config, registry, invocations,
                                                           workers, autoscaler);
  scheduler->set_ready(true);

  serverless::HttpServer server(io, config, registry, scheduler, workers, nodes, store,
                                invocations);

  serverless::HttpRequest req;
  req.method = "GET";
  req.target = "/api/v1/functions";
  auto resp = server.route(req);
  REQUIRE(resp.status == 200);
  REQUIRE(resp.body.find("functions") != std::string::npos);

  req.target = "/api/v1/stats";
  resp = server.route(req);
  REQUIRE(resp.status == 200);
  REQUIRE(resp.body.find("invocations_per_second") != std::string::npos);

  req.target = "/api/v1/metrics/json";
  resp = server.route(req);
  REQUIRE(resp.status == 200);
  REQUIRE(resp.headers.at("Access-Control-Allow-Origin") == "*");

  req.method = "OPTIONS";
  req.target = "/api/v1/stats";
  resp = server.route(req);
  REQUIRE(resp.status == 204);
}
