#pragma once
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
  size_t GetCount();
  size_t Request();
  void Release(size_t);
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
size_t Pool<T>::GetCount() {
  return last;
}
template <typename T>
void Pool<T>::Resize() {
  capacity *= 2;
  memory.resize(capacity);
}
template <typename T>
size_t Pool<T>::Request() {
  if (last >= memory.size())
    Resize();
  if (last >= count) // NOTE: last can't be greater than count, an assert statement can be added here
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
