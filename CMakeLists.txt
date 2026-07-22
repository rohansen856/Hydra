cmake_minimum_required(VERSION 3.20)
project(serverless-cpp VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(ENABLE_SANITIZERS "Enable AddressSanitizer and UBSan" OFF)
option(ENABLE_TSAN "Enable ThreadSanitizer" OFF)
option(BUILD_TESTS "Build tests" ON)

if(ENABLE_SANITIZERS AND ENABLE_TSAN)
  message(FATAL_ERROR "ENABLE_SANITIZERS and ENABLE_TSAN are mutually exclusive")
endif()

if(ENABLE_SANITIZERS)
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined)
elseif(ENABLE_TSAN)
  add_compile_options(-fsanitize=thread -fno-omit-frame-pointer)
  add_link_options(-fsanitize=thread)
endif()

set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/json-3.11.3)

set(SPDLOG_BUILD_SHARED OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/spdlog-1.14.1)

set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/yaml-cpp-0.8.0)

if(BUILD_TESTS)
  set(CATCH_BUILD_TESTING OFF CACHE BOOL "" FORCE)
  set(CATCH_INSTALL_DOCS OFF CACHE BOOL "" FORCE)
  add_subdirectory(third_party/Catch2-3.6.0)
endif()

add_library(sqlite3 STATIC third_party/sqlite-amalgamation-3460100/sqlite3.c)
target_include_directories(sqlite3 PUBLIC third_party/sqlite-amalgamation-3460100)
target_compile_definitions(sqlite3 PRIVATE SQLITE_THREADSAFE=1)

find_package(Boost REQUIRED COMPONENTS system)

add_library(serverless_lib STATIC
  src/common/Error.cpp
  src/common/Id.cpp
  src/common/Clock.cpp
  src/common/Json.cpp
  src/config/Config.cpp
  src/observability/StructuredLogger.cpp
  src/observability/Metrics.cpp
  src/storage/SqliteStore.cpp
  src/core/FunctionRegistry.cpp
  src/core/InvocationManager.cpp
  src/core/Scheduler.cpp
  src/core/WorkerManager.cpp
  src/core/Autoscaler.cpp
  src/runtime/Process.cpp
  src/runtime/FunctionRunner.cpp
  src/runtime/WorkerState.cpp
  src/runtime/WorkerProcess.cpp
  src/runtime/WorkerDaemon.cpp
  src/api/HttpServer.cpp
  src/api/DashboardApi.cpp
  src/api/FunctionController.cpp
  src/api/InvocationController.cpp
  src/api/NodeController.cpp
  src/api/HealthController.cpp
  src/core/NodeRegistry.cpp
  src/runtime/RemoteWorkerClient.cpp
)

target_include_directories(serverless_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(serverless_lib PUBLIC
  nlohmann_json::nlohmann_json
  spdlog::spdlog
  yaml-cpp
  Boost::system
  sqlite3
)

target_compile_options(serverless_lib PRIVATE -Wall -Wextra -Wpedantic)

add_executable(serverless src/main.cpp)
target_link_libraries(serverless PRIVATE serverless_lib)

add_executable(worker_node src/worker_node/main.cpp)
target_link_libraries(worker_node PRIVATE serverless_lib)

file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/functions)

function(add_sample_function name src)
  add_executable(${name} ${src})
  target_link_libraries(${name} PRIVATE nlohmann_json::nlohmann_json)
  target_compile_options(${name} PRIVATE -Wall -Wextra)
  set_target_properties(${name} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/functions)
endfunction()

add_sample_function(hello-function functions/hello/main.cpp)
add_sample_function(sleep-function functions/sleep/main.cpp)
add_sample_function(crash-function functions/crash/main.cpp)
add_sample_function(infinite-loop-function functions/infinite_loop/main.cpp)
add_sample_function(large-output-function functions/large_output/main.cpp)
add_sample_function(large-input-function functions/large_input/main.cpp)
add_sample_function(cpu-hog-function functions/cpu_hog/main.cpp)
add_sample_function(memory-hog-function functions/memory_hog/main.cpp)

if(BUILD_TESTS)
  enable_testing()
  include(third_party/Catch2-3.6.0/extras/Catch.cmake)
  add_executable(unit_tests
    tests/unit/SmokeTest.cpp
    tests/unit/FunctionRegistryTest.cpp
    tests/unit/SchedulerTest.cpp
    tests/unit/AutoscalerTest.cpp
    tests/unit/WorkerStateTest.cpp
    tests/unit/ProtocolTest.cpp
    tests/unit/DashboardApiTest.cpp
  )
  target_link_libraries(unit_tests PRIVATE serverless_lib Catch2::Catch2WithMain)
  catch_discover_tests(unit_tests)

  add_executable(integration_tests tests/integration/IntegrationTest.cpp)
  target_link_libraries(integration_tests PRIVATE serverless_lib Catch2::Catch2WithMain)
  catch_discover_tests(integration_tests PROPERTIES SKIP_RETURN_CODE 4)
endif()

install(TARGETS serverless worker_node RUNTIME DESTINATION bin)
install(DIRECTORY config/ DESTINATION etc/serverless)
