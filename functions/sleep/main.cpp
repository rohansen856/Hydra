#include <chrono>
#include <iostream>
#include <thread>

#include <nlohmann/json.hpp>

int main() {
  nlohmann::json input;
  std::cin >> input;
  const int ms = input["payload"].value("sleep_ms", 100);
  std::this_thread::sleep_for(std::chrono::milliseconds(ms));
  nlohmann::json output = {{"request_id", input.value("request_id", "")},
                           {"status", 200},
                           {"body", {{"slept_ms", ms}}}};
  std::cout << output.dump() << std::endl;
  return 0;
}
