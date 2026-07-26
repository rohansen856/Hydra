#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <sys/types.h>
#include <vector>

namespace serverless {

struct ResourceLimits {
  std::size_t memory_bytes{128 * 1024 * 1024};
  int cpu_seconds{60};
  std::size_t max_output_bytes{1048576};
};

class Process {
 public:
  Process() = default;
  ~Process();

  Process(const Process&) = delete;
  Process& operator=(const Process&) = delete;
  Process(Process&& other) noexcept;
  Process& operator=(Process&& other) noexcept;

  void spawn(const std::string& executable, const std::vector<std::string>& args,
             const ResourceLimits& limits, std::string_view stdin_data = {});
  void write_stdin(std::string_view data);
  std::string read_stdout();
  std::string read_stderr_nonblocking();
  void terminate();
  void kill();
  int wait();
  bool running() const noexcept;
  bool poll_exited();
  pid_t pid() const noexcept { return pid_; }

 private:
  pid_t pid_{-1};
  int stdin_write_{-1};
  int stdout_read_{-1};
  int stderr_read_{-1};
  bool reaped_{false};
  int exit_status_{0};
  void close_fds();
};

}  // namespace serverless
