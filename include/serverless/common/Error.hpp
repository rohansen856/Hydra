#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace serverless {

enum class ErrorCode {
  Ok = 0,
  FunctionNotFound,
  FunctionDisabled,
  InvalidRequest,
  PayloadTooLarge,
  QueueFull,
  InvocationTimeout,
  FunctionError,
  WorkerStartFailed,
  WorkerCrash,
  InternalError,
  DuplicateFunction,
  InvalidConfiguration,
  DatabaseError,
  NodeNotFound,
  NodeUnhealthy,
};

struct Error {
  ErrorCode code{ErrorCode::Ok};
  std::string message;

  static Error success();
  static Error make(ErrorCode code, std::string message);

  bool ok() const noexcept { return code == ErrorCode::Ok; }
  int http_status() const noexcept;
  std::string code_string() const;
};

}  // namespace serverless
