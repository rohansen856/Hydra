#include "serverless/runtime/FunctionRunner.hpp"

#include <future>
#include <thread>

#include "serverless/common/Clock.hpp"
#include "serverless/common/Json.hpp"
#include "serverless/observability/Metrics.hpp"

namespace serverless {

FunctionRunner::FunctionRunner(boost::asio::io_context& io, const Config& config)
    : io_(io), config_(config) {}

void FunctionRunner::run_async(const FunctionRecord& fn, const std::string& request_id,
                               const nlohmann::json& payload,
                               std::function<void(Error, nlohmann::json, double)> callback) {
  std::thread([this, fn, request_id, payload, cb = std::move(callback)]() mutable {
    const auto start = Clock::steady_now();
    nlohmann::json req = nlohmann::json::object();
    req["request_id"] = request_id;
    req["function"] = fn.name;
    req["version"] = fn.version;
    req["payload"] = payload;
    const auto req_str = req.dump();
    if (auto err = validate_payload_size(req_str, config_.limits.max_request_bytes); !err.ok()) {
      boost::asio::post(io_, [cb = std::move(cb), err]() mutable { cb(err, {}, 0); });
      return;
    }

    Process proc;
    try {
      ResourceLimits limits;
      limits.memory_bytes = static_cast<std::size_t>(fn.memory_mb) * 1024 * 1024;
      limits.cpu_seconds = std::max(1, fn.timeout_ms / 1000);
      limits.max_output_bytes = config_.limits.max_response_bytes;
      proc.spawn(fn.command, {}, limits, req_str);
    } catch (const std::exception& ex) {
      Error spawn_err = Error::make(ErrorCode::WorkerStartFailed, ex.what());
      boost::asio::post(io_, [cb = std::move(cb), spawn_err]() mutable { cb(spawn_err, {}, 0); });
      return;
    }

    const auto deadline = Clock::steady_now() + std::chrono::milliseconds(fn.timeout_ms);
    while (!proc.poll_exited() && Clock::steady_now() < deadline) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    Error err = Error::success();
    std::string output;
    if (!proc.poll_exited()) {
      proc.terminate();
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      if (!proc.poll_exited()) {
        proc.kill();
      }
      err = Error::make(ErrorCode::InvocationTimeout, "Function exceeded wall time");
    } else {
      output = proc.read_stdout();
      const int code = proc.wait();
      if (code != 0) {
        const auto err_out = proc.read_stderr_nonblocking();
        err = Error::make(ErrorCode::FunctionError,
                          "Function exited with status " + std::to_string(code) + " output=" + output +
                              (err_out.empty() ? "" : " stderr=" + err_out));
      }
    }

    const double duration_ms = Clock::elapsed_ms(start);

    if (!err.ok()) {
      Metrics::instance().inc_counter("worker_timeout_total", {{"function", fn.id}});
      boost::asio::post(io_, [cb = std::move(cb), err, duration_ms]() mutable {
        cb(err, {}, duration_ms);
      });
      return;
    }

    auto parsed = parse_invocation_response(output, config_.limits.max_response_bytes);
    if (!parsed) {
      Error parse_err =
          Error::make(ErrorCode::FunctionError, "Function returned malformed JSON output");
      boost::asio::post(io_, [cb = std::move(cb), parse_err, duration_ms]() mutable {
        cb(parse_err, {}, duration_ms);
      });
      return;
    }

    nlohmann::json result = {{"request_id", parsed->request_id},
                             {"status", parsed->status},
                             {"duration_ms", duration_ms}};
    if (!parsed->body.is_null()) {
      result["body"] = parsed->body;
    }
    if (!parsed->error.is_null()) {
      result["error"] = parsed->error;
    }

    boost::asio::post(io_, [cb = std::move(cb), result, duration_ms]() mutable {
      cb(Error::success(), result, duration_ms);
    });
  }).detach();
}

}  // namespace serverless
