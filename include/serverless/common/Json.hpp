#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

#include "serverless/common/Error.hpp"

namespace serverless {

struct InvocationRequest {
  std::string request_id;
  std::string function;
  std::string version;
  nlohmann::json payload;
};

struct InvocationResponse {
  std::string request_id;
  int status{500};
  nlohmann::json body;
  nlohmann::json error;
};

Error validate_payload_size(std::string_view data, std::size_t max_bytes);
std::optional<InvocationRequest> parse_invocation_request(std::string_view data,
                                                          std::size_t max_bytes);
std::optional<InvocationResponse> parse_invocation_response(std::string_view data,
                                                            std::size_t max_bytes);
nlohmann::json make_error_json(const Error& error, const std::string& request_id = {});

}  // namespace serverless
