#pragma once
#include <concepts.hpp>
#include <id.hpp>
#include <kuki_engine_export.h>
#include <render_pass.hpp>
#include <span>
#include <string>
#include <target_description.hpp>
#include <unordered_map>
namespace kuki {
class Renderer;
class KUKI_ENGINE_API RenderGraph {
public:
  auto AddInput(std::string) -> RenderGraph &;
  auto AddOutput(std::string, TargetDescription, const TargetSizing = TargetSizing::Viewport) -> RenderGraph &;
  /// @brief Declares something a pass produces that the graph orders on but does not allocate.
  ///
  /// Every other output here is a render target: the graph creates it, resizes it with the viewport,
  /// and hands its name to whoever reads it. Not everything a pass produces is one. An irradiance
  /// probe field is a structured buffer, sized by the geometry rather than the viewport, holding an
  /// average taken across frames that a resize would throw away -- so the backend owns it and keeps
  /// it, and the graph is told only that it exists and which pass fills it.
  ///
  /// The edge is what this buys. A consumer names it with `AddInput` like any other resource, and
  /// the pass that fills it is thereby ordered ahead of every pass that reads it, rather than the
  /// two being held in step by the order somebody wrote the statements in.
  auto AddResource(std::string) -> RenderGraph &;
  auto BeginPass(const RenderPass) -> void;
  /// @brief Resolves pass inputs against pass outputs, builds the dependency edges, and flattens the graph into execution order.
  ///
  /// TODO: check if a pass has an input and an output with the same name
  auto Compile() -> void;
  auto EndPass() -> RenderGraph &;
  auto Execute(Renderer &) -> void;
  auto GetFinalOutputName() -> std::string;
  auto GetInputs(const PassID) -> std::span<std::string>;
  auto GetOutputs(const PassID) -> std::span<std::string>;
  auto ResizeTargets(Renderer &, const int, const int) -> void;
  auto ForEachPass(auto &&) const -> void;
  auto ForEachTarget(auto &&) const -> void;
private:
  PassID nextId{PassID::First};
  PassID passId{PassID::Invalid};
  bool dirty{false};
  std::unordered_map<PassID, RenderPass> idToFunc;
  std::unordered_map<PassID, std::vector<PassID>> idToPredecessors;
  std::unordered_map<PassID, std::vector<PassID>> idToSuccessors;
  std::unordered_map<PassID, std::vector<std::string>> idToInputs;
  std::unordered_map<PassID, std::vector<std::string>> idToOutputs;
  std::unordered_map<std::string, TargetDescription> nameToDesc; // NOTE: assumes that each name maps to only one description
  std::unordered_map<std::string, TargetSizing> nameToSizing;
  std::vector<PassID> graphFlat;
  auto AreConnected(const PassID, const PassID) -> bool;
  auto CreateEdge(const PassID, const PassID) -> bool;
  auto Flatten() -> void;
};
auto RenderGraph::ForEachPass(auto &&func) const -> void {
  for (const auto &id : graphFlat)
    if (auto it = idToFunc.find(id); it != idToFunc.end())
      func(id, it->second);
}
auto RenderGraph::ForEachTarget(auto &&func) const -> void {
  for (const auto &[name, desc] : nameToDesc)
    func(name, desc);
}
} // namespace kuki
