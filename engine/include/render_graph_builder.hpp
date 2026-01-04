#pragma once
#include <kuki_engine_export.h>
#include <memory>
#include <render_graph.hpp>
#include <string>
namespace kuki {
struct TargetDescription;
class KUKI_ENGINE_API RenderGraphBuilder {
public:
  auto AddInput(std::string) -> RenderGraphBuilder &;
  auto AddOutput(std::string, TargetDescription) -> RenderGraphBuilder &;
  auto BeginGraph() -> RenderGraphBuilder &;
  auto EndGraph() -> std::unique_ptr<RenderGraph>;
  auto EndPass() -> RenderGraphBuilder &;
  auto BeginPass(auto &&) -> RenderGraphBuilder &;
  template <typename F>
  auto SetFunc(F &&) -> RenderGraphBuilder &;
private:
  std::unique_ptr<RenderGraph> renderGraph;
};
auto RenderGraphBuilder::BeginPass(auto &&func) -> RenderGraphBuilder & {
  renderGraph->BeginPass(std::forward<decltype(func)>(func));
  return *this;
}
template <typename F>
auto RenderGraphBuilder::SetFunc(F &&func) -> RenderGraphBuilder & {
  return *this;
}
} // namespace kuki
