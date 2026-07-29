#include <iostream>
#include <vector>

#include <nlohmann/json.hpp>

int main() {
  nlohmann::json input;
  std::cin >> input;
  std::vector<char> hog(256 * 1024 * 1024);
  hog[0] = 1;
  nlohmann::json output = {{"request_id", input.value("request_id", "")},
                           {"status", 200},
                           {"body", {{"allocated_mb", 256}}}};
  std::cout << output.dump() << std::endl;
  return 0;
}
