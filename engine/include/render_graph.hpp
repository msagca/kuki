#pragma once
#include <concepts.hpp>
#include <id.hpp>
#include <kuki_engine_export.h>
#include <span>
#include <string>
#include <target_description.hpp>
#include <unordered_map>
namespace kuki {
struct TargetBinding {
  std::string name;
  TargetDescription desc;
};
class Renderer;
using PassFunc = std::function<void(Renderer &, std::span<std::string>, std::span<TargetBinding>)>;
class KUKI_ENGINE_API RenderGraph {
public:
  auto AddInput(std::string) -> RenderGraph &;
  auto AddOutput(std::string, TargetDescription) -> RenderGraph &;
  auto Compile() -> void;
  auto EndPass() -> RenderGraph &;
  auto Execute(Renderer &) -> void;
  auto GetInputs(const PassID) -> std::span<std::string>;
  auto GetOutputs(const PassID) -> std::span<TargetBinding>;
  auto BeginPass(auto &&) -> RenderGraph &;
  auto ForEachPass(auto &&) const -> void;
private:
  bool dirty{false};
  PassID nextId{PassID::First};
  PassID passId{PassID::Invalid};
  // passes
  std::unordered_map<PassID, PassFunc> idToFunc;
  std::unordered_map<PassID, std::vector<std::string>> idToInputs;
  std::unordered_map<PassID, std::vector<TargetBinding>> idToOutputs;
  std::unordered_map<std::string, TargetDescription> ioToDesc; // NOTE: assumes that each name maps to only one description
  // graph
  std::unordered_map<PassID, std::vector<PassID>> idToPredecessors;
  std::unordered_map<PassID, std::vector<PassID>> idToSuccessors;
  std::vector<PassID> graphFlat;
  /// @return `true` if there is a path between `src` and `dst`
  auto AreConnected(const PassID, const PassID) -> bool;
  auto CreateEdge(const PassID, const PassID) -> bool;
  /// @brief Turn the adjacency list into an array (using Kahn's algorithm) to optimize graph traversal
  auto Flatten() -> void;
};
auto RenderGraph::BeginPass(auto &&func) -> RenderGraph & {
  passId = nextId++;
  idToFunc[passId] = std::forward<decltype(func)>(func);
  return *this;
}
auto RenderGraph::ForEachPass(auto &&func) const -> void {
  for (const auto &id : graphFlat) {
    if (auto it = idToFunc.find(id); it != idToFunc.end())
      func(id, it->second);
  }
}
} // namespace kuki
