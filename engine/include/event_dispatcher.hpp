#pragma once
#include <functional>
namespace kuki {
class IEventDispatcher {
public:
  virtual ~IEventDispatcher() = default;
  virtual void Unsubscribe(unsigned int) = 0;
};
template <typename T>
class EventDispatcher : public IEventDispatcher {
public:
  ~EventDispatcher() override = default;
  using CallbackSignature = std::function<void(const T&)>;
  unsigned int Subscribe(CallbackSignature);
  void Unsubscribe(unsigned int) override;
  void Emit(const T&);
private:
  std::unordered_map<unsigned int, CallbackSignature> idToCallback;
  /// @brief Id that will be assigned to the next subscriber, it shall be used for unsubscribing later
  unsigned int idNext = 0;
};
template <typename T>
unsigned int EventDispatcher<T>::Subscribe(CallbackSignature callback) {
  idToCallback[idNext] = callback;
  return idNext++;
}
template <typename T>
void EventDispatcher<T>::Unsubscribe(unsigned int id) {
  idToCallback.erase(id);
}
template <typename T>
void EventDispatcher<T>::Emit(const T& event) {
  for (const auto& [id, callback] : idToCallback)
    callback(event);
}
struct EntityCreatedEvent {
  unsigned int id;
};
struct EntityDeletedEvent {
  unsigned int id;
};
} // namespace kuki
