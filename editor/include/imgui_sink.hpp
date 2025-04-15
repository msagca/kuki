#pragma once
#include <spdlog/sinks/base_sink.h>
#include <spdlog/spdlog.h>
#include <string>
#include <vector>
template <typename T>
class ImGuiSink final : public spdlog::sinks::base_sink<T> {
private:
  std::vector<std::string> logs;
protected:
  void sink_it_(const spdlog::details::log_msg&) override;
  void flush_() override;
public:
  template <typename F>
  void ForEachLine(F) const;
  void Clear();
};
template <typename T>
void ImGuiSink<T>::sink_it_(const spdlog::details::log_msg& message) {
  spdlog::memory_buf_t formatted;
  this->formatter_->format(message, formatted);
  std::lock_guard<T> lock(this->mutex_);
  logs.push_back(std::string(formatted.data(), formatted.size()));
}
template <typename T>
void ImGuiSink<T>::flush_() {
}
template <typename T>
template <typename F>
void ImGuiSink<T>::ForEachLine(F func) const {
  for (const auto& line : logs)
    func(line);
}
template <typename T>
void ImGuiSink<T>::Clear() {
  std::lock_guard<T> lock(this->mutex_);
  logs.clear();
}
