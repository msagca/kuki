#include <mutex>
#include <optional>
#include <queue>
namespace kuki {
template <typename T>
class EventQueue {
private:
  std::queue<T> queue;
  std::mutex mutex;
public:
  void Push(T);
  std::optional<T> Pop();
};
template <typename T>
void EventQueue<T>::Push(T event) {
  std::lock_guard<std::mutex> lock(mutex);
  queue.push(std::move(event));
}
template <typename T>
std::optional<T> EventQueue<T>::Pop() {
  std::lock_guard<std::mutex> lock(mutex);
  if (queue.empty())
    return std::nullopt;
  auto event = std::move(queue.front());
  queue.pop();
  return event;
}
} // namespace kuki
