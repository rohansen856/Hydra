#include <filesystem>
#include <iostream>

#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>

#include "serverless/api/HttpServer.hpp"
#include "serverless/config/Config.hpp"
#include "serverless/core/Autoscaler.hpp"
#include "serverless/core/FunctionRegistry.hpp"
#include "serverless/core/InvocationManager.hpp"
#include "serverless/core/NodeRegistry.hpp"
#include "serverless/core/Scheduler.hpp"
#include "serverless/core/WorkerManager.hpp"
#include "serverless/observability/StructuredLogger.hpp"
#include "serverless/runtime/FunctionRunner.hpp"
#include "serverless/storage/SqliteStore.hpp"

int main(int argc, char** argv) {
  const std::string config_path = argc > 1 ? argv[1] : "config/config.yaml";
  serverless::Config config;
  try {
    config = serverless::Config::load_from_file(config_path);
  } catch (const std::exception& ex) {
    std::cerr << "Config error: " << ex.what() << "\n";
    return 1;
  }

  serverless::StructuredLogger::init(config.logging.level);
  std::filesystem::create_directories("data");

  auto store = std::make_shared<serverless::SqliteStore>(config.storage.db_path);
  if (auto err = store->init_schema(); !err.ok()) {
    std::cerr << err.message << "\n";
    return 1;
  }

  boost::asio::io_context io;
  auto registry = std::make_shared<serverless::FunctionRegistry>(store, config);
  auto invocations = std::make_shared<serverless::InvocationManager>(store);
  auto runner = std::make_shared<serverless::FunctionRunner>(io, config);
  auto workers = std::make_shared<serverless::WorkerManager>(io, config, registry, runner);
  auto nodes = std::make_shared<serverless::NodeRegistry>();

  std::shared_ptr<serverless::Autoscaler> autoscaler;
  std::shared_ptr<serverless::Scheduler> scheduler;

  scheduler = std::make_shared<serverless::Scheduler>(io, config, registry, invocations, workers,
                                                      autoscaler);
  autoscaler = std::make_shared<serverless::Autoscaler>(
      io, config, workers,
      [scheduler](const std::string& fid) { return scheduler->queue_depth(fid); },
      [scheduler]() { return scheduler->total_queue_depth(); },
      [registry](const std::string& fid) -> const serverless::FunctionRecord* {
        if (auto fn = registry->get_function(fid)) {
          static thread_local serverless::FunctionRecord cached;
          cached = *fn;
          return &cached;
        }
        return nullptr;
      });

  workers->set_callbacks(
      [scheduler](const std::string& id) { scheduler->on_worker_idle(id); },
      [scheduler](const std::string& id) { scheduler->on_worker_failed(id); });
  workers->monitor_workers();
  autoscaler->start();
  scheduler->set_ready(true);

  serverless::HttpServer server(io, config, registry, scheduler, workers, nodes, store, invocations);
  server.start();

  boost::asio::signal_set signals(io, SIGINT, SIGTERM);
  signals.async_wait([&io](const boost::system::error_code&, int) { io.stop(); });

  serverless::StructuredLogger::info("Control plane listening on " + config.server.host + ":" +
                                     std::to_string(config.server.port));
  io.run();
  return 0;
}
