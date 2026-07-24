#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "serverless/common/Error.hpp"

namespace serverless {

struct InvocationQuery {
  int limit{50};
  int offset{0};
  std::optional<std::string> function;
  std::optional<std::string> status;
  std::optional<std::int64_t> since_ms;
};

struct FunctionRecord {
  std::string id;
  std::string name;
  std::string version;
  std::string command;
  int min_workers{0};
  int max_workers{10};
  int timeout_ms{5000};
  int max_concurrency{10};
  int memory_mb{128};
  std::int64_t created_at{0};
  std::string status{"ACTIVE"};
};

struct InvocationRecord {
  std::string request_id;
  std::string function_id;
  std::string status;
  std::int64_t started_at{0};
  std::int64_t finished_at{0};
  double duration_ms{0};
  std::string error_code;
};

class SqliteStore {
 public:
  explicit SqliteStore(std::string path);
  ~SqliteStore();

  SqliteStore(const SqliteStore&) = delete;
  SqliteStore& operator=(const SqliteStore&) = delete;

  Error init_schema();
  Error insert_function(const FunctionRecord& fn);
  std::optional<FunctionRecord> get_function_by_name(const std::string& name);
  std::optional<FunctionRecord> get_function_by_id(const std::string& id);
  std::vector<FunctionRecord> list_functions();
  Error delete_function(const std::string& name);
  Error insert_invocation(const InvocationRecord& inv);
  std::vector<InvocationRecord> list_invocations(const InvocationQuery& query) const;
  std::size_t count_invocations(const InvocationQuery& query) const;
  bool ping() const;

 private:
  std::string path_;
  void* db_{nullptr};  // sqlite3*
};

}  // namespace serverless
