#include "serverless/runtime/RemoteWorkerClient.hpp"

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

namespace serverless {

namespace beast = boost::beast;
namespace http = beast::http;
using tcp = boost::asio::ip::tcp;

RemoteWorkerClient::RemoteWorkerClient(boost::asio::io_context& io, const std::string& host,
                                       int port)
    : io_(io), host_(host), port_(port) {}

void RemoteWorkerClient::invoke_async(const FunctionRecord& fn, const std::string& request_id,
                                      const nlohmann::json& payload,
                                      std::function<void(Error, nlohmann::json, double)> callback) {
  boost::asio::post(io_, [this, fn, request_id, payload, cb = std::move(callback)]() mutable {
    try {
      tcp::resolver resolver(io_);
      beast::tcp_stream stream(io_);
      auto const results = resolver.resolve(host_, std::to_string(port_));
      stream.connect(results);

      nlohmann::json body = {{"function", fn.name}, {"request_id", request_id}, {"payload", payload}};
      http::request<http::string_body> req{http::verb::post, "/internal/v1/invoke",
                                           11};
      req.set(http::field::host, host_);
      req.set(http::field::content_type, "application/json");
      req.body() = body.dump();
      req.prepare_payload();
      http::write(stream, req);

      beast::flat_buffer buffer;
      http::response<http::string_body> res;
      http::read(stream, buffer, res);
      stream.socket().shutdown(tcp::socket::shutdown_both);

      auto result = nlohmann::json::parse(res.body());
      cb(Error::success(), result, result.value("duration_ms", 0.0));
    } catch (const std::exception& ex) {
      cb(Error::make(ErrorCode::InternalError, ex.what()), {}, 0);
    }
  });
}

}  // namespace serverless
