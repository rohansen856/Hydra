#include "serverless/common/Id.hpp"

#include <random>
#include <sstream>

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

namespace serverless {

static std::string random_id(const std::string& prefix) {
  static thread_local std::mt19937_64 rng{std::random_device{}()};
  static boost::uuids::random_generator gen;
  const auto uuid = gen();
  std::ostringstream oss;
  oss << prefix << boost::uuids::to_string(uuid);
  return oss.str();
}

std::string generate_request_id() { return random_id("req-"); }
std::string generate_worker_id() { return random_id("w-"); }
std::string generate_node_id() { return random_id("n-"); }

std::string make_function_id(const std::string& name, const std::string& version) {
  return name + ":" + version;
}

}  // namespace serverless
