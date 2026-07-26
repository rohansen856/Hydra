#include "serverless/runtime/Process.hpp"

#include <poll.h>
#include <signal.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cstring>
#include <stdexcept>

namespace serverless {

Process::~Process() {
  if (running()) {
    kill();
    wait();
  }
  close_fds();
}

Process::Process(Process&& other) noexcept { *this = std::move(other); }

Process& Process::operator=(Process&& other) noexcept {
  if (this != &other) {
    close_fds();
    pid_ = other.pid_;
    stdin_write_ = other.stdin_write_;
    stdout_read_ = other.stdout_read_;
    stderr_read_ = other.stderr_read_;
    other.pid_ = -1;
    other.stdin_write_ = -1;
    other.stdout_read_ = -1;
    other.stderr_read_ = -1;
  }
  return *this;
}

void Process::close_fds() {
  if (stdin_write_ >= 0) {
    close(stdin_write_);
    stdin_write_ = -1;
  }
  if (stdout_read_ >= 0) {
    close(stdout_read_);
    stdout_read_ = -1;
  }
  if (stderr_read_ >= 0) {
    close(stderr_read_);
    stderr_read_ = -1;
  }
}

void Process::spawn(const std::string& executable, const std::vector<std::string>& args,
                    const ResourceLimits& limits, std::string_view stdin_data) {
  int in_pipe[2]{-1, -1};
  int out_pipe[2]{-1, -1};
  int err_pipe[2]{-1, -1};
  if (pipe(in_pipe) != 0 || pipe(out_pipe) != 0 || pipe(err_pipe) != 0) {
    throw std::runtime_error("pipe failed");
  }

  pid_ = fork();
  if (pid_ < 0) {
    close(in_pipe[0]);
    close(in_pipe[1]);
    close(out_pipe[0]);
    close(out_pipe[1]);
    close(err_pipe[0]);
    close(err_pipe[1]);
    throw std::runtime_error("fork failed");
  }

  if (pid_ == 0) {
    dup2(in_pipe[0], STDIN_FILENO);
    dup2(out_pipe[1], STDOUT_FILENO);
    dup2(err_pipe[1], STDERR_FILENO);
    close(in_pipe[0]);
    close(in_pipe[1]);
    close(out_pipe[0]);
    close(out_pipe[1]);
    close(err_pipe[0]);
    close(err_pipe[1]);

    struct rlimit rl {};
    rl.rlim_cur = limits.cpu_seconds;
    rl.rlim_max = limits.cpu_seconds;
    setrlimit(RLIMIT_CPU, &rl);

    std::vector<char*> argv;
    argv.reserve(args.size() + 2);
    argv.push_back(const_cast<char*>(executable.c_str()));
    for (const auto& arg : args) {
      argv.push_back(const_cast<char*>(arg.c_str()));
    }
    argv.push_back(nullptr);
    execv(executable.c_str(), argv.data());
    _exit(127);
  }

  close(in_pipe[0]);
  close(out_pipe[1]);
  close(err_pipe[1]);
  stdin_write_ = in_pipe[1];
  stdout_read_ = out_pipe[0];
  stderr_read_ = err_pipe[0];

  if (!stdin_data.empty()) {
    write_stdin(stdin_data);
  }
}

void Process::write_stdin(std::string_view data) {
  if (stdin_write_ < 0) {
    return;
  }
  std::size_t written = 0;
  while (written < data.size()) {
    const ssize_t n = write(stdin_write_, data.data() + written, data.size() - written);
    if (n <= 0) {
      break;
    }
    written += static_cast<std::size_t>(n);
  }
  close(stdin_write_);
  stdin_write_ = -1;
}

std::string Process::read_stdout() {
  std::string out;
  if (stdout_read_ < 0) {
    return out;
  }
  char buf[4096];
  while (true) {
    pollfd pfd{.fd = stdout_read_, .events = POLLIN};
    const int pr = poll(&pfd, 1, 5000);
    if (pr <= 0) {
      break;
    }
    const ssize_t n = read(stdout_read_, buf, sizeof(buf));
    if (n <= 0) {
      break;
    }
    out.append(buf, static_cast<std::size_t>(n));
  }
  close(stdout_read_);
  stdout_read_ = -1;
  return out;
}

std::string Process::read_stderr_nonblocking() {
  std::string err;
  if (stderr_read_ < 0) {
    return err;
  }
  char buf[4096];
  while (true) {
    pollfd pfd{.fd = stderr_read_, .events = POLLIN};
    const int pr = poll(&pfd, 1, 0);
    if (pr <= 0) {
      break;
    }
    const ssize_t n = read(stderr_read_, buf, sizeof(buf));
    if (n <= 0) {
      break;
    }
    err.append(buf, static_cast<std::size_t>(n));
  }
  return err;
}

void Process::terminate() {
  if (pid_ > 0) {
    ::kill(pid_, SIGTERM);
  }
}

void Process::kill() {
  if (pid_ > 0) {
    ::kill(pid_, SIGKILL);
  }
}

int Process::wait() {
  if (pid_ <= 0 && reaped_) {
    close_fds();
    if (WIFEXITED(exit_status_)) {
      return WEXITSTATUS(exit_status_);
    }
    if (WIFSIGNALED(exit_status_)) {
      return 128 + WTERMSIG(exit_status_);
    }
    return exit_status_;
  }
  if (pid_ <= 0) {
    return -1;
  }
  int status = 0;
  waitpid(pid_, &status, 0);
  pid_ = -1;
  reaped_ = true;
  exit_status_ = status;
  close_fds();
  if (WIFEXITED(status)) {
    return WEXITSTATUS(status);
  }
  if (WIFSIGNALED(status)) {
    return 128 + WTERMSIG(status);
  }
  return status;
}

bool Process::running() const noexcept { return pid_ > 0 && !reaped_; }

bool Process::poll_exited() {
  if (reaped_ || pid_ <= 0) {
    return true;
  }
  int status = 0;
  const pid_t r = waitpid(pid_, &status, WNOHANG);
  if (r == 0) {
    return false;
  }
  reaped_ = true;
  exit_status_ = status;
  pid_ = -1;
  return true;
}

}  // namespace serverless
