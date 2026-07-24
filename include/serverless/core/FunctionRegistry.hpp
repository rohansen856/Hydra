#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "serverless/common/Error.hpp"
#include "serverless/config/Config.hpp"
#include "serverless/storage/SqliteStore.hpp"

namespace serverless {

class FunctionRegistry {
 public:
  FunctionRegistry(std::shared_ptr<SqliteStore> store, const Config& config);

  Error register_function(const nlohmann::json& request);
  std::optional<FunctionRecord> get_function(const std::string& name) const;
  Error delete_function(const std::string& name);
  std::vector<FunctionRecord> list_functions() const;
  Error validate_function_config(const nlohmann::json& request) const;

 private:
  std::shared_ptr<SqliteStore> store_;
  Config config_;

  Error copy_executable(const std::string& source, const std::string& name,
                        const std::string& version, std::string& dest) const;
};

}  // namespace serverless
