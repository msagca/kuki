#include <id.hpp>
#include <queue>
#include <render_graph.hpp>
#include <renderer.hpp>
#include <stack>
#include <target_description.hpp>
#include <unordered_map>
#include <unordered_set>
namespace kuki {
auto RenderGraph::AddInput(std::string name) -> RenderGraph & {
  if (passId) {
    auto &inputs = idToInputs[passId];
    if (auto it = std::find(inputs.begin(), inputs.end(), name); it == inputs.end())
      inputs.emplace_back(std::move(name));
  }
  return *this;
}
auto RenderGraph::AddOutput(std::string name, TargetDescription desc) -> RenderGraph & {
  if (passId) {
    if (auto it = nameToDesc.find(name); it != nameToDesc.end()) {
      if (desc != it->second)
        return *this;
    } else
      nameToDesc.insert_or_assign(name, desc);
    auto &outputs = idToOutputs[passId];
    if (auto it = std::find(outputs.begin(), outputs.end(), name); it == outputs.end())
      outputs.emplace_back(std::move(name));
  }
  return *this;
}
auto RenderGraph::Compile() -> void {
  idToPredecessors.clear();
  idToSuccessors.clear();
  std::unordered_map<std::string, std::vector<PassID>> inputToIDs;
  std::unordered_map<std::string, std::vector<PassID>> outputsToIDs;
  for (const auto &[id, inputs] : idToInputs)
    for (const auto &in : inputs)
      if (auto it = inputToIDs.find(in); it != inputToIDs.end()) {
        auto &ids = it->second;
        if (auto it2 = std::find(ids.begin(), ids.end(), id); it2 == ids.end())
          ids.push_back(id);
      } else
        inputToIDs[in].push_back(id);
  for (const auto &[id, outputs] : idToOutputs)
    for (const auto &out : outputs)
      if (auto it = outputsToIDs.find(out); it != outputsToIDs.end()) {
        auto &ids = it->second;
        if (auto it2 = std::find(ids.begin(), ids.end(), id); it2 == ids.end())
          ids.push_back(id);
      } else
        outputsToIDs[out].push_back(id);
  for (const auto &[in, dstIDs] : inputToIDs)
    if (auto it = outputsToIDs.find(in); it != outputsToIDs.end()) {
      const auto &srcIDs = it->second;
      for (const auto &src : srcIDs)
        for (const auto &dst : dstIDs)
          // TODO: check if a pass has an input and an output with the same name
          CreateEdge(src, dst);
    }
  Flatten();
  dirty = false;
}
auto RenderGraph::EndPass() -> RenderGraph & {
  passId = PassID::Invalid;
  return *this;
}
auto RenderGraph::Execute(Renderer &renderer) -> void {
  if (dirty)
    Compile();
  renderer.Reset();
  ForEachTarget([&](const std::string &name, const TargetDescription &desc) {
    renderer.CreateTarget(name, desc);
  });
  ForEachPass([&](const PassID id, const PassFunc &func) {
    auto inputs = GetInputs(id);
    auto outputs = GetOutputs(id);
    func(renderer, inputs, outputs);
  });
}
auto RenderGraph::GetFinalOutputName() -> std::string {
  if (graphFlat.empty())
    return "";
  const auto index = graphFlat.size() - 1;
  const auto id = graphFlat[index];
  if (auto it = idToOutputs.find(id); it != idToOutputs.end()) {
    const auto &outputs = it->second;
    if (!outputs.empty())
      return outputs[0];
  }
  return "";
}
auto RenderGraph::GetInputs(const PassID id) -> std::span<std::string> {
  if (auto it = idToInputs.find(id); it != idToInputs.end())
    return it->second;
  return {};
}
auto RenderGraph::GetOutputs(const PassID id) -> std::span<std::string> {
  if (auto it = idToOutputs.find(id); it != idToOutputs.end())
    return it->second;
  return {};
}
auto RenderGraph::ResizeTargets(Renderer &renderer, const int width, const int height) -> void {
  if (width < 0 || height < 0)
    return;
  for (auto &[name, desc] : nameToDesc)
    if (desc.width != width || desc.height != height) {
      desc.width = width;
      desc.height = height;
      renderer.UpdateTarget(name, desc);
    }
}
auto RenderGraph::AreConnected(const PassID src, const PassID dst) -> bool {
  if (src == dst)
    return true;
  std::stack<PassID> stack;
  stack.push(src);
  std::unordered_set<PassID> visited;
  while (!stack.empty()) {
    const auto node = stack.top();
    stack.pop();
    if (!visited.insert(node).second)
      continue;
    auto it = idToSuccessors.find(node);
    if (it == idToSuccessors.end())
      continue;
    for (const auto &nxt : it->second) {
      if (nxt == dst)
        return true;
      if (!visited.contains(nxt))
        stack.push(nxt);
    }
  }
  return false;
}
auto RenderGraph::CreateEdge(const PassID src, const PassID dst) -> bool {
  if (auto it = idToSuccessors.find(src); it != idToSuccessors.end()) {
    const auto &successors = it->second;
    if (auto it2 = std::find(successors.begin(), successors.end(), dst); it2 != successors.end())
      return false;
  }
  if (AreConnected(dst, src))
    return false; // prevent cycles
  idToSuccessors[src].push_back(dst);
  idToPredecessors[dst].push_back(src);
  dirty = true;
  return true;
}
auto RenderGraph::Flatten() -> void {
  std::unordered_map<PassID, size_t> inDegrees;
  for (const auto &[node, predecessors] : idToPredecessors)
    inDegrees[node] = predecessors.size();
  for (const auto &[node, _] : idToFunc)
    if (!inDegrees.contains(node))
      inDegrees[node] = 0;
  std::queue<PassID> roots;
  for (const auto &[node, degree] : inDegrees)
    if (degree == 0)
      roots.push(node);
  graphFlat.clear();
  while (!roots.empty()) {
    const auto node = roots.front();
    roots.pop();
    graphFlat.push_back(node);
    if (auto it = idToSuccessors.find(node); it != idToSuccessors.end())
      for (const auto &successor : it->second)
        if (--inDegrees[successor] == 0)
          roots.push(successor);
  }
}
} // namespace kuki
