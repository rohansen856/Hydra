#include "serverless/observability/Metrics.hpp"

#include <algorithm>
#include <sstream>

namespace serverless {

Metrics& Metrics::instance() {
  static Metrics metrics;
  return metrics;
}

std::string Metrics::label_key(const std::string& name,
                               const std::unordered_map<std::string, std::string>& labels) {
  std::ostringstream oss;
  oss << name;
  for (const auto& [k, v] : labels) {
    oss << "{" << k << "=" << v << "}";
  }
  return oss.str();
}

std::string Metrics::format_labels(const std::unordered_map<std::string, std::string>& labels) {
  if (labels.empty()) {
    return "";
  }
  std::ostringstream oss;
  oss << "{";
  bool first = true;
  for (const auto& [k, v] : labels) {
    if (!first) {
      oss << ",";
    }
    first = false;
    oss << k << "=\"" << v << "\"";
  }
  oss << "}";
  return oss.str();
}

void Metrics::inc_counter(const std::string& name,
                          const std::unordered_map<std::string, std::string>& labels, double value) {
  std::lock_guard lock(mutex_);
  counters_[label_key(name, labels)] += value;
}

void Metrics::set_gauge(const std::string& name, double value,
                        const std::unordered_map<std::string, std::string>& labels) {
  std::lock_guard lock(mutex_);
  gauges_[label_key(name, labels)] = value;
}

void Metrics::observe_histogram(const std::string& name, double value,
                                const std::unordered_map<std::string, std::string>& labels) {
  std::lock_guard lock(mutex_);
  auto key = label_key(name, labels);
  auto& hist = histograms_[key];
  if (hist.counts.empty()) {
    hist.counts.assign(hist.bounds.size(), 0);
  }
  hist.sum += value;
  hist.count++;
  for (std::size_t i = 0; i < hist.bounds.size(); ++i) {
    if (value <= hist.bounds[i]) {
      hist.counts[i]++;
    }
  }
}

std::string Metrics::render_prometheus() const {
  std::lock_guard lock(mutex_);
  std::ostringstream out;

  auto emit = [&](const std::string& type, const std::string& name) {
    out << "# TYPE " << name << " " << type << "\n";
  };

  if (!counters_.empty()) {
    emit("counter", "serverless_counter");
    for (const auto& [key, val] : counters_) {
      out << key << " " << val << "\n";
    }
  }
  if (!gauges_.empty()) {
    emit("gauge", "serverless_gauge");
    for (const auto& [key, val] : gauges_) {
      out << key << " " << val << "\n";
    }
  }
  for (const auto& [key, hist] : histograms_) {
    emit("histogram", key);
    for (std::size_t i = 0; i < hist.bounds.size(); ++i) {
      out << key << "_bucket{le=\"" << hist.bounds[i] << "\"} " << hist.counts[i] << "\n";
    }
    out << key << "_sum " << hist.sum << "\n";
    out << key << "_count " << hist.count << "\n";
  }

  return out.str();
}

void Metrics::parse_metric_key(const std::string& key, std::string& name,
                               std::unordered_map<std::string, std::string>& labels) {
  labels.clear();
  const auto pos = key.find('{');
  name = pos == std::string::npos ? key : key.substr(0, pos);
  std::size_t i = pos;
  while (i != std::string::npos && i < key.size()) {
    if (key[i] != '{') {
      break;
    }
    const auto end = key.find('}', i);
    if (end == std::string::npos) {
      break;
    }
    const auto part = key.substr(i + 1, end - i - 1);
    const auto eq = part.find('=');
    if (eq != std::string::npos) {
      labels[part.substr(0, eq)] = part.substr(eq + 1);
    }
    i = end + 1;
  }
}

nlohmann::json Metrics::render_json() const {
  std::lock_guard lock(mutex_);
  nlohmann::json counters = nlohmann::json::array();
  nlohmann::json gauges = nlohmann::json::array();
  nlohmann::json histograms = nlohmann::json::array();

  for (const auto& [key, val] : counters_) {
    std::string name;
    std::unordered_map<std::string, std::string> labels;
    parse_metric_key(key, name, labels);
    counters.push_back({{"name", name}, {"labels", labels}, {"value", val}});
  }
  for (const auto& [key, val] : gauges_) {
    std::string name;
    std::unordered_map<std::string, std::string> labels;
    parse_metric_key(key, name, labels);
    gauges.push_back({{"name", name}, {"labels", labels}, {"value", val}});
  }
  for (const auto& [key, hist] : histograms_) {
    std::string name;
    std::unordered_map<std::string, std::string> labels;
    parse_metric_key(key, name, labels);
    nlohmann::json buckets = nlohmann::json::array();
    for (std::size_t i = 0; i < hist.bounds.size(); ++i) {
      buckets.push_back({{"le", hist.bounds[i]}, {"count", hist.counts[i]}});
    }
    histograms.push_back({{"name", name},
                          {"labels", labels},
                          {"count", hist.count},
                          {"sum", hist.sum},
                          {"buckets", buckets}});
  }

  return {{"counters", counters}, {"gauges", gauges}, {"histograms", histograms}};
}

}  // namespace serverless
