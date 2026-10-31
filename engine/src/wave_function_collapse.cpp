#include <algorithm>
#include <cmath>
#include <wave_function_collapse.hpp>
namespace kuki {
WaveRules::WaveRules(const size_t tiles, const uint32_t kinds)
  : tiles(std::min(tiles, MAX_WAVE_TILES)), kinds(std::max(kinds, 1u)), weights(this->tiles, 1.f), allowed(this->tiles * this->kinds) {}
auto WaveRules::Allow(const TileIndex a, const TileIndex b, const uint32_t kind, const uint32_t reverse) -> WaveRules & {
  if (a < tiles && b < tiles && kind < kinds && reverse < kinds) {
    allowed[kind * tiles + a].set(b);
    allowed[reverse * tiles + b].set(a);
  }
  return *this;
}
auto WaveRules::AllowSelf(const TileIndex tile) -> WaveRules & {
  for (uint32_t kind = 0; kind < kinds; ++kind)
    if (tile < tiles)
      allowed[kind * tiles + tile].set(tile);
  return *this;
}
auto WaveRules::Weight(const TileIndex tile, const float weight) -> WaveRules & {
  if (tile < tiles)
    weights[tile] = std::max(weight, .0f);
  return *this;
}
auto WaveRules::TileCount() const -> size_t {
  return tiles;
}
auto WaveRules::KindCount() const -> uint32_t {
  return kinds;
}
auto WaveRules::GetWeight(const TileIndex tile) const -> float {
  return weights[tile];
}
auto WaveRules::Allowed(const TileIndex tile, const uint32_t kind) const -> const TileSet & {
  return allowed[std::min(kind, kinds - 1) * tiles + tile];
}
auto WaveRules::All() const -> TileSet {
  TileSet all;
  for (size_t i = 0; i < tiles; ++i)
    all.set(i);
  return all;
}
auto WaveGraph::AddNode(const std::span<const Side> list) -> uint32_t {
  sides.insert(sides.end(), list.begin(), list.end());
  offsets.push_back(static_cast<uint32_t>(sides.size()));
  return static_cast<uint32_t>(offsets.size() - 2);
}
auto WaveGraph::NodeCount() const -> size_t {
  return offsets.size() - 1;
}
auto WaveGraph::GetSides(const uint32_t node) const -> std::span<const Side> {
  return std::span(sides).subspan(offsets[node], offsets[node + 1] - offsets[node]);
}
auto WaveGraph::FromGrid(const IrregularGrid &grid) -> WaveGraph {
  return FromGrid(grid, [](CellIndex, CellIndex) { return 0u; });
}
auto WaveGraph::FromGrid(const IrregularGrid &grid, const std::function<uint32_t(CellIndex, CellIndex)> &kind) -> WaveGraph {
  WaveGraph graph;
  std::vector<Side> list;
  for (CellIndex c = 0; c < grid.CellCount(); ++c) {
    list.clear();
    for (const auto next : grid.GetCell(c).neighbours)
      list.push_back({next, kind(c, next)});
    graph.AddNode(list);
  }
  return graph;
}
WaveFunctionCollapse::WaveFunctionCollapse(WaveGraph graph, WaveRules rules)
  : graph(std::move(graph)), rules(std::move(rules)) {
  const auto nodes = this->graph.NodeCount();
  restriction.assign(nodes, this->rules.All());
  bias.assign(nodes * this->rules.TileCount(), 1.f);
  domain.assign(nodes, this->rules.All());
  entropy.assign(nodes, .0f);
  stale.assign(nodes, true);
}
auto WaveFunctionCollapse::Restrict(const uint32_t node, const TileSet &tiles) -> void {
  if (node < restriction.size())
    restriction[node] &= tiles;
}
auto WaveFunctionCollapse::Bias(const uint32_t node, const TileIndex tile, const float factor) -> void {
  if (node < restriction.size() && tile < rules.TileCount())
    bias[node * rules.TileCount() + tile] *= std::max(factor, .0f);
}
auto WaveFunctionCollapse::WeightAt(const uint32_t node, const TileIndex tile) const -> float {
  return rules.GetWeight(tile) * bias[node * rules.TileCount() + tile];
}
auto WaveFunctionCollapse::Set(const uint32_t node, const TileSet &tiles) -> void {
  domain[node] = tiles;
  stale[node] = true;
  queue.push_back(node);
}
auto WaveFunctionCollapse::Propagate() -> bool {
  const auto tiles = rules.TileCount();
  while (!queue.empty()) {
    const auto node = queue.back();
    queue.pop_back();
    const auto &own = domain[node];
    // What the node's tiles let be across each kind of side, worked out once per kind rather than
    // once per side: a node has up to eight sides and usually one kind.
    TileSet support[4];
    bool supported[4]{};
    for (const auto &side : graph.GetSides(node)) {
      TileSet scratch;
      TileSet *allowed = &scratch;
      if (side.kind < 4) {
        allowed = &support[side.kind];
        if (!supported[side.kind]) {
          for (size_t t = 0; t < tiles; ++t)
            if (own.test(t))
              *allowed |= rules.Allowed(static_cast<TileIndex>(t), side.kind);
          supported[side.kind] = true;
        }
      } else
        for (size_t t = 0; t < tiles; ++t)
          if (own.test(t))
            scratch |= rules.Allowed(static_cast<TileIndex>(t), side.kind);
      const auto narrowed = domain[side.to] & *allowed;
      if (narrowed == domain[side.to])
        continue;
      if (narrowed.none()) {
        failed = side.to;
        queue.clear();
        return false;
      }
      Set(side.to, narrowed);
    }
  }
  return true;
}
auto WaveFunctionCollapse::Lowest() -> uint32_t {
  const auto tiles = rules.TileCount();
  auto best = INVALID_TILE;
  auto bestEntropy = std::numeric_limits<float>::max();
  for (uint32_t node = 0; node < domain.size(); ++node) {
    if (domain[node].count() <= 1)
      continue;
    if (stale[node]) {
      auto sum = .0f;
      auto sumLog = .0f;
      for (size_t t = 0; t < tiles; ++t)
        if (domain[node].test(t))
          if (const auto w = WeightAt(node, static_cast<TileIndex>(t)); w > .0f) {
            sum += w;
            sumLog += w * std::log(w);
          }
      // A node whose every remaining tile has no weight is as good as settled on whichever is first,
      // and is settled before anything with a real choice to make.
      entropy[node] = sum > .0f ? std::log(sum) - sumLog / sum : -1.f;
      stale[node] = false;
    }
    // A little noise, so that nodes of equal entropy -- every node, at the start -- are not settled
    // in the order of their indices, which would grow the result as a sweep from one corner.
    const auto noisy = entropy[node] + random.Float() * 1e-3f;
    if (noisy < bestEntropy) {
      bestEntropy = noisy;
      best = node;
    }
  }
  return best;
}
auto WaveFunctionCollapse::Choose(const uint32_t node) -> TileIndex {
  const auto tiles = rules.TileCount();
  auto sum = .0f;
  auto first = INVALID_TILE;
  for (size_t t = 0; t < tiles; ++t)
    if (domain[node].test(t)) {
      sum += WeightAt(node, static_cast<TileIndex>(t));
      if (first == INVALID_TILE)
        first = static_cast<TileIndex>(t);
    }
  if (sum <= .0f)
    return first;
  auto pick = random.Float() * sum;
  for (size_t t = 0; t < tiles; ++t)
    if (domain[node].test(t)) {
      pick -= WeightAt(node, static_cast<TileIndex>(t));
      if (pick < .0f)
        return static_cast<TileIndex>(t);
    }
  // Rounding can leave a sliver past the last tile; it belongs to the last one.
  for (size_t t = tiles; t-- > 0;)
    if (domain[node].test(t))
      return static_cast<TileIndex>(t);
  return first;
}
auto WaveFunctionCollapse::Rebuild() -> bool {
  queue.clear();
  for (uint32_t node = 0; node < domain.size(); ++node) {
    if (chosen[node] != INVALID_TILE) {
      domain[node].reset();
      domain[node].set(chosen[node]);
    } else
      domain[node] = restriction[node];
    stale[node] = true;
    queue.push_back(node);
  }
  return Propagate();
}
auto WaveFunctionCollapse::Repair(const uint32_t node, const int reach) -> bool {
  // Outwards from the node that was boxed in, a step at a time, unsettling every choice met.
  std::vector<int> depth(domain.size(), -1);
  std::vector<uint32_t> frontier{node};
  depth[node] = 0;
  for (size_t i = 0; i < frontier.size(); ++i) {
    const auto current = frontier[i];
    chosen[current] = INVALID_TILE;
    if (depth[current] >= reach)
      continue;
    for (const auto &side : graph.GetSides(current))
      if (depth[side.to] < 0) {
        depth[side.to] = depth[current] + 1;
        frontier.push_back(side.to);
      }
  }
  return Rebuild();
}
auto WaveFunctionCollapse::Attempt(const uint32_t limit) -> bool {
  chosen.assign(domain.size(), INVALID_TILE);
  for (const auto &tiles : restriction)
    if (tiles.none())
      return false;
  // The restrictions, carried outwards by the rules before any choice is made. A contradiction here
  // is in the restrictions themselves, and no choice can mend it.
  if (!Rebuild())
    return false;
  const auto start = repairs;
  auto reach = 1;
  auto quiet = 0;
  for (;;) {
    const auto node = Lowest();
    if (node == INVALID_TILE)
      return true;
    const auto tile = Choose(node);
    chosen[node] = tile;
    TileSet one;
    one.set(tile);
    Set(node, one);
    if (Propagate()) {
      // A run of choices that went through: the trouble was local, and the next repair can be too.
      if (++quiet > 16)
        reach = 1;
      continue;
    }
    quiet = 0;
    // Repaired until a rebuild goes through, each time a step further out from where it failed.
    for (;;) {
      ++repairs;
      if (repairs - start > limit)
        return false;
      if (Repair(failed, reach)) {
        reach = std::min(reach + 1, 12);
        break;
      }
      reach = std::min(reach + 1, 12);
    }
  }
}
auto WaveFunctionCollapse::Run(const WaveSettings &settings) -> bool {
  repairs = 0;
  attempts = 0;
  for (uint32_t attempt = 0; attempt < std::max(settings.attempts, 1u); ++attempt) {
    ++attempts;
    random = Random(settings.seed, 0x3c6ef372fe94f82bull + attempt * 2ull);
    if (Attempt(settings.repairs))
      return true;
  }
  return false;
}
auto WaveFunctionCollapse::Get(const uint32_t node) const -> TileIndex {
  if (node >= domain.size() || domain[node].count() != 1)
    return INVALID_TILE;
  for (size_t t = 0; t < rules.TileCount(); ++t)
    if (domain[node].test(t))
      return static_cast<TileIndex>(t);
  return INVALID_TILE;
}
auto WaveFunctionCollapse::Possible(const uint32_t node) const -> const TileSet & {
  return domain[node];
}
auto WaveFunctionCollapse::NodeCount() const -> size_t {
  return domain.size();
}
auto WaveFunctionCollapse::GetRepairs() const -> uint32_t {
  return repairs;
}
auto WaveFunctionCollapse::GetAttempts() const -> uint32_t {
  return attempts;
}
auto WaveFunctionCollapse::GetGraph() const -> const WaveGraph & {
  return graph;
}
auto WaveFunctionCollapse::GetRules() const -> const WaveRules & {
  return rules;
}
} // namespace kuki
