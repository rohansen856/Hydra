#include <catch2/catch_test_macros.hpp>

#include "serverless/common/Json.hpp"

TEST_CASE("Protocol valid JSON") {
  const std::string data =
      R"({"request_id":"r1","function":"hello","version":"1","payload":{"name":"A"}})";
  auto req = serverless::parse_invocation_request(data, 1024);
  REQUIRE(req.has_value());
  REQUIRE(req->function == "hello");
}

TEST_CASE("Protocol malformed JSON") {
  REQUIRE_FALSE(serverless::parse_invocation_request("{bad", 1024).has_value());
}

TEST_CASE("Protocol oversized payload") {
  std::string big(2000, 'a');
  REQUIRE_FALSE(serverless::parse_invocation_request(big, 1000).has_value());
}

TEST_CASE("Protocol valid response") {
  const std::string data = R"({"request_id":"r1","status":200,"body":{"ok":true}})";
  auto resp = serverless::parse_invocation_response(data, 1024);
  REQUIRE(resp.has_value());
  REQUIRE(resp->status == 200);
}
