#pragma once
#include <array>
#include <component.hpp>
#include <component_type.hpp>
#include <concepts.hpp>
#include <id.hpp>
#include <memory>
#include <vector>
namespace kuki {
inline constexpr size_t ArchetypeColumnCount = ComponentMask().size();
struct IArchetypeColumn {
  virtual ~IArchetypeColumn() = default;
  virtual auto Size() const -> size_t = 0;
  virtual auto EmplaceDefault() -> void = 0;
  virtual auto MoveAppend(IArchetypeColumn &src, size_t row) -> void = 0;
  virtual auto SwapPop(size_t row) -> void = 0;
};
template <typename T>
struct ArchetypeColumn final : public IArchetypeColumn {
  std::vector<T> components;
  auto Size() const -> size_t override {
    return components.size();
  }
  auto EmplaceDefault() -> void override {
    components.emplace_back();
  }
  auto MoveAppend(IArchetypeColumn &src, size_t row) -> void override {
    components.push_back(std::move(static_cast<ArchetypeColumn<T> &>(src).components[row]));
  }
  auto SwapPop(size_t row) -> void override {
    const auto last = components.size() - 1;
    if (row != last)
      components[row] = std::move(components[last]);
    components.pop_back();
  }
};
struct Archetype {
  Archetype() {
    columnIndexByType.fill(-1);
  }
  ComponentMask signature;
  std::vector<EntityID> entities;
  std::array<int, ArchetypeColumnCount> columnIndexByType;
  std::vector<std::unique_ptr<IArchetypeColumn>> columns;
  template <typename T>
  auto GetColumn(this auto &self) -> ConstCorrectPointer<decltype(self), ArchetypeColumn<T>> {
    const auto index = self.columnIndexByType[Component::GetBit(typeid(T))];
    if (index < 0)
      return nullptr;
    return static_cast<ConstCorrectPointer<decltype(self), ArchetypeColumn<T>>>(self.columns[static_cast<size_t>(index)].get());
  }
};
} // namespace kuki
