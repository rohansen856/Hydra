#include "serverless/core/FunctionRegistry.hpp"

#include <filesystem>
#include <fstream>

#include "serverless/common/Clock.hpp"
#include "serverless/common/Id.hpp"

namespace serverless {

FunctionRegistry::FunctionRegistry(std::shared_ptr<SqliteStore> store, const Config& config)
    : store_(std::move(store)), config_(config) {
  std::filesystem::create_directories(config_.storage.functions_dir);
}

Error FunctionRegistry::validate_function_config(const nlohmann::json& request) const {
  if (!request.contains("name") || !request.contains("command")) {
    return Error::make(ErrorCode::InvalidRequest, "name and command are required");
  }
  const int min_workers = request.value("min_workers", 0);
  const int max_workers = request.value("max_workers", config_.default_function.max_concurrency);
  if (min_workers < 0 || max_workers < 1 || min_workers > max_workers) {
    return Error::make(ErrorCode::InvalidConfiguration, "Invalid worker bounds");
  }
  const int timeout_ms = request.value("timeout_ms", config_.default_function.timeout_ms);
  if (timeout_ms <= 0) {
    return Error::make(ErrorCode::InvalidConfiguration, "timeout_ms must be positive");
  }
  const std::string command = request.at("command").get<std::string>();
  std::error_code ec;
  const auto cmd_path = std::filesystem::path(command);
  const auto source_dir = std::filesystem::is_directory(cmd_path, ec) ? cmd_path : cmd_path.parent_path();
  if (!std::filesystem::exists(command, ec) && !std::filesystem::exists(source_dir, ec)) {
    return Error::make(ErrorCode::InvalidConfiguration,
                       "command executable does not exist: " + command);
  }
  return Error::success();
}

Error FunctionRegistry::copy_executable(const std::string& source, const std::string& name,
                                        const std::string& version, std::string& dest) const {
  const auto dir = std::filesystem::path(config_.storage.functions_dir) / name / version;
  std::filesystem::create_directories(dir);
  std::error_code ec;
  const auto source_path = std::filesystem::path(source);

  if (std::filesystem::is_directory(source_path, ec)) {
    for (const auto& entry : std::filesystem::directory_iterator(source_path)) {
      const auto target = dir / entry.path().filename();
      if (entry.is_regular_file()) {
        std::filesystem::copy_file(entry.path(), target,
                                   std::filesystem::copy_options::overwrite_existing, ec);
        if (ec) {
          return Error::make(ErrorCode::InternalError, "Failed to copy file: " + ec.message());
        }
        if (entry.path().filename() == "run.sh") {
          std::filesystem::permissions(target, std::filesystem::perms::owner_all |
                                                    std::filesystem::perms::group_exec |
                                                    std::filesystem::perms::others_exec,
                                       ec);
        }
      }
    }
    dest = (dir / "run.sh").string();
    if (!std::filesystem::exists(dest, ec)) {
      dest = (dir / "function").string();
    }
    return Error::success();
  }

  const auto source_dir = source_path.parent_path();
  const auto filename = source_path.filename();
  if (filename == "run.sh" && std::filesystem::is_directory(source_dir, ec)) {
    for (const auto& entry : std::filesystem::directory_iterator(source_dir)) {
      if (!entry.is_regular_file()) {
        continue;
      }
      const auto target = dir / entry.path().filename();
      std::filesystem::copy_file(entry.path(), target,
                                 std::filesystem::copy_options::overwrite_existing, ec);
      if (ec) {
        return Error::make(ErrorCode::InternalError, "Failed to copy file: " + ec.message());
      }
      if (entry.path().filename() == "run.sh") {
        std::filesystem::permissions(target, std::filesystem::perms::owner_all |
                                                  std::filesystem::perms::group_exec |
                                                  std::filesystem::perms::others_exec,
                                     ec);
      }
    }
    dest = (dir / "run.sh").string();
    return Error::success();
  }

  dest = std::filesystem::absolute(dir / "function").string();
  std::filesystem::copy_file(source, dest, std::filesystem::copy_options::overwrite_existing, ec);
  if (ec) {
    return Error::make(ErrorCode::InternalError, "Failed to copy executable: " + ec.message());
  }
  std::filesystem::permissions(
      dest,
      std::filesystem::perms::owner_read | std::filesystem::perms::owner_write |
          std::filesystem::perms::owner_exec | std::filesystem::perms::group_exec |
          std::filesystem::perms::others_exec,
      ec);
  (void)ec;
  return Error::success();
}

Error FunctionRegistry::register_function(const nlohmann::json& request) {
  auto err = validate_function_config(request);
  if (!err.ok()) {
    return err;
  }

  FunctionRecord fn;
  fn.name = request.at("name").get<std::string>();
  fn.version = request.value("version", "1");
  fn.id = make_function_id(fn.name, fn.version);
  fn.min_workers = request.value("min_workers", 0);
  fn.max_workers = request.value("max_workers", 10);
  fn.timeout_ms = request.value("timeout_ms", config_.default_function.timeout_ms);
  fn.max_concurrency = request.value("max_concurrency", config_.default_function.max_concurrency);
  fn.memory_mb = request.value("memory_mb", config_.default_function.memory_mb);
  fn.created_at = Clock::now_unix_ms();

  err = copy_executable(request.at("command").get<std::string>(), fn.name, fn.version, fn.command);
  if (!err.ok()) {
    return err;
  }

  return store_->insert_function(fn);
}

std::optional<FunctionRecord> FunctionRegistry::get_function(const std::string& name) const {
  return store_->get_function_by_name(name);
}

Error FunctionRegistry::delete_function(const std::string& name) {
  return store_->delete_function(name);
}

std::vector<FunctionRecord> FunctionRegistry::list_functions() const {
  return store_->list_functions();
}

}  // namespace serverless
