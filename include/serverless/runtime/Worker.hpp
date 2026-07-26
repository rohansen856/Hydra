#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

namespace serverless {

enum class WorkerState {
  Starting,
  Idle,
  Running,
  Draining,
  Terminating,
  Terminated,
  Failed,
};

class WorkerStateMachine {
 public:
  explicit WorkerStateMachine(WorkerState initial = WorkerState::Starting);

  WorkerState state() const noexcept { return state_; }
  bool transition(WorkerState next);
  bool can_dispatch() const noexcept { return state_ == WorkerState::Idle; }
  static const char* to_string(WorkerState state);

 private:
  WorkerState state_;
  static bool is_valid_transition(WorkerState from, WorkerState to);
};

struct WorkerInfo {
  std::string id;
  std::string function_id;
  std::string node_id{"local"};
  WorkerState state{WorkerState::Starting};
  pid_t pid{-1};
  std::chrono::steady_clock::time_point idle_since;
  std::chrono::steady_clock::time_point started_at;
  bool is_remote{false};
};

}  // namespace serverless
