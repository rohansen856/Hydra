#pragma once

#include <string>

namespace serverless {

std::string generate_request_id();
std::string generate_worker_id();
std::string generate_node_id();
std::string make_function_id(const std::string& name, const std::string& version);

}  // namespace serverless
