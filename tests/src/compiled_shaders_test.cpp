#ifdef KUKI_HAS_DIRECTX
#include <algorithm>
#include <compiled_shaders.hpp>
#include <cstring>
#include <gtest/gtest.h>
#include <span>
#include <utility>
using kuki::FindCompiledShader;
namespace {
// A DXIL container opens with `DXBC` and then the digest `dxil.dll` writes when it signs, which is
// all zeroes in unsigned bytecode -- what the runtime refuses outside developer mode.
auto IsSignedContainer(const std::span<const unsigned char> bytecode) -> bool {
  if (bytecode.size() < 20 || std::memcmp(bytecode.data(), "DXBC", 4) != 0)
    return false;
  return std::any_of(bytecode.begin() + 4, bytecode.begin() + 20, [](const unsigned char byte) { return byte != 0; });
}
} // namespace
TEST(CompiledShaders, HoldsSignedDXILForEveryStage) {
  for (const auto &[file, entryPoint] : {std::pair{"scene", "VSMain"}, {"scene", "VSSkinned"}, {"scene", "PSMain"}, {"post", "PSToneMapping"}, {"ibl", "CSBRDF"}}) {
    const auto bytecode = FindCompiledShader(file, entryPoint);
    EXPECT_TRUE(IsSignedContainer(bytecode)) << file << ".hlsl " << entryPoint;
  }
}
// Shader Model 6.5, which a file is compiled for because it traces rays inline.
TEST(CompiledShaders, HoldsTheRayTracingEntryPoints) {
  for (const auto &[file, entryPoint] : {std::pair{"probe_trace", "CSTrace"}, {"probe_update", "CSMeasure"}, {"raytrace", "CSProbe"}})
    EXPECT_TRUE(IsSignedContainer(FindCompiledShader(file, entryPoint))) << file << ".hlsl " << entryPoint;
}
// An entry point is a function named for its stage, so a helper beside them is not compiled as one.
TEST(CompiledShaders, FindsNothingThatIsNotAnEntryPoint) {
  EXPECT_TRUE(FindCompiledShader("scene", "Displace").empty());
  EXPECT_TRUE(FindCompiledShader("scene", "PSMissing").empty());
  EXPECT_TRUE(FindCompiledShader("missing", "VSMain").empty());
}
#endif
