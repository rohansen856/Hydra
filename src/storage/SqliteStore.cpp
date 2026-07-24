#include "serverless/storage/SqliteStore.hpp"

#include <filesystem>

#include "serverless/common/Clock.hpp"

extern "C" {
#include "sqlite3.h"
}

namespace serverless {

namespace {

Error from_sqlite(int rc, const char* context) {
  if (rc == SQLITE_OK || rc == SQLITE_DONE || rc == SQLITE_ROW) {
    return Error::success();
  }
  return Error::make(ErrorCode::DatabaseError, std::string(context) + " failed: " + std::to_string(rc));
}

FunctionRecord row_to_function(sqlite3_stmt* stmt) {
  FunctionRecord fn;
  fn.id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
  fn.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
  fn.version = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
  fn.command = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
  fn.min_workers = sqlite3_column_int(stmt, 4);
  fn.max_workers = sqlite3_column_int(stmt, 5);
  fn.timeout_ms = sqlite3_column_int(stmt, 6);
  fn.max_concurrency = sqlite3_column_int(stmt, 7);
  fn.memory_mb = sqlite3_column_int(stmt, 8);
  fn.created_at = sqlite3_column_int64(stmt, 9);
  fn.status = "ACTIVE";
  return fn;
}

}  // namespace

SqliteStore::SqliteStore(std::string path) : path_(std::move(path)) {
  if (path_ != ":memory:") {
    std::filesystem::create_directories(std::filesystem::path(path_).parent_path());
  }
  sqlite3* db = nullptr;
  if (sqlite3_open(path_.c_str(), &db) != SQLITE_OK) {
    throw std::runtime_error("Failed to open sqlite database");
  }
  db_ = db;
}

SqliteStore::~SqliteStore() {
  if (db_) {
    sqlite3_close(static_cast<sqlite3*>(db_));
  }
}

Error SqliteStore::init_schema() {
  auto* db = static_cast<sqlite3*>(db_);
  const char* sql = R"(
    CREATE TABLE IF NOT EXISTS functions (
      id TEXT PRIMARY KEY,
      name TEXT NOT NULL,
      version TEXT NOT NULL,
      command TEXT NOT NULL,
      min_workers INTEGER NOT NULL DEFAULT 0,
      max_workers INTEGER NOT NULL DEFAULT 10,
      timeout_ms INTEGER NOT NULL DEFAULT 5000,
      max_concurrency INTEGER NOT NULL DEFAULT 10,
      memory_mb INTEGER NOT NULL DEFAULT 128,
      created_at INTEGER NOT NULL,
      UNIQUE(name, version)
    );
    CREATE TABLE IF NOT EXISTS invocations (
      request_id TEXT PRIMARY KEY,
      function_id TEXT NOT NULL,
      status TEXT NOT NULL,
      started_at INTEGER,
      finished_at INTEGER,
      duration_ms REAL,
      error_code TEXT
    );
  )";
  char* err = nullptr;
  int rc = sqlite3_exec(db, sql, nullptr, nullptr, &err);
  if (rc != SQLITE_OK) {
    std::string msg = err ? err : "schema init failed";
    sqlite3_free(err);
    return Error::make(ErrorCode::DatabaseError, msg);
  }
  return Error::success();
}

Error SqliteStore::insert_function(const FunctionRecord& fn) {
  auto* db = static_cast<sqlite3*>(db_);
  const char* sql =
      "INSERT INTO functions (id,name,version,command,min_workers,max_workers,timeout_ms,"
      "max_concurrency,memory_mb,created_at) VALUES (?,?,?,?,?,?,?,?,?,?)";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return Error::make(ErrorCode::DatabaseError, sqlite3_errmsg(db));
  }
  sqlite3_bind_text(stmt, 1, fn.id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, fn.name.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 3, fn.version.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 4, fn.command.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, 5, fn.min_workers);
  sqlite3_bind_int(stmt, 6, fn.max_workers);
  sqlite3_bind_int(stmt, 7, fn.timeout_ms);
  sqlite3_bind_int(stmt, 8, fn.max_concurrency);
  sqlite3_bind_int(stmt, 9, fn.memory_mb);
  sqlite3_bind_int64(stmt, 10, fn.created_at);
  int rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  if (rc == SQLITE_CONSTRAINT) {
    return Error::make(ErrorCode::DuplicateFunction, "Function version already exists");
  }
  return from_sqlite(rc, "insert_function");
}

std::optional<FunctionRecord> SqliteStore::get_function_by_name(const std::string& name) {
  auto* db = static_cast<sqlite3*>(db_);
  const char* sql = "SELECT id,name,version,command,min_workers,max_workers,timeout_ms,"
                    "max_concurrency,memory_mb,created_at FROM functions WHERE name=? ORDER BY "
                    "created_at DESC LIMIT 1";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return std::nullopt;
  }
  sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
  std::optional<FunctionRecord> result;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    result = row_to_function(stmt);
  }
  sqlite3_finalize(stmt);
  return result;
}

std::optional<FunctionRecord> SqliteStore::get_function_by_id(const std::string& id) {
  auto* db = static_cast<sqlite3*>(db_);
  const char* sql = "SELECT id,name,version,command,min_workers,max_workers,timeout_ms,"
                    "max_concurrency,memory_mb,created_at FROM functions WHERE id=?";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return std::nullopt;
  }
  sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);
  std::optional<FunctionRecord> result;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    result = row_to_function(stmt);
  }
  sqlite3_finalize(stmt);
  return result;
}

