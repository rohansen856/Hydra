#include <catch2/catch_test_macros.hpp>

#include "serverless/runtime/Worker.hpp"

TEST_CASE("Worker legal transitions") {
  serverless::WorkerStateMachine sm(serverless::WorkerState::Starting);
  REQUIRE(sm.transition(serverless::WorkerState::Idle));
  REQUIRE(sm.transition(serverless::WorkerState::Running));
  REQUIRE(sm.transition(serverless::WorkerState::Idle));
}

TEST_CASE("Worker illegal transitions rejected") {
  serverless::WorkerStateMachine sm(serverless::WorkerState::Terminated);
  REQUIRE_FALSE(sm.transition(serverless::WorkerState::Idle));
}

TEST_CASE("Worker crash to failed") {
  serverless::WorkerStateMachine sm(serverless::WorkerState::Running);
  REQUIRE(sm.transition(serverless::WorkerState::Failed));
}
