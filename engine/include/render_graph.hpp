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
  auto AddOutput(std::string, TargetDescription) -> RenderGraph &;
  auto BeginPass(const RenderPass) -> void;
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
