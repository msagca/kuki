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
  auto Subscribe(auto &&) -> size_t;
  auto Unsubscribe(const size_t) -> void;
  auto operator+=(auto &&) -> size_t;
  auto operator-=(const size_t) -> Event &;
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
auto Event<T...>::Subscribe(auto &&handler) -> size_t {
  // TODO: return an object that unsubscribes automatically when destroyed
  std::lock_guard lock(mutex);
  const auto id = nextId++;
  handlers.emplace(id, std::forward<decltype(handler)>(handler));
  return id;
}
template <typename... T>
auto Event<T...>::Unsubscribe(const size_t id) -> void {
  std::lock_guard lock(mutex);
  handlers.erase(id);
}
template <typename... T>
auto Event<T...>::operator+=(auto &&handler) -> size_t {
  return Subscribe(std::forward<decltype(handler)>(handler));
}
template <typename... T>
auto Event<T...>::operator-=(const size_t id) -> Event & {
  Unsubscribe(id);
  return *this;
}
} // namespace kuki
