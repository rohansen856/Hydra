#include "serverless/runtime/Worker.hpp"

namespace serverless {

WorkerStateMachine::WorkerStateMachine(WorkerState initial) : state_(initial) {}

bool WorkerStateMachine::is_valid_transition(WorkerState from, WorkerState to) {
  switch (from) {
    case WorkerState::Starting:
      return to == WorkerState::Idle || to == WorkerState::Failed || to == WorkerState::Terminated;
    case WorkerState::Idle:
      return to == WorkerState::Running || to == WorkerState::Draining ||
             to == WorkerState::Terminating || to == WorkerState::Failed;
    case WorkerState::Running:
      return to == WorkerState::Idle || to == WorkerState::Failed ||
             to == WorkerState::Terminated;
    case WorkerState::Draining:
      return to == WorkerState::Terminating || to == WorkerState::Terminated;
    case WorkerState::Terminating:
      return to == WorkerState::Terminated || to == WorkerState::Failed;
    case WorkerState::Terminated:
    case WorkerState::Failed:
      return false;
  }
  return false;
}

bool WorkerStateMachine::transition(WorkerState next) {
  if (!is_valid_transition(state_, next)) {
    return false;
  }
  state_ = next;
  return true;
}

const char* WorkerStateMachine::to_string(WorkerState state) {
  switch (state) {
    case WorkerState::Starting:
      return "STARTING";
    case WorkerState::Idle:
      return "IDLE";
    case WorkerState::Running:
      return "RUNNING";
    case WorkerState::Draining:
      return "DRAINING";
    case WorkerState::Terminating:
      return "TERMINATING";
    case WorkerState::Terminated:
      return "TERMINATED";
    case WorkerState::Failed:
      return "FAILED";
  }
  return "UNKNOWN";
}

}  // namespace serverless
