#pragma once
#ifdef KUKI_HAS_DIRECTX
#include <dx_common.hpp>
#include <dx_shader_compiler.hpp>
#include <kuki_engine_export.h>
#include <light_limits.hpp>
#include <unordered_map>
namespace kuki {
/// @brief Number of texture slots a material's descriptor table holds, one per `TextureContent`.
inline constexpr uint32_t MATERIAL_TEXTURE_SLOTS = 7;
/// @brief One point light as the scene shader's constant buffer sees it.
///
/// Every member is padded to a four-float boundary. HLSL constant buffers never let a vector
/// straddle a 16-byte register, so a tightly packed C++ struct would silently disagree with the
/// shader's own view of the same bytes.
struct DXPointLight {
  float position[4]{};
  float diffuse[4]{};
  float specular[4]{};
  float attenuation[4]{};
};
/// @brief One spot light as the scene shader's constant buffer sees it.
struct DXSpotLight {
  float position[4]{};
  float direction[4]{};
  float diffuse[4]{};
  float specular[4]{};
  float attenuation[4]{};
  float cutoff[4]{};
};
/// @brief Everything the scene pass needs that is the same for every draw in a frame.
///
/// Lives in a constant buffer rather than root constants because the light arrays alone are far
/// larger than the root signature's whole 64-DWORD budget. The per-draw data that does still fit
/// stays in root constants, where it costs no descriptor and no allocation.
///
/// The probe fields describe the volume `DXProbeVolume` last built, so that shading and the trace
/// agree on where it sits without either being told separately: `probeVolume` is its minimum corner
/// with its cubic side in the fourth component, and `probeCounts` the probe count, the node count,
/// the lookup resolution, and whether there is a volume to sample at all.
struct DXFrameConstants {
  float viewProjection[16]{};
  float lightViewProjection[16]{};
  float spotLightViewProjection[MAX_SPOT_LIGHTS][16]{};
  float viewPosition[4]{};
  float directionalDirection[4]{};
  float directionalAmbient[4]{};
  float directionalDiffuse[4]{};
  float directionalSpecular[4]{};
  float directionalIntensity[4]{};
  DXPointLight pointLights[MAX_POINT_LIGHTS]{};
  DXSpotLight spotLights[MAX_SPOT_LIGHTS]{};
  uint32_t counts[4]{};
  uint32_t flags[4]{};
  float probeVolume[4]{};
  uint32_t probeCounts[4]{};
  /// @brief Which shading step the pass writes out instead of the finished pixel, in `x`.
  ///
  /// A `LightingDebugView`. Last in the buffer deliberately: `HashLighting` covers the range from
  /// the directional light through the flags, and that hash is what winds the probe volume's running
  /// mean back when the lighting changes. Selecting a debug view changes nothing about the light, so
  /// it has to sit outside that range or every change of view would throw away a converged field.
  uint32_t debug[4]{};
  /// @brief How much of each indirect term is added: the bounce, the sky, and the flat fallback.
  ///
  /// Past `debug` and so past `HashLighting` as well, which is right for these three: they scale
  /// what the shading pass does with the field rather than changing what went into it, so a scene
  /// lit brighter is the same field read differently and there is nothing to converge again.
  float indirectScale[4]{};
  /// @brief How the field is reconstructed at a point: surface bias, weight floor, and the exponent
  /// the visibility verdict is raised to.
  ///
  /// Outside `HashLighting` too, but not for the reason above -- `HashProbeTuning` covers this
  /// vector separately. Two of the three are read by the trace as well, for the light it gathers
  /// off other probes, so moving one really does change the field and the mean has to be wound
  /// back. Keeping them out of `HashLighting` and hashing them on their own is what lets the trace
  /// react to these while the scale above passes through it without disturbing anything.
  float probeTuning[4]{};
};
/// @brief One drawn instance, read out of a structured buffer by `SV_InstanceID`.
///
/// Everything that varies between instances of the same mesh and material lives here, which is
/// what lets a batch collapse into one draw call. The transform stays a full matrix rather than a
/// compressed form because the shader needs it twice, once for the position and once to build the
/// normal basis.
///
/// Structured buffers pack tightly rather than by 16-byte register, so this layout is byte-for-byte
/// what the shader's own struct declares. The padding is explicit for exactly that reason.
struct DXInstanceData {
  float model[16]{};
  uint32_t entityId{};
  uint32_t padding[3]{};
};
/// @brief Root constants the scene pipeline takes, laid out to match `cbuffer DrawConstants`.
///
/// Only the material's scalar values remain here: the camera is per-frame and the transforms are
/// per-instance, so what is left is the little that changes from batch to batch. Passed as root
/// constants rather than a constant buffer so a batch needs no descriptor or heap allocation.
///
/// Root constants are the scarcest thing in the signature: all of these count against the same
/// 64-DWORD budget every other parameter draws from, so the volume values are packed into two
/// vectors instead of being spelt out. `attenuation` is the volume tint with its distance in the
/// fourth component, `volume` is the transmission factor, the thickness and the index of
/// refraction.
struct DXSceneConstants {
  float albedo[4]{};
  float specular[4]{};
  float emissive[4]{};
  float surface[4]{};
  float attenuation[4]{};
  float volume[4]{};
  uint32_t textureMask{};
  uint32_t unlit{};
  float alphaCutoff{};
  uint32_t alphaMode{};
};
/// @brief Root constants for the fullscreen post-processing pipelines.
///
/// One layout for every post pass. `parameter` is whatever that pass's shader reads it as: a gamma,
/// a luminance threshold, a bloom intensity. `horizontal` picks the axis for the separable blur,
/// which is the one pass that runs more than once with different constants.
///
/// `exposure` and `toneMapper` are filled for every pass rather than only for the one that maps the
/// image, because the bright pass thresholds on the exposed luminance and so needs the first of
/// them. Both are laid out to stay clear of a sixteen byte boundary, which is what decides how the
/// matching `cbuffer` in `post.hlsl` packs them.
struct DXPostConstants {
  float parameter{};
  float texelSize[2]{};
  uint32_t horizontal{};
  float exposure{};
  uint32_t toneMapper{};
  /// @brief Hands the image to the display untouched: no exposure, no curve, no transfer function.
  ///
  /// Set while a lighting debug view is up. Those views write a quantity rather than a radiance --
  /// an occlusion factor, a weight, a coordinate in a cell -- and putting a quantity through a curve
  /// built to make light look like a photograph leaves a picture of the number rather than the
  /// number. Half of what the views are for is reading a value off the screen and saying whether it
  /// is the value it should be, which a tone mapper makes impossible.
  uint32_t raw{};
};
/// @brief Largest number of selected entities the outline pass can consider in one draw.
inline constexpr uint32_t MAX_OUTLINE_SELECTED = 16;
/// @brief Root constants for the outline pass.
///
/// The selected set is passed inline rather than through a buffer because it is tiny and changes
/// every frame; `uint4` groups keep the HLSL packing rules from padding each id to 16 bytes.
struct DXOutlineConstants {
  float color[4]{};
  float texelSize[4]{};
  uint32_t selectedCount[4]{};
  uint32_t selected[MAX_OUTLINE_SELECTED]{};
};
/// @brief Number of shader resource and unordered access slots the shared compute signature binds.
///
/// Every image-based lighting shader is compiled against one root signature, so the tables are
/// sized for the union of what any of them needs. A dispatch fills the slots it uses and points
/// the rest at stand-in resources, because a descriptor a table can reach has to be a real one
/// even when the shader never reads it.
inline constexpr uint32_t COMPUTE_SRV_SLOTS = 4;
inline constexpr uint32_t COMPUTE_UAV_SLOTS = 3;
/// @brief Root constants shared by the image-based lighting compute shaders.
///
/// One struct for all of them rather than one per shader: they are dispatched in a fixed sequence
/// from a single place, and a shared layout keeps that sequence readable. Each shader reads the
/// few fields that mean something to it.
struct DXIBLConstants {
  uint32_t size{};
  uint32_t mipLevels{};
  uint32_t sourceSize{};
  uint32_t reserved{};
  float roughness{};
  float mipLevel{};
  uint32_t faceSize{};
  uint32_t padding{};
};
/// @brief Root constants for the skybox pass.
///
/// The inverse view-projection turns a screen-space position back into a world-space view ray.
/// Only the camera's rotation contributes, so the translation is stripped before inverting and
/// the ray is the same wherever the camera stands, which is what makes the sky look infinitely far.
struct DXSkyboxConstants {
  float inverseViewProjection[16]{};
  uint32_t useTexture{};
  uint32_t useGradient{};
  uint32_t padding[2]{};
};
/// @brief Root constants for the compute shader that traces a grid of rays to check the scene.
///
/// The inverse view-projection and the origin between them turn a cell of the grid into a world
/// ray, exactly as the skybox pass turns a pixel into a view direction. `dimensions` carries the
/// grid's extent so the threads past its edge can leave early.
struct DXRayProbeConstants {
  float inverseViewProjection[16]{};
  float origin[4]{};
  uint32_t dimensions[4]{};
};
/// @brief Root constants describing the probe volume, for any shader that samples it.
///
/// `origin` is the volume's minimum corner with its cubic side length in the fourth component, which
/// is everything needed to turn a world position into a lookup cell. `grid` carries the lookup
/// resolution, the audit grid's own resolution, and the probe and node counts a sampler bounds-checks
/// its indices against.
struct DXProbeVolumeConstants {
  float origin[4]{};
  uint32_t grid[4]{};
};
/// @brief Root constants for the compute shader that traces rays out of every probe.
///
/// The rotation is a per-frame random orthonormal basis applied to the fixed spherical Fibonacci
/// ray set. Without it the same sixty-four directions are sampled every frame, so the estimate
/// converges to whatever those directions happen to see and the bias never averages out; with it,
/// the temporal blend is what integrates the sphere rather than the ray count alone.
///
/// `volume` is the probe volume's minimum corner and cubic side, `counts` holds the probe count,
/// the node count, the frame index and the lookup resolution, and `params` the temporal hysteresis,
/// the furthest a ray may travel, the distance a hit is nudged along its normal before shading, and
/// how tightly a ray must line up with a visibility texel to count towards it.
///
/// The two vectors after it were constants in the shader until it became clear that each is a
/// tradeoff rather than a value with a right answer -- see `IndirectLighting`. Root
/// constants rather than a buffer because there is one dispatch a frame and this signature spends
/// 48 of its 64 DWORDs even with them, so a constant here costs nothing that is scarce.
struct DXProbeTraceConstants {
  float rotation[3][4]{};
  float volume[4]{};
  uint32_t counts[4]{};
  float params[4]{};
  /// @brief What keeps a probe out of the geometry it was placed in: the fraction of its allowance
  /// it may step each trace, how much of that step is held back, how much the rays behind it must
  /// agree before their mean is a direction, and the share of rays coming back off a far side that
  /// has it disbelieved and filled in from its neighbours.
  ///
  /// The allowance those first two are fractions *of* is not here. It is baked into each probe's
  /// anchor when the volume is built, so changing it means rebuilding rather than retracing, which
  /// is a different tier of cost and belongs with the octree settings rather than with these.
  float relocation[4]{};
  /// @brief How far a probe's visibility reaches as a fraction of the volume's side, and how much
  /// light must still be getting through an alpha-blended chain before a ray stops being followed.
  float field[4]{};
};
/// @brief Root constants for the pass that draws the probe volume as a field of spheres.
///
/// `scale` is the diameter each sphere is drawn at, in world units. It is derived from the volume's
/// finest possible probe spacing rather than fixed, so the spheres stay separated at whatever size
/// the scene is and however deep the octree went.
struct DXProbeDebugConstants {
  float viewProjection[16]{};
  float scale{};
  /// @brief Which of the probe's fields the spheres are coloured by. A `ProbeDebugView`.
  uint32_t mode{};
  /// @brief How far a probe's stored distances reach, in world units.
  ///
  /// The distance view needs something to divide by or it can only show whether a texel is near or
  /// far in units no one can read. This is the clamp the trace records against, so a sphere reading
  /// white is one whose rays reached as far as the probe is able to see and not merely far.
  float range{};
  float padding{};
};
/// @brief Root constants for the shadow depth pass: the light's view-projection.
///
/// The per-instance model matrix is combined with this in the shader, from the same instance
/// buffer the scene pass reads. Rasterising a caster without its model matrix would record depths
/// that have nothing to do with where the geometry actually is.
struct DXShadowConstants {
  float lightViewProjection[16]{};
};
/// @brief One compiled graphics pipeline plus the root signature it was created against.
struct DXPipeline {
  ComPtr<ID3D12RootSignature> rootSignature;
  ComPtr<ID3D12PipelineState> pipelineState;
  explicit operator bool() const {
    return rootSignature && pipelineState;
  }
};
/// @brief Builds and caches the pipelines the Direct3D 12 passes draw with.
///
/// A pipeline state object bakes in the render-target format and sample count, so a separate
/// object is needed per target configuration. They are cached on that pair.
///
/// The raster and compute pipelines here are built at Shader Model 6.0, the lowest the standalone
/// compiler emits and a floor every Direct3D 12 driver clears. Only a pass that needs a later
/// model should ask for one, and only after `DXCapabilities` says the device has it.
class KUKI_ENGINE_API DXPipelineCache {
public:
  /// @brief Returns the unlit scene pipeline for a target format and sample count, building it once.
  /// @return Null when compilation or creation failed; the caller should skip the draw.
  /// @brief Returns the scene pipeline for a target format and sample count, building it once.
  ///
  /// Writes two render targets: shaded colour, and the entity id the picking and outline passes
  /// read. Every target in a pipeline must share a sample count, which is why the id buffer is
  /// allocated multisampled alongside the colour target rather than as a resolved companion.
  ///
  /// The skinned variant differs only in its vertex stage and input layout; both share a root
  /// signature and a pixel shader, so a skinned mesh is shaded identically to a static one.
  auto GetScenePipeline(ID3D12Device *, const DXGI_FORMAT, const uint32_t, const bool = false, const bool = false) -> const DXPipeline *;
  /// @brief Returns a fullscreen post-processing pipeline for a target format, building it once.
  ///
  /// Post passes take no vertex buffer: the vertex shader generates a single oversized triangle
  /// from `SV_VertexID`, which covers the target with one draw and no input assembler state.
  ///
  /// @param effect Selects the pixel shader entry point, so all post passes share one cache.
  /// @return Null when compilation or creation failed; the caller should fall back to a blit.
  auto GetPostPipeline(ID3D12Device *, const DXGI_FORMAT, const char *) -> const DXPipeline *;
  /// @brief Returns the outline pipeline for a target format, building it once.
  ///
  /// Distinct from the generic post pipeline because it binds a second resource: the multisampled
  /// entity id buffer, read with `Load` so ids are never filtered.
  auto GetOutlinePipeline(ID3D12Device *, const DXGI_FORMAT) -> const DXPipeline *;
  /// @brief Returns the skybox pipeline for a target format and sample count, building it once.
  ///
  /// Samples the same cubemap the image-based lighting passes read, which the compute path builds
  /// from the equirectangular source. Sampling the source directly would draw the same background
  /// by a second path, and two paths over one texture can disagree: a projection fixed in one and
  /// not the other shows up as a sky that no longer matches the light reflecting off the scene.
  ///
  /// Shares the scene target's sample count so it can draw into the same multisampled targets, and
  /// writes the entity id buffer as well, leaving the sky unpickable.
  auto GetSkyboxPipeline(ID3D12Device *, const DXGI_FORMAT, const uint32_t) -> const DXPipeline *;
  /// @brief Returns an image-based lighting compute pipeline by entry point, building it once.
  ///
  /// All of them share a root signature, so a dispatch can swap pipelines without rebinding
  /// anything it did not change.
  ///
  /// @param entryPoint Compute entry point in the shared image-based lighting HLSL.
  /// @return Null when compilation or creation failed; the caller should skip the dispatch.
  auto GetComputePipeline(ID3D12Device *, const char *) -> const DXPipeline *;
  /// @brief Returns the depth-only pipeline the shadow pass renders with, building it once.
  ///
  /// Bound with no render target and no pixel shader: the pass exists purely to populate a depth
  /// buffer from the light's point of view, so rasterising colour would be wasted work.
  auto GetShadowPipeline(ID3D12Device *) -> const DXPipeline *;
  /// @brief Returns the compute pipeline that traces rays against the scene, building it once.
  ///
  /// The one pipeline here built above Shader Model 6.0: `RayQuery` entered the language at 6.5, so
  /// this fails to compile on a device that reports less. Callers must ask `DXCapabilities` whether
  /// the device supports inline raytracing rather than treating a null return as a transient error.
  auto GetRayProbePipeline(ID3D12Device *) -> const DXPipeline *;
  /// @brief Returns the compute pipeline that checks the probe volume against itself, building it once.
  ///
  /// Plain Shader Model 6.0: it reads buffers and traces nothing, so it runs wherever the backend does.
  auto GetProbeAuditPipeline(ID3D12Device *) -> const DXPipeline *;
  /// @brief Returns the compute pipeline that traces rays out of the probes, building it once.
  ///
  /// Needs Shader Model 6.5 for `RayQuery` and resource binding tier 3 for the unbounded arrays it
  /// reaches geometry and materials through, so callers must check both on `DXCapabilities` first.
  auto GetProbeTracePipeline(ID3D12Device *) -> const DXPipeline *;
  /// @brief Returns the pipeline that draws the probes as spheres, building one per target format.
  ///
  /// Keyed on format and sample count like the scene pipeline, because it draws into the scene's
  /// own render targets and a pipeline whose output description disagrees with them is rejected.
  auto GetProbeDebugPipeline(ID3D12Device *, const DXGI_FORMAT, const uint32_t) -> const DXPipeline *;
  auto Clear() -> void;
private:
  std::unordered_map<uint64_t, DXPipeline> pipelines;
  DXShaderCompiler shaderCompiler;
  ComPtr<ID3D12RootSignature> sceneRootSignature;
  ComPtr<ID3D12RootSignature> postRootSignature;
  ComPtr<ID3D12RootSignature> outlineRootSignature;
  ComPtr<ID3D12RootSignature> shadowRootSignature;
  ComPtr<ID3D12RootSignature> skyboxRootSignature;
  ComPtr<ID3D12RootSignature> computeRootSignature;
  ComPtr<ID3D12RootSignature> rayProbeRootSignature;
  ComPtr<ID3D12RootSignature> probeAuditRootSignature;
  ComPtr<ID3D12RootSignature> probeTraceRootSignature;
  ComPtr<ID3D12RootSignature> probeDebugRootSignature;
  auto GetSceneRootSignature(ID3D12Device *) -> ID3D12RootSignature *;
  /// @brief Root signature shared by every post pass: one source texture plus a scalar parameter.
  auto GetPostRootSignature(ID3D12Device *) -> ID3D12RootSignature *;
  /// @brief Root signature for the outline pass: source colour, entity ids, and the selected set.
  auto GetOutlineRootSignature(ID3D12Device *) -> ID3D12RootSignature *;
  /// @brief Root signature for the shadow pass: a single light-space matrix.
  auto GetShadowRootSignature(ID3D12Device *) -> ID3D12RootSignature *;
  /// @brief Root signature for the skybox pass: the view ray transform and the environment map.
  auto GetSkyboxRootSignature(ID3D12Device *) -> ID3D12RootSignature *;
  /// @brief Root signature shared by every image-based lighting compute shader.
  auto GetComputeRootSignature(ID3D12Device *) -> ID3D12RootSignature *;
  /// @brief Root signature for the ray probe: the camera, the scene, its geometry, and a result.
  ///
  /// Every binding is a root descriptor, so the pass needs no descriptor heap of its own. That is a
  /// raytracing tier 1.1 privilege as far as the acceleration structure goes: before it, a structure
  /// could only be reached through a table.
  auto GetRayProbeRootSignature(ID3D12Device *) -> ID3D12RootSignature *;
  /// @brief Root signature for the volume audit: the volume's three buffers and a result to write.
  auto GetProbeAuditRootSignature(ID3D12Device *) -> ID3D12RootSignature *;
  /// @brief Root signature for the probe trace, and the first here to use unbounded tables.
  ///
  /// The scene, the geometry records, the volume and the probes are all root descriptors. The mesh
  /// buffers and material textures cannot be: a ray reports which triangle of which instance it hit
  /// and nothing more, so the shader picks its buffer and its texture per lane, from arrays whose
  /// size is not known when the dispatch is recorded. Both are declared unbounded and both are
  /// bound at the heap's first slot, which is what lets a record store one absolute index that
  /// reaches a buffer and a texture alike.
  auto GetProbeTraceRootSignature(ID3D12Device *) -> ID3D12RootSignature *;
  /// @brief Root signature for the probe visualisation: the camera transform and the probes.
  ///
  /// The probes are a root descriptor read by both stages, since the vertex shader takes a probe's
  /// position and the pixel shader takes the coefficients stored beside it.
  auto GetProbeDebugRootSignature(ID3D12Device *) -> ID3D12RootSignature *;
};
} // namespace kuki
#endif
