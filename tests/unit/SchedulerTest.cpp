#include <catch2/catch_test_macros.hpp>

#include "serverless/config/Config.hpp"

TEST_CASE("Scheduler queue limit config") {
  serverless::Config cfg;
  cfg.scheduler.queue_limit = 100;
  REQUIRE(cfg.scheduler.queue_limit == 100);
}

TEST_CASE("Scheduler queue timeout config") {
  serverless::Config cfg;
  cfg.scheduler.queue_timeout_ms = 3000;
  REQUIRE(cfg.scheduler.queue_timeout_ms == 3000);
}
