#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <memory>

#include "serverless/config/Config.hpp"
#include "serverless/core/FunctionRegistry.hpp"
#include "serverless/storage/SqliteStore.hpp"

TEST_CASE("FunctionRegistry register and lookup") {
  const auto db = std::filesystem::temp_directory_path() / "fr_test.db";
  std::filesystem::remove(db);
  auto store = std::make_shared<serverless::SqliteStore>(db.string());
  REQUIRE(store->init_schema().ok());
  serverless::Config cfg;
  const auto fn_dir = std::filesystem::temp_directory_path() / "serverless_fn_reg";
  std::filesystem::remove_all(fn_dir);
  cfg.storage.functions_dir = fn_dir.string();
  serverless::FunctionRegistry registry(store, cfg);

  const std::string hello_bin = std::filesystem::absolute("functions/hello-function").string();
  if (!std::filesystem::exists(hello_bin)) {
    SKIP("hello-function binary not built");
  }
  nlohmann::json req = {{"name", "hello"},
                        {"version", "1"},
                        {"command", hello_bin},
                        {"min_workers", 1},
                        {"max_workers", 2}};
  const auto reg = registry.register_function(req);
  INFO(reg.message);
  REQUIRE(reg.ok());
  auto fn = registry.get_function("hello");
  REQUIRE(fn.has_value());
  REQUIRE(fn->name == "hello");
}

TEST_CASE("FunctionRegistry duplicate rejection") {
  const auto db = std::filesystem::temp_directory_path() / "fr_dup.db";
  std::filesystem::remove(db);
  auto store = std::make_shared<serverless::SqliteStore>(db.string());
  store->init_schema();
  serverless::Config cfg;
  const auto fn_dir = std::filesystem::temp_directory_path() / "serverless_fn_dup";
  std::filesystem::remove_all(fn_dir);
  cfg.storage.functions_dir = fn_dir.string();
  serverless::FunctionRegistry registry(store, cfg);
  nlohmann::json req = {{"name", "x"}, {"version", "1"}, {"command", "/bin/true"}};
  const auto first = registry.register_function(req);
  INFO(first.message);
  REQUIRE(first.ok());
  REQUIRE_FALSE(registry.register_function(req).ok());
}

TEST_CASE("FunctionRegistry invalid configuration") {
  serverless::Config cfg;
  auto store = std::make_shared<serverless::SqliteStore>(":memory:");
  store->init_schema();
  serverless::FunctionRegistry registry(store, cfg);
  nlohmann::json req = {{"name", "bad"}, {"version", "1"}, {"min_workers", 5}, {"max_workers", 1}};
  REQUIRE_FALSE(registry.register_function(req).ok());
}
