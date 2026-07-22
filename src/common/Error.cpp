#include "serverless/common/Error.hpp"

namespace serverless {

Error Error::success() { return Error{ErrorCode::Ok, ""}; }

Error Error::make(ErrorCode code, std::string message) {
  return Error{code, std::move(message)};
}

int Error::http_status() const noexcept {
  switch (code) {
    case ErrorCode::Ok:
      return 200;
    case ErrorCode::FunctionNotFound:
      return 404;
    case ErrorCode::FunctionDisabled:
    case ErrorCode::WorkerStartFailed:
    case ErrorCode::NodeUnhealthy:
      return 503;
    case ErrorCode::InvalidRequest:
    case ErrorCode::InvalidConfiguration:
    case ErrorCode::DuplicateFunction:
      return 400;
    case ErrorCode::PayloadTooLarge:
      return 413;
    case ErrorCode::QueueFull:
      return 429;
    case ErrorCode::InvocationTimeout:
      return 504;
    case ErrorCode::FunctionError:
    case ErrorCode::WorkerCrash:
      return 502;
    case ErrorCode::NodeNotFound:
      return 404;
    default:
      return 500;
  }
}

std::string Error::code_string() const {
  switch (code) {
    case ErrorCode::FunctionNotFound:
      return "FUNCTION_NOT_FOUND";
    case ErrorCode::FunctionDisabled:
      return "FUNCTION_DISABLED";
    case ErrorCode::InvalidRequest:
      return "INVALID_REQUEST";
    case ErrorCode::PayloadTooLarge:
      return "PAYLOAD_TOO_LARGE";
    case ErrorCode::QueueFull:
      return "QUEUE_FULL";
    case ErrorCode::InvocationTimeout:
      return "INVOCATION_TIMEOUT";
    case ErrorCode::FunctionError:
      return "FUNCTION_ERROR";
    case ErrorCode::WorkerStartFailed:
      return "WORKER_START_FAILED";
    case ErrorCode::WorkerCrash:
      return "WORKER_CRASH";
    case ErrorCode::DuplicateFunction:
      return "DUPLICATE_FUNCTION";
    case ErrorCode::InvalidConfiguration:
      return "INVALID_CONFIGURATION";
    case ErrorCode::DatabaseError:
      return "DATABASE_ERROR";
    case ErrorCode::NodeNotFound:
      return "NODE_NOT_FOUND";
    case ErrorCode::NodeUnhealthy:
      return "NODE_UNHEALTHY";
    default:
      return "INTERNAL_ERROR";
  }
}

}  // namespace serverless
