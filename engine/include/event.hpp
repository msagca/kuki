#pragma once
#include <functional>
#include <mutex>
#include <vector>
namespace kuki {
template <typename... T>
class Event {
public:
  using Handler = std::function<void(T...)>;
  auto Emit(T...) -> void;
  auto Subscribe(Handler) -> size_t;
  auto Unsubscribe(size_t) -> void;
private:
  size_t nextId{0};
  std::mutex mutex;
  std::unordered_map<size_t, Handler> handlers;
};
template <typename... T>
auto Event<T...>::Emit(T... args) -> void {
  std::vector<Handler> handlers_;
  {
    std::lock_guard lock(mutex);
    handlers_.reserve(handlers.size());
    for (const auto &[_, handler] : handlers)
      handlers_.push_back(handler);
  }
  for (const auto &handler : handlers_)
    if (handler)
      handler(args...);
}
template <typename... T>
auto Event<T...>::Subscribe(Handler handler) -> size_t {
  // TODO: return an object that unsubscribes automatically when destroyed
  std::lock_guard lock(mutex);
  const auto id = nextId++;
  handlers.emplace(id, std::move(handler));
  return id;
}
template <typename... T>
auto Event<T...>::Unsubscribe(size_t id) -> void {
  std::lock_guard lock(mutex);
  handlers.erase(id);
}
} // namespace kuki
