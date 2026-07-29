#include <iostream>

#include <nlohmann/json.hpp>

int main() {
  std::string data(2 * 1024 * 1024, 'y');
  std::cin >> data;
  nlohmann::json output = {{"status", 200}, {"body", {{"ok", true}}}};
  std::cout << output.dump() << std::endl;
  return 0;
}
