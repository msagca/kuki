#pragma once
#include <cassert>
#include <functional>
#include <vector>
template <typename T>
class Pool {
private:
  std::vector<T> memory;
  std::function<T()> generator;
  std::function<void(T)> destroyer;
  size_t capacity;
  size_t count = 0; // number of initialized items
  size_t last = 0; // index of the next available item
  void Resize();
public:
  Pool(std::function<T()>, std::function<void(T)>, size_t = 2);
  size_t GetCapacity();
  /// <returns>The number of items currently in use</returns>
  size_t GetActive();
  /// <returns>The number of items ready for use</returns>
  size_t GetInactive();
  /// <returns>The index of the next available item</returns>
  size_t Request();
  void Release(size_t);
  /// <returns>A pointer to the item at the given index</returns>
  T* Get(size_t);
  template <typename F>
  void ForEach(F);
  void Clear();
};
template <typename T>
Pool<T>::Pool(std::function<T()> generator, std::function<void(T)> destroyer, size_t capacity)
  : generator(generator), destroyer(destroyer), capacity(capacity) {
  memory.resize(this->capacity);
}
template <typename T>
size_t Pool<T>::GetCapacity() {
  return capacity;
}
template <typename T>
size_t Pool<T>::GetActive() {
  return last;
}
template <typename T>
size_t Pool<T>::GetInactive() {
  return count - last;
}
template <typename T>
void Pool<T>::Resize() {
  capacity *= 2;
  memory.resize(capacity);
}
template <typename T>
size_t Pool<T>::Request() {
  assert(last <= count && "Pool: Last index cannot be greater than the item count.");
  if (last >= memory.size())
    Resize();
  if (last >= count)
    memory[count++] = generator();
  return last++;
}
template <typename T>
void Pool<T>::Release(size_t index) {
  if (index >= last)
    return;
  std::swap(memory[index], memory[--last]);
}
template <typename T>
T* Pool<T>::Get(size_t index) {
  if (index >= last)
    return nullptr;
  return &memory[index];
}
template <typename T>
template <typename F>
void Pool<T>::ForEach(F func) {
  for (auto i = 0; i < last; i++)
    func(memory[i]);
}
template <typename T>
void Pool<T>::Clear() {
  for (auto i = 0; i < count; i++)
    destroyer(memory[i]);
}
