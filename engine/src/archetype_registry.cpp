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
  tablesOfMask[spec.mask].push_back(static_cast<uint32_t>(tables.size()));
  tables.push_back(std::move(table));
}
auto ArchetypeRegistry::Find(const ComponentMask &mask) const -> std::optional<uint32_t> {
  if (auto it = tableByMask.find(mask); it != tableByMask.end())
    return it->second;
  return std::nullopt;
}
auto ArchetypeRegistry::Find(const ComponentMask &mask, const PoolKey &pool) const -> std::optional<uint32_t> {
  if (!pool)
    return Find(mask);
  if (auto it = tableByPool.find({mask, pool}); it != tableByPool.end())
    return it->second;
  return std::nullopt;
}
auto ArchetypeRegistry::FindOrCreate(const ComponentMask &mask, const PoolKey &pool, const PoolSettings &settings) -> std::optional<uint32_t> {
  if (!pool)
    return Find(mask);
  if (auto found = Find(mask, pool))
    return found;
  const auto own = Find(mask);
  if (!own)
    return std::nullopt;
  const auto &model = *tables[*own];
  auto table = std::make_unique<ArchetypeTable>();
  table->signature = mask;
  table->pool = pool;
  table->bounds = settings.bounds.value_or(BoundingBox{});
  table->columnIndexById = model.columnIndexById;
  for (const auto &column : model.columns) {
    table->columns.push_back(column->CloneEmpty());
    table->columns.back()->Reserve(settings.capacity);
  }
  table->entities.reserve(settings.capacity);
  const auto index = static_cast<uint32_t>(tables.size());
  tableByPool.emplace(PoolOf{mask, pool}, index);
  tablesOfMask[mask].push_back(index);
  tables.push_back(std::move(table));
  return index;
}
auto ArchetypeRegistry::GetTableCount() const -> size_t {
  return tables.size();
}
auto ArchetypeRegistry::Append(const uint32_t index, const EntityID id) -> size_t {
  auto &table = *tables[index];
  for (auto &column : table.columns)
    column->EmplaceDefault();
  table.entities.push_back(id);
  ++table.version;
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
  ++table.version;
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
    ++table->version;
  }
}
} // namespace kuki
