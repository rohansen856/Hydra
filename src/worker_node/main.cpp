#include <iostream>

#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>

#include "serverless/api/HttpServer.hpp"
#include "serverless/common/Id.hpp"
#include "serverless/config/Config.hpp"
#include "serverless/core/NodeRegistry.hpp"
#include "serverless/observability/StructuredLogger.hpp"
#include "serverless/runtime/FunctionRunner.hpp"

int main(int argc, char** argv) {
  const std::string config_path = argc > 1 ? argv[1] : "config/config.yaml";
  const std::string control_host = argc > 2 ? argv[2] : "127.0.0.1";
  const int control_port = argc > 3 ? std::stoi(argv[3]) : 8080;

  serverless::Config config = serverless::Config::load_from_file(config_path);
  serverless::StructuredLogger::init(config.logging.level);

  boost::asio::io_context io;
  auto nodes = std::make_shared<serverless::NodeRegistry>();
  const auto node_id = serverless::generate_node_id();

  boost::asio::steady_timer heartbeat(io);
  std::function<void()> beat;
  beat = [&]() {
    heartbeat.expires_after(std::chrono::milliseconds(config.node.heartbeat_interval_ms));
    heartbeat.async_wait([&](const boost::system::error_code& ec) {
      if (ec) {
        return;
      }
      nodes->heartbeat(node_id, 0, 1);
      beat();
    });
  };

  nodes->register_node(node_id, control_host, control_port, 1, 512);
  beat();

  serverless::StructuredLogger::info("Worker node " + node_id + " started");
  io.run();
  return 0;
}
