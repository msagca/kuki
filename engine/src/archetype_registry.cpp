#include <archetype_registry.hpp>
namespace kuki {
auto ArchetypeRegistry::Declare(const ArchetypeSpec &spec) -> void {
  if (tableByMask.contains(spec.mask))
    return;
  auto table = std::make_unique<ArchetypeTable>();
  table->signature = spec.mask;
  for (size_t index = 0; index < spec.columns.size(); ++index) {
    table->columnIndexById[spec.ids[index]] = static_cast<int>(table->columns.size());
    table->columns.push_back(spec.columns[index]->CloneEmpty());
  }
  tableByMask.emplace(spec.mask, static_cast<uint32_t>(tables.size()));
  tables.push_back(std::move(table));
}
auto ArchetypeRegistry::Find(const ComponentMask &mask) const -> std::optional<uint32_t> {
  if (auto it = tableByMask.find(mask); it != tableByMask.end())
    return it->second;
  return std::nullopt;
}
auto ArchetypeRegistry::GetTableCount() const -> size_t {
  return tables.size();
}
auto ArchetypeRegistry::Append(const uint32_t index, const EntityID id) -> size_t {
  auto &table = *tables[index];
  for (auto &column : table.columns)
    column->EmplaceDefault();
  table.entities.push_back(id);
  return table.entities.size() - 1;
}
auto ArchetypeRegistry::Remove(const EntityLocation &location) -> EntityID {
  auto &table = *tables[location.table];
  const auto row = location.row;
  const auto last = table.entities.size() - 1;
  EntityID displaced{};
  if (row != last)
    displaced = table.entities[last];
  for (auto &column : table.columns)
    column->SwapPop(row);
  if (row != last)
    table.entities[row] = displaced;
  table.entities.pop_back();
  return displaced;
}
auto ArchetypeRegistry::ClearRows() -> void {
  for (auto &table : tables) {
    for (auto &column : table->columns)
      column->Clear();
    table->entities.clear();
  }
}
} // namespace kuki