std::vector<FunctionRecord> SqliteStore::list_functions() {
  auto* db = static_cast<sqlite3*>(db_);
  const char* sql = "SELECT id,name,version,command,min_workers,max_workers,timeout_ms,"
                    "max_concurrency,memory_mb,created_at FROM functions ORDER BY name";
  sqlite3_stmt* stmt = nullptr;
  std::vector<FunctionRecord> out;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return out;
  }
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    out.push_back(row_to_function(stmt));
  }
  sqlite3_finalize(stmt);
  return out;
}

Error SqliteStore::delete_function(const std::string& name) {
  auto* db = static_cast<sqlite3*>(db_);
  const char* sql = "DELETE FROM functions WHERE name=?";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return Error::make(ErrorCode::DatabaseError, sqlite3_errmsg(db));
  }
  sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
  int rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  if (sqlite3_changes(db) == 0) {
    return Error::make(ErrorCode::FunctionNotFound, "Function not found");
  }
  return from_sqlite(rc, "delete_function");
}

Error SqliteStore::insert_invocation(const InvocationRecord& inv) {
  auto* db = static_cast<sqlite3*>(db_);
  const char* sql =
      "INSERT OR REPLACE INTO invocations (request_id,function_id,status,started_at,finished_at,"
      "duration_ms,error_code) VALUES (?,?,?,?,?,?,?)";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
    return Error::make(ErrorCode::DatabaseError, sqlite3_errmsg(db));
  }
  sqlite3_bind_text(stmt, 1, inv.request_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, inv.function_id.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 3, inv.status.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(stmt, 4, inv.started_at);
  sqlite3_bind_int64(stmt, 5, inv.finished_at);
  sqlite3_bind_double(stmt, 6, inv.duration_ms);
  sqlite3_bind_text(stmt, 7, inv.error_code.c_str(), -1, SQLITE_TRANSIENT);
  int rc = sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  return from_sqlite(rc, "insert_invocation");
}

bool SqliteStore::ping() const {
  auto* db = static_cast<sqlite3*>(db_);
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db, "SELECT 1", -1, &stmt, nullptr) != SQLITE_OK) {
    return false;
  }
  bool ok = sqlite3_step(stmt) == SQLITE_ROW;
  sqlite3_finalize(stmt);
  return ok;
}

namespace {

InvocationRecord row_to_invocation(sqlite3_stmt* stmt) {
  InvocationRecord inv;
  inv.request_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
  inv.function_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
  inv.status = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
  inv.started_at = sqlite3_column_int64(stmt, 3);
  inv.finished_at = sqlite3_column_int64(stmt, 4);
  inv.duration_ms = sqlite3_column_double(stmt, 5);
  if (sqlite3_column_text(stmt, 6)) {
    inv.error_code = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
  }
  return inv;
}

std::string build_invocation_where(const InvocationQuery& query) {
  std::string where = "1=1";
  if (query.function) {
    where += " AND (function_id = ? OR function_id LIKE ?)";
  }
  if (query.status) {
    where += " AND status = ?";
  }
  if (query.since_ms) {
    where += " AND finished_at >= ?";
  }
  return where;
}

int bind_invocation_filters(sqlite3_stmt* stmt, const InvocationQuery& query, int idx) {
  if (query.function) {
    sqlite3_bind_text(stmt, idx++, query.function->c_str(), -1, SQLITE_TRANSIENT);
    const std::string like = *query.function + ":%";
    sqlite3_bind_text(stmt, idx++, like.c_str(), -1, SQLITE_TRANSIENT);
  }
  if (query.status) {
    sqlite3_bind_text(stmt, idx++, query.status->c_str(), -1, SQLITE_TRANSIENT);
  }
  if (query.since_ms) {
    sqlite3_bind_int64(stmt, idx++, *query.since_ms);
  }
  return idx;
}

}  // namespace

std::vector<InvocationRecord> SqliteStore::list_invocations(const InvocationQuery& query) const {
  auto* db = static_cast<sqlite3*>(db_);
  const std::string where = build_invocation_where(query);
  const std::string sql = "SELECT request_id,function_id,status,started_at,finished_at,"
                          "duration_ms,error_code FROM invocations WHERE " +
                          where + " ORDER BY finished_at DESC LIMIT ? OFFSET ?";

  sqlite3_stmt* stmt = nullptr;
  std::vector<InvocationRecord> out;
  if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
    return out;
  }
  int bind_idx = bind_invocation_filters(stmt, query, 1);
  sqlite3_bind_int(stmt, bind_idx++, query.limit);
  sqlite3_bind_int(stmt, bind_idx++, query.offset);

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    out.push_back(row_to_invocation(stmt));
  }
  sqlite3_finalize(stmt);
  return out;
}

std::size_t SqliteStore::count_invocations(const InvocationQuery& query) const {
  auto* db = static_cast<sqlite3*>(db_);
  const std::string where = build_invocation_where(query);
  const std::string sql = "SELECT COUNT(*) FROM invocations WHERE " + where;

  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
    return 0;
  }
  bind_invocation_filters(stmt, query, 1);
  std::size_t count = 0;
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    count = static_cast<std::size_t>(sqlite3_column_int64(stmt, 0));
  }
  sqlite3_finalize(stmt);
  return count;
}

}  // namespace serverless
