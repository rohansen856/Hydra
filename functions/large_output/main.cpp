#include <iostream>
#include <string>

#include <nlohmann/json.hpp>

int main() {
  nlohmann::json input;
  std::cin >> input;
  std::string big(2 * 1024 * 1024, 'x');
  nlohmann::json output = {{"request_id", input.value("request_id", "")},
                           {"status", 200},
                           {"body", {{"data", big}}}};
  std::cout << output.dump() << std::endl;
  return 0;
}
