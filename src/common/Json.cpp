#include "serverless/common/Json.hpp"

namespace serverless {

Error validate_payload_size(std::string_view data, std::size_t max_bytes) {
  if (data.size() > max_bytes) {
    return Error::make(ErrorCode::PayloadTooLarge, "Payload exceeds maximum allowed size");
  }
  return Error::success();
}

std::optional<InvocationRequest> parse_invocation_request(std::string_view data,
                                                          std::size_t max_bytes) {
  if (data.size() > max_bytes) {
    return std::nullopt;
  }
  try {
    auto json = nlohmann::json::parse(data);
    InvocationRequest req;
    req.request_id = json.value("request_id", "");
    req.function = json.at("function").get<std::string>();
    req.version = json.at("version").get<std::string>();
    req.payload = json.value("payload", nlohmann::json::object());
    return req;
  } catch (...) {
    return std::nullopt;
  }
}

std::optional<InvocationResponse> parse_invocation_response(std::string_view data,
                                                            std::size_t max_bytes) {
  if (data.size() > max_bytes) {
    return std::nullopt;
  }
  try {
    auto json = nlohmann::json::parse(data);
    InvocationResponse resp;
    resp.request_id = json.value("request_id", "");
    resp.status = json.value("status", 500);
    if (json.contains("body")) {
      resp.body = json["body"];
    }
    if (json.contains("error")) {
      resp.error = json["error"];
    }
    return resp;
  } catch (...) {
    return std::nullopt;
  }
}

nlohmann::json make_error_json(const Error& error, const std::string& request_id) {
  nlohmann::json j;
  if (!request_id.empty()) {
    j["request_id"] = request_id;
  }
  j["status"] = error.http_status();
  j["error"] = {{"code", error.code_string()}, {"message", error.message}};
  return j;
}

}  // namespace serverless
