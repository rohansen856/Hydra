#include <catch2/catch_test_macros.hpp>

#include "serverless/config/Config.hpp"

TEST_CASE("Autoscaler thresholds configuration") {
  serverless::Config cfg;
  cfg.scaling.busy_threshold = 0.7;
  cfg.scaling.max_scale_step = 4;
  REQUIRE(cfg.scaling.busy_threshold == 0.7);
  REQUIRE(cfg.scaling.max_scale_step == 4);
}

TEST_CASE("Config validation rejects bad port") {
  serverless::Config cfg;
  cfg.server.port = -1;
  REQUIRE_THROWS(cfg.validate());
}
