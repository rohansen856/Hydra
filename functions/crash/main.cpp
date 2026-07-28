#include <iostream>

#include <nlohmann/json.hpp>

int main() {
  nlohmann::json input;
  std::cin >> input;
  std::cerr << "crash-function invoked\n";
  return 1;
}
