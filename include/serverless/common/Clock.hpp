#pragma once

#include <chrono>
#include <cstdint>

namespace serverless {

class Clock {
 public:
  static std::int64_t now_unix_ms();
  static std::chrono::steady_clock::time_point steady_now();
  static double elapsed_ms(std::chrono::steady_clock::time_point start);
};

}  // namespace serverless
