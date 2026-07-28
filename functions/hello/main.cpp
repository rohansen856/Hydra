#include <iostream>
#include <string>

#include <nlohmann/json.hpp>

int main() {
  try {
    std::string raw((std::istreambuf_iterator<char>(std::cin)), std::istreambuf_iterator<char>());
    const auto input = nlohmann::json::parse(raw);
    const auto& payload =
        input.contains("payload") && input["payload"].is_object() ? input["payload"] : input;
    const auto name = payload.value("name", "World");
    nlohmann::json output = {{"request_id", input.value("request_id", "")},
                             {"status", 200},
                             {"body", {{"message", "Hello " + name}}}};
    std::cout << output.dump() << std::endl;
    return 0;
  } catch (const std::exception& ex) {
    std::cerr << ex.what() << std::endl;
    nlohmann::json output = {{"status", 500},
                             {"error", {{"code", "FUNCTION_ERROR"}, {"message", ex.what()}}}};
    std::cout << output.dump() << std::endl;
    return 1;
  }
}
