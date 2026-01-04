#pragma once
#include <mutex>
#include <queue>
namespace kuki {
template <typename T>
class EventQueue {
private:
  std::queue<std::unique_ptr<T>> queue;
  std::mutex mutex;
public:
  void Push(std::unique_ptr<T>);
  std::unique_ptr<T> Pop();
  /// @brief Process all events in the queue by applying the given function to each event
  /// @tparam F Function type
  /// @param F&& Function to execute on events
  template <typename F>
  void Drain(F &&);
};
template <typename T>
void EventQueue<T>::Push(std::unique_ptr<T> event) {
  std::lock_guard<std::mutex> lock(mutex);
  queue.push(std::move(event));
}
template <typename T>
std::unique_ptr<T> EventQueue<T>::Pop() {
  std::lock_guard<std::mutex> lock(mutex);
  if (queue.empty())
    return nullptr;
  auto event = std::move(queue.front());
  queue.pop();
  return event;
}
template <typename T>
template <typename F>
void EventQueue<T>::Drain(F &&func) {
  std::queue<std::unique_ptr<T>> queue_;
  { // NOTE: create new scope so that the mutex is released before the func calls, avoiding unnecessary blocking or potential deadlocks (if func enqueues new items)
    std::lock_guard<std::mutex> lock(mutex);
    if (queue.empty())
      return;
    queue_.swap(queue);
  }
  while (!queue_.empty()) {
    func(std::move(queue_.front()));
    queue_.pop();
  }
}
} // namespace kuki
