#include "serverless/common/Clock.hpp"

namespace serverless {

std::int64_t Clock::now_unix_ms() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

std::chrono::steady_clock::time_point Clock::steady_now() {
  return std::chrono::steady_clock::now();
}

double Clock::elapsed_ms(std::chrono::steady_clock::time_point start) {
  return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start)
      .count();
}

}  // namespace serverless
