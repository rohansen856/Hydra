#include <catch2/catch_test_macros.hpp>

#include <cstdlib>
#include <string>

TEST_CASE("integration placeholder") { REQUIRE(true); }

TEST_CASE("HTTP E2E when SERVERLESS_E2E_URL is set") {
  const char* url = std::getenv("SERVERLESS_E2E_URL");
  if (url == nullptr || std::string(url).empty()) {
    SKIP("Set SERVERLESS_E2E_URL to run HTTP integration tests");
  }
  const std::string cmd =
      std::string("scripts/e2e-test.sh ") + url + " >/dev/null 2>&1";
  const int rc = std::system(cmd.c_str());
  REQUIRE(rc == 0);
}
