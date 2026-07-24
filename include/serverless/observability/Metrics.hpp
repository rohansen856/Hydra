#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

namespace serverless {

class Metrics {
 public:
  static Metrics& instance();

  void inc_counter(const std::string& name,
                   const std::unordered_map<std::string, std::string>& labels = {},
                   double value = 1.0);
  void observe_histogram(const std::string& name, double value,
                         const std::unordered_map<std::string, std::string>& labels = {});
  void set_gauge(const std::string& name, double value,
                 const std::unordered_map<std::string, std::string>& labels = {});

  std::string render_prometheus() const;
  nlohmann::json render_json() const;

 private:
  Metrics() = default;

  mutable std::mutex mutex_;
  std::unordered_map<std::string, double> counters_;
  std::unordered_map<std::string, double> gauges_;
  struct HistBucket {
    std::vector<double> bounds{0.001, 0.005, 0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1, 2.5, 5, 10};
    std::vector<std::uint64_t> counts;
    double sum{0};
    std::uint64_t count{0};
  };
  std::unordered_map<std::string, HistBucket> histograms_;

  static std::string label_key(const std::string& name,
                               const std::unordered_map<std::string, std::string>& labels);
  static std::string format_labels(const std::unordered_map<std::string, std::string>& labels);
  static void parse_metric_key(const std::string& key, std::string& name,
                               std::unordered_map<std::string, std::string>& labels);
};

}  // namespace serverless
