#pragma once
#include <bitset>
#include <cstdint>
#include <functional>
#include <irregular_grid.hpp>
#include <kuki_engine_export.h>
#include <limits>
#include <random.hpp>
#include <span>
#include <utility>
#include <vector>
namespace kuki {
/// @brief The most tiles one set of rules can have. A set of tiles is one machine word wide, so a
/// cell's possibilities are a mask and narrowing them is an `and`.
inline constexpr size_t MAX_WAVE_TILES = 64;
using TileSet = std::bitset<MAX_WAVE_TILES>;
using TileIndex = uint32_t;
inline constexpr TileIndex INVALID_TILE = std::numeric_limits<TileIndex>::max();
/// @brief Which tiles may sit next to which, and how common each is.
///
/// On a square grid the rules of a wave function collapse are about directions: what may be north
/// of what. An irregular grid has no north, so the rules here are about sides, and a side has a
/// kind -- a number the caller gives each side of the graph. With one kind, which is the default,
/// the rules are only "may these two be neighbours", and they are symmetric. With more, a side can
/// mean something: uphill and downhill, a river's way in and way out, a side facing the sea wind.
/// A kind seen from the other cell is its reverse, which the caller names when allowing a pair.
class KUKI_ENGINE_API WaveRules {
public:
  explicit WaveRules(const size_t tiles, const uint32_t kinds = 1);
  /// @brief Lets `b` be across a side of `kind` from `a`, and so `a` across that side's reverse
  /// from `b`. A kind that is its own reverse -- the default, and the only one there is with one
  /// kind -- makes the rule symmetric.
  auto Allow(const TileIndex a, const TileIndex b, const uint32_t kind = 0, const uint32_t reverse = 0) -> WaveRules &;
  /// @brief Lets a tile be next to itself across any side of its own reverse kind.
  auto AllowSelf(const TileIndex) -> WaveRules &;
  /// @brief How often a tile is chosen when nothing rules it out, relative to the others.
  auto Weight(const TileIndex, const float) -> WaveRules &;
  auto TileCount() const -> size_t;
  auto KindCount() const -> uint32_t;
  auto GetWeight(const TileIndex) const -> float;
  /// @brief The tiles that may be across a side of `kind` from `tile`.
  auto Allowed(const TileIndex tile, const uint32_t kind) const -> const TileSet &;
  /// @brief Every tile there is.
  auto All() const -> TileSet;
private:
  size_t tiles;
  uint32_t kinds;
  std::vector<float> weights;
  std::vector<TileSet> allowed;
};
/// @brief The cells a wave function collapse is run over and which are neighbours, with the kind of
/// each side.
///
/// Any graph will do: an irregular grid's cells, a square grid's, the rooms of a dungeon. Kept
/// apart from the grid so the solver needs to know nothing of geometry.
struct KUKI_ENGINE_API WaveGraph {
  struct Side {
    uint32_t to{};
    uint32_t kind{};
  };
  /// Node `i`'s sides are `sides[offsets[i]]` up to `sides[offsets[i + 1]]`.
  std::vector<uint32_t> offsets{0u};
  std::vector<Side> sides;
  auto AddNode(const std::span<const Side>) -> uint32_t;
  auto NodeCount() const -> size_t;
  auto GetSides(const uint32_t) const -> std::span<const Side>;
  /// @brief A node for each cell and a side for each pair of neighbours, every side of kind nought.
  static auto FromGrid(const IrregularGrid &) -> WaveGraph;
  /// @brief As above, with the kind of the side from one cell to another given by `kind`.
  static auto FromGrid(const IrregularGrid &, const std::function<uint32_t(CellIndex, CellIndex)> &kind) -> WaveGraph;
};
/// @brief How hard the solver tries before it gives up.
struct WaveSettings {
  uint64_t seed{};
  /// @brief Repairs it may make in one attempt before starting again from nothing. See
  /// `WaveFunctionCollapse`.
  uint32_t repairs{2048};
  /// @brief Attempts, each from a seed of its own, before it reports that it found no answer.
  uint32_t attempts{8};
};
/// @brief A wave function collapse over any graph: fills every node with a tile so that every side
/// joins two tiles the rules allow to meet.
///
/// The model is Gumin's simple tiled one, taken off the grid. Every node begins able to be any tile.
/// The node with the least uncertainty left -- the least Shannon entropy over its tiles' weights --
/// is settled on one tile, chosen at random by weight; the rules then take from each neighbour every
/// tile that could not be next to what is left, and from their neighbours in turn, until nothing
/// changes. Repeat until every node has one tile.
///
/// That alone fails often on a graph whose nodes have many neighbours: a node with seven can be
/// boxed in by three of them that no tile agrees with. What is done then is a repair rather than a
/// backtrack. Every node within a few steps of the one left with nothing is unsettled, every node
/// still unsettled goes back to what its restriction allows, and the rules are carried out again
/// from the nodes still settled. If that boxes a node in too, the next repair reaches further, and
/// the reach shrinks back once choices go through again.
///
/// Backtracking -- taking back the last choice, and the one before, from a trail -- was the first
/// way, and it is the right one for a puzzle. It is the wrong one for a map, where the choice that
/// boxed a node in was usually made long before and far away, by whichever side of a region was
/// settled first: unwinding choices in the order they were made thrashes through thousands of the
/// innocent ones near the end. A seed whose bias asked for a ring of hills round every mountain
/// took over thirty thousand backtracks across eight attempts and settled nothing; repaired, the same
/// map settles in one attempt with under two hundred repairs. Only past `WaveSettings::repairs` of those does it start
/// again, from another seed.
///
/// A caller shapes the result two ways besides the rules. `Restrict` fixes what a node may be before
/// the run -- the map's rim is sea -- and is kept for every attempt. `Bias` makes a tile more or less
/// likely at one node without forbidding anything, which is how a noise field gives the result its
/// large-scale shape while the rules keep it locally consistent: the noise says where the mountains
/// tend to be, the rules say a mountain never touches the beach.
///
/// Deterministic: the same graph, rules, restrictions, biases and seed give the same tiles.
class KUKI_ENGINE_API WaveFunctionCollapse {
public:
  WaveFunctionCollapse(WaveGraph, WaveRules);
  /// @brief Narrows what a node may be, for every attempt. Several calls on one node narrow it to
  /// what they have in common.
  auto Restrict(const uint32_t node, const TileSet &) -> void;
  /// @brief Scales a tile's weight at one node. Several calls multiply. Nought forbids it there,
  /// as surely as `Restrict` would, without making a node that had no other tile unsolvable.
  auto Bias(const uint32_t node, const TileIndex, const float factor) -> void;
  /// @brief Fills every node. False when no attempt found an answer, in which case what is left is
  /// the last attempt's state, nodes it had not settled included.
  auto Run(const WaveSettings & = {}) -> bool;
  /// @brief The tile a node was settled on, or `INVALID_TILE` while it could still be several.
  auto Get(const uint32_t) const -> TileIndex;
  auto Possible(const uint32_t) const -> const TileSet &;
  auto NodeCount() const -> size_t;
  /// @brief Repairs made in the last run, over every attempt, and the attempts it took.
  auto GetRepairs() const -> uint32_t;
  auto GetAttempts() const -> uint32_t;
  auto GetGraph() const -> const WaveGraph &;
  auto GetRules() const -> const WaveRules &;
private:
  WaveGraph graph;
  WaveRules rules;
  std::vector<TileSet> restriction;
  std::vector<float> bias;
  std::vector<TileSet> domain;
  std::vector<float> entropy;
  std::vector<bool> stale;
  /// The tile each node was settled on by a choice, or `INVALID_TILE`. Narrowing settles nodes too,
  /// and those are not listed: a repair keeps the choices and works the rest out again.
  std::vector<TileIndex> chosen;
  std::vector<uint32_t> queue;
  /// The node the last propagation left with no tile.
  uint32_t failed{};
  Random random;
  uint32_t repairs{};
  uint32_t attempts{};
  auto WeightAt(const uint32_t node, const TileIndex) const -> float;
  auto Set(const uint32_t node, const TileSet &) -> void;
  /// @brief Takes from every node what its neighbours rule out, starting from the nodes queued.
  /// False on finding a node with nothing left.
  auto Propagate() -> bool;
  /// @brief The unsettled node with the least entropy, or `INVALID_TILE` if every node is settled.
  auto Lowest() -> uint32_t;
  auto Choose(const uint32_t node) -> TileIndex;
  /// @brief Every node back to its restriction and its choice, if it has one, and the rules carried
  /// out from there. False if that leaves a node with nothing.
  auto Rebuild() -> bool;
  /// @brief Unsettles the choices within `reach` steps of a node and rebuilds. False if it could not
  /// be done without boxing a node in.
  auto Repair(const uint32_t node, const int reach) -> bool;
  auto Attempt(const uint32_t limit) -> bool;
};
} // namespace kuki
