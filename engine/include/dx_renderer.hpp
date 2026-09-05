#pragma once
#ifdef KUKI_HAS_DIRECTX
#include <array>
#include <bounding_box.hpp>
#include <desktop_size.hpp>
#include <dx_acceleration_structure.hpp>
#include <dx_common.hpp>
#include <dx_compute_texture.hpp>
#include <glm/ext/matrix_float4x4.hpp>
#include <material_asset.hpp>
#include <dx_context.hpp>
#include <dx_mesh.hpp>
#include <dx_pipeline.hpp>
#include <dx_probe_volume.hpp>
#include <dx_render_target.hpp>
#include <dx_material.hpp>
#include <dx_texture.hpp>
#include <dx_texture_view.hpp>
#include <mesh.hpp>
#include <texture.hpp>
#include <id.hpp>
#include <kuki_engine_export.h>
#include <renderer.hpp>
#include <span>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
namespace kuki {
/// @brief One draw call's worth of work: a mesh, a material, and every instance of the pair.
///
/// The material may be null, which the depth-only passes rely on: they group purely by mesh
/// because a shadow caster's shading is irrelevant to the depth it writes.
struct DXDrawBatch {
  const DXMesh *mesh{};
  const DXMaterial *material{};
  /// @brief Every instance of the pair, with the ones the camera can see first.
  ///
  /// The hidden ones are still here because a ray does not care where the camera is pointing: the
  /// acceleration structure is built from this whole array, and dropping what is off screen would
  /// make reflections and probe lighting change as the camera turns.
  std::vector<DXInstanceData> instances;
  /// @brief How many of `instances` the rasteriser should draw, always a prefix of the array.
  ///
  /// Equal to the whole array when the batch was collected without a camera, which is what the
  /// depth-only passes do.
  uint32_t visibleCount{};
};
/// @brief What two entities must agree on to share a draw call.
///
/// A struct rather than a tuple so the fields say what they are, and compared exactly rather than
/// by hash, so a hash collision costs one comparison instead of merging two unrelated batches.
struct DXBatchKey {
  const DXMesh *mesh{};
  uint64_t textureTable{};
  uint32_t variant{};
  auto operator==(const DXBatchKey &) const noexcept -> bool = default;
};
struct KUKI_ENGINE_API DXBatchKeyHash {
  auto operator()(const DXBatchKey &) const noexcept -> size_t;
};
/// @brief One draw held back until every opaque surface has been rasterised.
///
/// Two unrelated kinds of surface end up here for the same reason. Blended ones cannot be drawn
/// where they are found because the result depends on the order they reach the blend unit, so they
/// are sorted back to front and issued after the depth buffer they test against exists.
/// Transmissive ones are opaque as far as the blend unit is concerned, but they read the scene
/// behind them out of a copy of the colour target, and that copy is only complete once everything
/// opaque has been drawn.
///
/// `blended` decides which of the two a draw is, and with it which pipeline issues it: the blended
/// variant with blending on, depth writes off and no culling, or the ordinary opaque one. A
/// transmissive surface wants the opaque state, since it composites the background itself and
/// drawing its back faces as well would refract the scene twice.
///
/// `depth` is the distance from the camera to the instance's origin, which is what the sort keys
/// on. Sorting by origin is exact only for draws that do not intersect; two crossing transparent
/// meshes still resolve by whichever origin is further, and a single concave transparent mesh
/// still blends its own faces in index order. Per-object sorting is the standard compromise and
/// the alternative is order-independent transparency, which is a different technique entirely.
///
/// Both address fields are already allocated when the draw is collected, since the arena's
/// allocation order has nothing to do with the order the draws are issued in.
struct DXDeferredDraw {
  const DXMesh *mesh{};
  const DXMaterial *material{};
  D3D12_GPU_VIRTUAL_ADDRESS instances{};
  D3D12_GPU_VIRTUAL_ADDRESS bones{};
  uint32_t count{};
  bool skinned{};
  bool blended{};
  float depth{};
};
/// @brief One skinned mesh, which is always drawn alone.
///
/// Skinned meshes cannot be batched: the bone palette is per entity, so two entities sharing a
/// mesh still pose it differently. The instance is carried anyway, to supply the entity id the
/// picking buffer needs.
struct DXSkinnedDraw {
  const DXMesh *mesh{};
  const DXMaterial *material{};
  DXInstanceData instance{};
  std::vector<glm::mat4> bones;
};
/// @brief Direct3D 12 implementation of the engine renderer.
///
/// Feature parity with `GLRenderer` is close. The scene draw is physically based and instanced,
/// with the same directional, point and spot lights, the same shadow maps, skinning, the skybox,
/// the post-processing passes, the outline, picking readback and asset previews.
///
/// Image-based lighting is precomputed the same way: the environment is projected onto a cubemap,
/// reduced to spherical harmonics for the diffuse irradiance, prefiltered by roughness for the
/// specular reflection, and paired with an integrated split-sum lookup table. A scene with no
/// skybox falls back to the analytic sky `lit.frag` uses when those maps are missing.
class KUKI_ENGINE_API DXRenderer final : public Renderer {
public:
  DXRenderer(Application &);
  /// @brief Resolves the multisampled scene target into the single-sampled one.
  ///
  /// The anti-aliasing is the multisampling itself; this pass only collapses the samples. That is
  /// exactly what the OpenGL backend's blit between a multisampled and a single-sampled buffer
  /// does, so there is no post-process filter missing here.
  auto ApplyAntiAliasing(std::span<std::string>, std::span<std::string>) -> void override;
  auto ApplyBloomEffect(std::span<std::string>, std::span<std::string>) -> void override;
  /// @brief Blurs the first input into the first output through a chain of single-axis passes.
  ///
  /// A two-dimensional Gaussian separates into a horizontal and a vertical pass, so the passes
  /// alternate axes and ping-pong between two scratch targets named after the output. Running the
  /// pair repeatedly widens the blur far more cheaply than widening the kernel would.
  auto ApplyBlurEffect(std::span<std::string>, std::span<std::string>) -> void override;
  auto ApplyBrightPassFilter(std::span<std::string>, std::span<std::string>) -> void override;
  auto ApplyToneMapping(std::span<std::string>, std::span<std::string>) -> void override;
  /// @brief Draws the selection outline over the scene, or passes the scene through unchanged.
  ///
  /// Takes the colour to draw over from the first single-sampled input and the entity ids from the
  /// input that carries an id buffer, rather than by name, so the pass works wherever a graph puts
  /// it. The pass-through taken when nothing is selected has to copy that same colour input: the
  /// multisampled input is listed first and copying it instead would silently discard whatever the
  /// passes in between had already applied.
  auto ApplyOutline(std::span<std::string>, std::span<std::string>) -> void override;
  auto Clear() -> void override;
  auto CreateDepthPrepass(std::span<std::string>, std::span<std::string>) -> void override;
  auto CreateShadowMap(std::span<std::string>, std::span<std::string>) -> void override;
  auto CreateSpotShadowMap(std::span<std::string>, std::span<std::string>) -> void override;
  auto CreateTarget(const TargetDescription &, const std::string & = "") -> EntityID override;
  auto GetPreviewSize() const -> int override;
  auto GetTarget(const std::string &) -> RenderTarget * override;
  auto LoadAsset(const AssetID) -> void override;
  auto LoadAssets(const AssetType) -> void override;
  /// @brief Resolves scene handles into `DXMaterial` components and uploads what they reference.
  ///
  /// Collects first and mutates afterwards. Adding a component moves the entity to a different
  /// archetype, which invalidates both the component pointers and the iteration in progress, so
  /// nothing may be added while a `ForEachEntity` walk is still running.
  auto LoadScene(Scene &) -> void override;
  /// @brief Reads back the entity id written at a pixel of the scene pass's picking buffer.
  ///
  /// Synchronous: the id buffer is resolved, one pixel is copied to a readback heap, and the GPU
  /// is waited on before the value can be mapped. OpenGL hides this behind `glReadPixels`; here the
  /// stall is explicit and unavoidable without accepting a frame or two of latency.
  ///
  /// The resolve averages samples, so a pixel exactly on a silhouette can yield an id belonging to
  /// neither object. This matches the OpenGL path's blit and is only wrong on edge pixels.
  ///
  /// @return The entity under the pixel, or an invalid id when nothing was hit.
  auto PickEntity(const int, const int) -> EntityID override;
  /// @brief Returns something the editor can display for an asset, or null when it cannot.
  ///
  /// A texture asset previews as itself, through the shader resource view its upload already
  /// produced. Everything else is rendered off-screen once, framed by its own bounds, and cached.
  ///
  /// Callers must still cope with null: an asset whose geometry has not finished loading has
  /// nothing to draw yet, and the editor puts a plain button in its place until it has.
  auto PreviewAsset(const AssetID) -> RenderTarget * override;
  auto RenderScene(std::span<std::string>, std::span<std::string>) -> void override;
  /// @brief Per-frame reset hook, called by the render graph before it recreates its targets.
  ///
  /// Rewinds the instance arena, which every pass in the frame allocates from in turn. This is the
  /// only place that can do it: a pass cannot rewind on entry without discarding what the passes
  /// before it recorded, since none of them have executed yet.
  ///
  /// Render targets are deliberately left alone. The OpenGL backend uses this hook to return
  /// transient resources to its pools, which makes the graph's recreate-every-frame pattern cheap
  /// there. Direct3D 12 targets are persistent committed resources, so tearing them down here
  /// would reallocate every target on every frame. `CreateTarget` already no-ops when a target
  /// exists with a matching description, so leaving them alone is both correct and free.
  auto PresentTarget(const std::string &) -> void override;
  auto Reset() -> void override;
  auto SetPreviewSize(const int) -> void override;
  /// @brief Bytes of upload staging allowed in flight before the upload path stops and waits.
  auto GetUploadBudget() const -> size_t;
  /// @brief Resizes the upload batch. Safe at any time; takes effect at the next texture.
  ///
  /// Lowering it below what is already staged does not cancel anything: the open batch runs to its
  /// end and the new, smaller budget bounds the batch after it.
  auto SetUploadBudget(const size_t) -> void;
  auto SetResolution(const int = 1920, const int = 1080) -> void override;
  /// @brief What this backend can do on this machine, which is not the same question.
  ///
  /// The probe field needs bindless descriptors to reach its material table and inline raytracing
  /// to cast a ray at all, and the context already says at startup when the hardware has neither
  /// and that the traced passes will be skipped. Reporting the field as present anyway would leave
  /// the editor offering a panel of knobs onto passes that are not running, which is the same
  /// complaint one level down.
  auto GetCapabilities() const -> RendererCapabilities override;
  auto TraceProbes(std::span<std::string>, std::span<std::string>) -> void override;
  auto UpdateTarget(const std::string &, const TargetDescription &) -> void override;
private:
  DXPipelineCache pipelines;
  /// @brief The same geometry the scene pass rasterises, arranged so a ray can be traced against it.
  ///
  /// Rebuilt from the scene pass's own batches, so the two can never disagree about what is in the
  /// scene. Nothing reads it yet; it exists for the indirect lighting that will.
  DXAccelerationStructure rayScene;
  /// @brief What `rayScene` was last filled with, as one number: placements and their materials.
  ///
  /// The probe volume averages its estimates over as many frames as the scene stays the scene, so
  /// it has to be told when that stops being true. Its own rebuild hash cannot answer this: that
  /// one covers where the geometry is, and light bounces off a surface's colour just as much.
  size_t raySceneHash{};
  /// @brief Where indirect light is gathered, and how a shading point finds the nearest gatherings.
  ///
  /// Built from the same geometry `rayScene` is, but from the processor-side copy rather than the
  /// uploaded buffers: placing probes means sorting triangles into cells, which needs to read them.
  DXProbeVolume probeVolume;
  std::unordered_map<AssetID, DXMesh> assetToMesh;
  std::unordered_map<AssetID, DXTexture> assetToTexture;
  std::unordered_map<AssetID, DXTextureView> assetToPreview;
  std::unordered_map<AssetID, DXRenderTarget> assetToPreviewTarget;
  std::unordered_map<AssetID, std::unordered_map<size_t, DXMesh>> modelToMeshes;
  std::unordered_map<AssetID, std::unordered_map<size_t, DXTexture>> modelToTextures;
  std::unordered_map<std::string, DXRenderTarget> nameToTarget;
  std::unordered_map<std::string, EntityID> nameToId;
  DXTexture dummyTexture;
  /// @brief A one-slice array view of the dummy texture, for the spot shadow table when unused.
  ///
  /// A `Texture2DArray` register cannot be satisfied by a `Texture2D` view, so the stand-in the
  /// material slots use is not enough on its own. The resource is the same one pixel either way.
  uint32_t dummyArraySrvIndex{DXDescriptorHeap::InvalidIndex};
  D3D12_GPU_DESCRIPTOR_HANDLE dummyArraySrvGPU{};
  /// @brief An all-white texture table, bound by draws whose entity carries no material at all.
  DXMaterial fallbackMaterial;
  D3D12_GPU_DESCRIPTOR_HANDLE fallbackMaterialTable{};
  /// @brief Texture tables keyed on the asset they came from, so entities can share one.
  ///
  /// Sharing is what makes instancing possible: a descriptor table is bound once per batch, so two
  /// entities can only be drawn together if they resolve to the same table. Building one table per
  /// entity would give every entity a distinct handle and quietly reduce every batch to one.
  ///
  /// Standalone material assets are stored under index zero; a model's materials under their index
  /// within it. The two cannot collide, since a model and a material never share an asset id.
  std::unordered_map<AssetID, std::unordered_map<size_t, DXMaterial>> materialTables;
  /// @brief Per-frame-in-flight instance data, filled by every pass that draws geometry.
  ///
  /// One arena per frame, consumed by a cursor that the render graph resets at the start of each
  /// frame. The shadow passes and the scene pass all write into the same arena, which is why the
  /// cursor cannot be reset per pass.
  ComPtr<ID3D12Resource> instanceBuffer;
  uint8_t *instanceData{};
  uint64_t instanceCapacity{};
  uint64_t instanceCursor{};
  /// @brief Storage `CollectBatches` refills rather than rebuilds, and the span it hands back.
  ///
  /// Grouping the scene means one instance array per batch, and a scene where every mesh is its own
  /// batch turns that into an allocation per mesh per call, three times a frame. Keeping the arrays
  /// and clearing only their contents leaves the capacity in place, so a steady scene allocates
  /// nothing here after its first frame. `batchPool` may hold more batches than the last call used;
  /// the returned span is what says how many are live.
  std::vector<DXDrawBatch> batchPool;
  /// @brief Per batch, the instances the camera rejected, held aside until the sweep ends.
  ///
  /// Parallel to `batchPool` and pooled for the same reason.
  std::vector<std::vector<DXInstanceData>> hiddenPool;
  std::unordered_map<DXBatchKey, size_t, DXBatchKeyHash> batchLookup;
  /// @brief The environment maps the ambient term samples, precomputed once per skybox texture.
  DXComputeTexture skyboxCubemap;
  DXComputeTexture irradianceMap;
  DXComputeTexture prefilterMap;
  DXComputeTexture brdfLUT;
  /// @brief A one-texel cube standing in wherever a real environment map is absent.
  ///
  /// Serves two purposes: it keeps the scene shader's cube registers pointing at something real
  /// when a scene has no skybox, and it fills the compute tables' unused slots. A descriptor a
  /// shader could reach must resolve to a resource even when that shader never reads it.
  DXComputeTexture dummyCubemap;
  /// @brief Processor-side heap the compute views are authored in, then copied out of.
  ///
  /// The shader-visible heap cannot be a copy source, so views that need to be placed into a table
  /// on demand are built here first. Nothing binds this heap; it exists only to be copied from.
  DXDescriptorHeap computeStagingHeap;
  uint32_t dummyEquirectSrv{DXDescriptorHeap::InvalidIndex};
  /// @brief Nine spherical harmonic coefficients, written by one dispatch and read by the next.
  ComPtr<ID3D12Resource> shBuffer;
  uint32_t shSrvIndex{DXDescriptorHeap::InvalidIndex};
  uint32_t shUavIndex{DXDescriptorHeap::InvalidIndex};
  D3D12_RESOURCE_STATES shState{D3D12_RESOURCE_STATE_COMMON};
  /// @brief Contiguous irradiance, prefilter and lookup-table views, as the scene pass binds them.
  uint32_t environmentTableIndex{DXDescriptorHeap::InvalidIndex};
  D3D12_GPU_DESCRIPTOR_HANDLE environmentTable{};
  /// @brief The same three views over stand-in resources, bound when no environment was built.
  uint32_t fallbackEnvironmentIndex{DXDescriptorHeap::InvalidIndex};
  D3D12_GPU_DESCRIPTOR_HANDLE fallbackEnvironmentTable{};
  /// @brief The cubemap view the skybox pass samples, and the stand-in cube it falls back to.
  ///
  /// Separate from the environment table because the two are bound by different root signatures,
  /// and a table's descriptors have to be contiguous in the order its own signature declares them.
  uint32_t skyboxTableIndex{DXDescriptorHeap::InvalidIndex};
  D3D12_GPU_DESCRIPTOR_HANDLE skyboxTable{};
  uint32_t fallbackSkyboxIndex{DXDescriptorHeap::InvalidIndex};
  D3D12_GPU_DESCRIPTOR_HANDLE fallbackSkyboxTable{};
  /// @brief The skybox texture the current environment maps were built from.
  AssetID environmentAsset{};
  bool environmentReady{};
  /// @brief Per-frame-in-flight storage for `DXFrameConstants`, mapped for the process's lifetime.
  ///
  /// One region per frame the context keeps in flight. Overwriting a single shared region would
  /// race the GPU, which may still be reading the previous frame's lights while the CPU records
  /// the next one.
  ///
  /// Each region holds many slots, not one, because a frame uploads constants more than once: the
  /// scene pass does, and so does every asset preview drawn afterwards. A single slot per frame
  /// meant the preview's camera landed on the address the scene pass had already pointed a root
  /// constant buffer view at, and the scene was drawn through the preview's camera.
  ComPtr<ID3D12Resource> frameConstantBuffer;
  uint8_t *frameConstantData{};
  uint64_t frameConstantStride{};
  /// @brief How many uploads one frame's region can hold.
  uint64_t frameConstantSlots{};
  /// @brief Slots taken so far this frame, reset by the render graph along with the instance arena.
  uint64_t frameConstantCursor{};
  /// @brief Buffers replaced by a larger one, kept alive until the GPU can no longer be reading them.
  ///
  /// Growing an upload buffer mid-frame cannot simply release the old one. The command list being
  /// recorded already holds raw GPU addresses into it, and it has not been submitted yet, so
  /// nothing else is keeping the resource alive; freeing it leaves those draws reading whatever
  /// takes its place. Holding the old buffer for a few frames keeps both the address and the bytes
  /// behind it valid for as long as anything can still reference them.
  std::vector<std::pair<uint64_t, ComPtr<ID3D12Resource>>> retiredUploadBuffers;
  /// @brief A descriptor range that is finished with but may still be named by a recorded list.
  struct DXRetiredRange {
    uint64_t epoch{};
    uint32_t first{};
    uint32_t count{};
  };
  std::vector<DXRetiredRange> retiredRanges;
  /// @brief Counts frames, only so retired buffers can be aged out.
  uint64_t uploadEpoch{};
  uint32_t spotShadowCount{};
  std::array<glm::mat4, MAX_SPOT_LIGHTS> spotShadowViewProjection{};
  /// @brief Where the pick shader writes the one id it read, and where that id is read back from.
  ///
  /// Four bytes apiece and sized once: a pick asks about a single texel, so neither of these has
  /// anything to do with how big the target is. The pair replaces a full-size resolve texture,
  /// which was both larger than the question and wrong -- see `DXRenderer::PickEntity`.
  ComPtr<ID3D12Resource> pickResultBuffer;
  ComPtr<ID3D12Resource> pickReadbackBuffer;
  EntityID nextTargetId{1};
  int previewSize{128};
  /// @brief Staging buffers whose copies are recorded into the open command list but not yet run.
  std::vector<ComPtr<ID3D12Resource>> pendingStaging;
  /// @brief Upload-heap bytes those buffers hold. The quantity the budget actually bounds.
  ///
  /// An overestimate whenever unrelated work flushes the command list mid-batch, which costs one
  /// redundant flush and nothing else. Erring high is the safe direction: it can only make the
  /// batch end early, never make it outlive the memory it is meant to bound.
  size_t pendingStagingBytes{};
  /// @brief Current batch size in bytes. See `DEFAULT_UPLOAD_BUDGET` for how the default is picked.
  size_t uploadBudgetBytes{DEFAULT_UPLOAD_BUDGET};
  bool uploadBudgetChecked{};
  DXMesh probeMesh;
  /// @brief A single-sampled copy of the colour target as it stood when the opaque phase ended.
  ///
  /// What a transmissive surface refracts. It cannot sample the target it is drawing into, so the
  /// scene behind it has to be resolved out to a resource of its own first. Single-sampled because
  /// it is read as an ordinary texture and because the resolve is the copy: on a multisampled
  /// target one `ResolveSubresource` does both jobs at once.
  ///
  /// Holds no mip chain, so a rough transmissive surface refracts as sharply as a smooth one. The
  /// blur that roughness should cause needs a filtered chain to sample into, which is a downsample
  /// pass this does not yet have.
  ComPtr<ID3D12Resource> sceneColorCopy;
  D3D12_RESOURCE_STATES sceneColorState{D3D12_RESOURCE_STATE_COMMON};
  uint32_t sceneColorSrvIndex{DXDescriptorHeap::InvalidIndex};
  D3D12_GPU_DESCRIPTOR_HANDLE sceneColorTable{};
  int sceneColorWidth{};
  int sceneColorHeight{};
  DXGI_FORMAT sceneColorFormat{DXGI_FORMAT_UNKNOWN};
  int screenWidth{DesktopWidth()};
  int screenHeight{DesktopHeight()};
  /// @brief The context downcast from the application, or null when running on another backend.
  /// @brief Default bytes of texture staging to keep in flight before flushing.
  ///
  /// Sized against what a flush costs rather than against any texture count. A flush is a full
  /// pipeline drain — submit, signal, block until idle — whose fixed cost is roughly a fraction of
  /// a millisecond regardless of how much was submitted. Copies run at PCIe speed, call it 10 GB/s
  /// on an older link, so 128 MB is on the order of 10 ms of real work per drain and the fixed
  /// cost disappears into the noise. Doubling it again would halve an overhead that is already
  /// negligible while doubling a memory cost that is not, which is where the curve flattens.
  ///
  /// Narrowed at startup to a fraction of what DXGI reports for the non-local segment, since a
  /// budget the adapter cannot hold trades a stall for eviction, which is strictly worse.
  static constexpr size_t DEFAULT_UPLOAD_BUDGET = 128ull * 1024 * 1024;
  /// @brief Floor for `SetUploadBudget`. Below one 4K mip chain the batch degenerates to one
  /// texture per flush, which is the behaviour batching exists to remove.
  static constexpr size_t MIN_UPLOAD_BUDGET = 16ull * 1024 * 1024;
  /// @brief Share of the adapter's non-local budget the upload batch may occupy.
  static constexpr uint64_t UPLOAD_BUDGET_SHARE = 8;
  auto GetContext() const -> DXContext *;
  /// @brief Creates the resource and views for a target, releasing anything it previously held.
  ///
  /// Callers must ensure no open command list still references the old resource. Waiting on the
  /// GPU is not enough on its own: the frame's command list may already have recorded against it
  /// but not yet been submitted, so it has to be flushed first.
  auto AllocateTarget(DXRenderTarget &, const TargetDescription &, const std::string &) -> bool;
  auto BypassPass(const RenderPass, std::span<std::string>, std::span<std::string>) -> void override;
  auto BypassCopy(std::span<std::string>, std::span<std::string>) -> void override;
  auto BypassClear(std::span<std::string>) -> void override;
  auto ReleaseTarget(DXRenderTarget &) -> void;
  /// @brief Destroys every render target. For shutdown and device loss, not per frame.
  auto ReleaseAllTargets() -> void;
  /// @brief Emits a transition barrier only when the tracked state differs from the requested one.
  auto Transition(DXRenderTarget &, const D3D12_RESOURCE_STATES) -> void;
  /// @brief Binds the named outputs and clears them, the shared prologue of every pass.
  auto ClearOutputs(std::span<std::string>) -> void;
  /// @brief Runs a fullscreen pixel shader from the first input into the first output.
  ///
  /// The shared shape of every post-processing pass: transition the source to a shader resource,
  /// bind the destination as a render target, and draw one oversized triangle. Falls back to
  /// `BlitOrResolve` when the pipeline is unavailable or the source is multisampled, since a
  /// fullscreen pass cannot sample an MSAA target through a plain `Texture2D`.
  ///
  /// @param effect Pixel shader entry point in the shared post-processing HLSL.
  /// @param parameter Single scalar the effect interprets, such as the gamma exponent.
  /// @param horizontal Axis for the separable blur; ignored by every other effect.
  auto ApplyFullscreenEffect(std::span<std::string>, std::span<std::string>, const char *, const float = 0.f, const bool = false) -> void;
  /// @brief Copies the first input into the first output, resolving when the sample counts differ.
  ///
  /// Stands in for the post-processing passes that are not ported yet, so the graph still carries
  /// the scene image through to its final output instead of showing a cleared target.
  auto BlitOrResolve(std::span<std::string>, std::span<std::string>) -> void;
  /// @brief Creates the depth buffer a colour target draws against, sized to match it.
  auto EnsureDepthBuffer(DXRenderTarget &) -> bool;
  /// @brief Creates the result and readback buffers picking reads through, once for the process.
  auto EnsurePickResources() -> bool;
  /// @brief Uploads a mesh asset's geometry, reusing the existing buffers when already resident.
  auto EnsureMesh(const AssetID) -> DXMesh *;
  /// @brief Uploads a texture asset into a default-heap resource and creates its shader resource view.
  ///
  /// Must be called before a pass binds any pipeline state. The copy is recorded into the open
  /// command list, and a batch that fills up flushes it, which resets the list and discards the
  /// bound root signature, pipeline and render targets.
  ///
  /// The texture is not resident when this returns, only scheduled. That is safe for anything the
  /// same command list draws afterwards, since the GPU runs the list in order, and unsafe for
  /// anything that reads the resource on the CPU without flushing first.
  ///
  /// @return Null when the asset is missing, still loading, or in an unsupported format.
  auto EnsureTexture(const AssetID) -> DXTexture *;
  /// @brief Creates the one-pixel white texture that stands in for any unbound descriptor table.
  ///
  /// A descriptor table the shader can reach must point at a real descriptor. Leaving one unbound
  /// carries the same risk as the editor's zero `ImTextureID`: whatever the previous draw left in
  /// that root slot is what the shader reads, and on the first draw of a frame there is nothing
  /// there at all. Materials without an albedo map and scenes without a shadow map both hit this,
  /// so both bind this instead of skipping the table.
  ///
  /// Uploading may flush the command list, so this must be called before a pass binds pipeline
  /// state. `EnsureSceneResources` does that on the caller's behalf.
  auto EnsureDummyTexture() -> DXTexture *;
  /// @brief Uploads one mesh out of a model asset, keyed on the model and its index within it.
  ///
  /// Model geometry lives inside `ModelAsset::meshes` rather than as standalone `MeshAsset`
  /// objects, so it cannot go through `EnsureMesh`.
  auto EnsureModelMesh(const AssetID, const size_t) -> DXMesh *;
  /// @brief Uploads one texture out of a model asset, keyed on the model and its index within it.
  auto EnsureModelTexture(const AssetID, const size_t) -> DXTexture *;
  /// @brief Creates vertex and index buffers for raw mesh data. Shared by both mesh paths.
  auto UploadMeshData(const Mesh &, const std::string &) -> DXMesh;
  /// @brief Creates a texture resource and shader resource view for raw pixels. Shared by both texture paths.
  ///
  /// Handles both byte and floating-point pixel data. Three-channel sources are expanded to four
  /// on the way up, because Direct3D has no three-channel texture formats at all — unlike OpenGL,
  /// where `GL_RGB` is a legal internal format and the driver does the padding.
  ///
  /// The staging buffer outlives this call: it goes into the open upload batch, which either the
  /// budget or the end of the frame closes. Because closing a batch flushes, this must not run
  /// once a pass has bound pipeline state.
  auto UploadTextureData(const Texture &, const std::string &) -> DXTexture;
  auto UploadCompressedTextureData(const Texture &, const std::string &) -> DXTexture;
  /// @brief Adds a staging buffer to the open batch, ending the batch if it no longer fits.
  ///
  /// The batch is measured in bytes rather than in textures because bytes are what it costs. A
  /// budget of "eight textures" is anywhere between eight one-pixel icons and half a gigabyte of
  /// 4K staging, so a count bounds nothing that actually matters.
  auto RetireStaging(ComPtr<ID3D12Resource>, const size_t) -> void;
  /// @brief Executes the open batch and waits, reclaiming its staging memory immediately.
  ///
  /// Only reached when the batch has outgrown the budget, which is the one case where the wait
  /// earns its cost: the point of waiting is to get the upload heap back before allocating more.
  auto FlushPendingUploads() -> void;
  /// @brief Hands the tail of the open batch to the frame fence and stops tracking it.
  ///
  /// Costs nothing. The copies are already recorded ahead of every draw that samples them on the
  /// same command list, so the GPU orders them correctly without help; all that is left is keeping
  /// the staging alive until the frame retires, which is what `DXContext::RetireResource` does.
  auto DrainPendingUploads() -> void;
  /// @brief Narrows the upload budget to what this adapter can actually hold. Runs once.
  auto EnsureUploadBudget() -> void;
  /// @brief Uploads every mesh and texture the scene references, before any state is bound.
  ///
  /// Also closes whatever upload batch was left open, wherever it came from. Runs every frame, so
  /// no staging buffer is tracked by hand for longer than a frame regardless of which path staged it.
  auto EnsureSceneResources(Scene &) -> void;
  /// @brief Creates the ring of constant buffers the scene pass writes its per-frame lights into.
  ///
  /// Allocated once and kept mapped. Upload-heap memory is CPU-visible and write-combined, so the
  /// map costs nothing to hold and repeated map/unmap pairs would only add overhead.
  /// @brief Makes sure a frame's region can hold at least this many uploads, growing if not.
  auto EnsureFrameConstantBuffer(const uint64_t) -> bool;
  /// @brief Hands a descriptor range back once the frames that could still name it have passed.
  ///
  /// A table is read where the command list executes, not where it was recorded, so returning the
  /// range at the point the code has finished with it would let the next taker overwrite
  /// descriptors a queued draw is about to read. This puts it on the same epoch the upload buffers
  /// ride, and `Reset` releases it when it is old enough.
  auto RetireDescriptorRange(const uint32_t, const uint32_t) -> void;
  /// @brief Gives up a material's texture table, if it has one.
  auto ReleaseMaterialTable(DXMaterial &) -> void;
  /// @brief Gives up a texture's shader resource view and its resource.
  auto ReleaseTexture(DXTexture &) -> void;
  /// @brief Hands a buffer to `retiredUploadBuffers` rather than releasing it outright.
  auto RetireUploadBuffer(ComPtr<ID3D12Resource>) -> void;
  /// @brief Gathers the frame's camera and lights into the layout the scene shader expects.
  ///
  /// Lights are read in scene order and truncated at `MAX_POINT_LIGHTS` / `MAX_SPOT_LIGHTS`. The
  /// spot lights must be gathered in the same order the shadow pass used, since a spot light's
  /// shadow map is found by its index rather than by any identity of its own.
  ///
  /// The last directional light in the scene wins, matching `GLRenderer::DrawMeshes`.
  auto BuildFrameConstants(Scene &, const Camera &, const bool, DXFrameConstants &) const -> void;
  /// @brief Uploads the frame constants and returns the address to bind, or zero on failure.
  auto UploadFrameConstants(const DXFrameConstants &) -> D3D12_GPU_VIRTUAL_ADDRESS;
  /// @brief Builds a material's contiguous run of texture views, filling unused slots with white.
  ///
  /// The views are created directly in the table rather than copied from the textures' own views,
  /// because a descriptor cannot be relocated once written and copying between shader-visible
  /// heaps is not allowed. Recreating a view is cheap; the resource behind it is shared either way.
  auto BuildMaterialTable(DXMaterial &, const std::array<const DXTexture *, MATERIAL_TEXTURE_SLOTS> &) -> void;
  /// @brief Copies the geometry of a batch into the frame's instance arena.
  ///
  /// @return Address of the batch's first instance, or zero when the arena could not be grown.
  ///
  /// Takes a span so a caller can upload the visible prefix of a batch without copying it out.
  auto AllocateInstances(std::span<const DXInstanceData>) -> D3D12_GPU_VIRTUAL_ADDRESS;
  /// @brief Grows the instance arena to hold at least the requested number of bytes.
  ///
  /// Growing discards the old buffer, which the GPU may still be reading from an earlier frame, so
  /// this waits for the GPU first. It happens when a scene's instance count reaches a new high and
  /// then stops, so the stall is a load-time cost rather than a per-frame one.
  auto EnsureInstanceCapacity(const uint64_t) -> bool;
  /// @brief Groups the scene's drawable meshes into one batch per mesh and material.
  ///
  /// Two entities can share a draw call only when they agree on both, since the mesh is bound as
  /// vertex buffers and the material as a descriptor table, neither of which can vary per instance.
  ///
  /// @param shadowCastersOnly Skips the material grouping, so the depth-only passes collapse every
  /// entity sharing a mesh into a single batch regardless of how it is shaded.
  /// @param camera Tested against each entity's bounds to order a batch's instances, visible first,
  /// and to set `visibleCount`. Null collects everything as visible, which is what a shadow pass
  /// wants: a caster outside the camera's view can still cast into it.
  ///
  /// @warning The result views storage this reuses on the next call, so only one collection may be
  /// live at a time. Every pass finishes with its batches before the next one collects.
  auto CollectBatches(Scene &, const bool, const Camera * = nullptr) -> std::span<const DXDrawBatch>;
  /// @brief Gathers the scene's skinned meshes, resolving each one's bone palette.
  ///
  /// A mesh's bones name nodes of a skeleton that lives on some ancestor entity rather than on the
  /// mesh itself, so the search walks up the hierarchy. Each bone's matrix is the node's world
  /// transform composed with the bone's offset, which is what takes a vertex from its bind pose
  /// into the posed skeleton.
  ///
  /// @param camera Drops meshes it cannot see. Safe to drop outright, unlike a rigid batch, because
  /// a skinned mesh is never placed in the acceleration structure. The bounds are the bind pose's,
  /// so a pose reaching well outside them can be culled early; `GLRenderer` accepts the same.
  auto CollectSkinnedDraws(Scene &, const Camera * = nullptr) -> std::vector<DXSkinnedDraw>;
  /// @brief Places the frame's batches into the acceleration structures a ray is traced against.
  ///
  /// Flattens the batches back into one placement each, because instancing is a property of the
  /// draw call and a ray has no draw call: every copy of a mesh has to exist in the structure
  /// separately, carrying its own transform.
  ///
  /// Must run before the pass binds any state. Building may wait on the GPU to grow a buffer, and
  /// the first build of a scene traces a validation grid that flushes the command list.
  auto BuildRayScene(const Camera &, std::span<const DXDrawBatch>) -> void;
  /// @brief Gathers the processor-side geometry the probe volume sorts triangles from.
  ///
  /// Separate from `CollectBatches` because it wants a different thing from the same entities: the
  /// batches carry uploaded buffers, and placing probes needs the vertices themselves. Yields spans
  /// into the assets rather than copies, so the result must not outlive the assets it points into.
  auto CollectProbeGeometry(Scene &) -> std::vector<DXProbeGeometry>;
  /// @brief Rebuilds the probe volume when the scene's geometry has changed, and audits it once.
  ///
  /// Must run before the pass binds any state: the build stages its buffers through the command
  /// list and flushes it, as does the audit.
  auto BuildProbeVolume(Scene &) -> void;
  /// @brief Points the scene pass at the probe volume, or at somewhere harmless when there is none.
  ///
  /// The three views are root descriptors rather than a table, so they have no fallback descriptor
  /// to fall back to and something valid has to be bound whether or not a volume exists. The shader
  /// reads none of them unless the frame constants say a volume is there, but a root signature slot
  /// left unset is undefined behaviour regardless of what the shader does with it.
  ///
  /// @param fallback Any live buffer address, bound in place of a volume that has not been built.
  auto BindProbeVolume(const D3D12_GPU_VIRTUAL_ADDRESS) -> void;
  /// @brief Draws every probe as a sphere shaded with the irradiance it stores.
  ///
  /// Not a pass of its own, and deliberately so. Drawn into the scene's own targets at the end of
  /// the scene pass, the spheres are depth-tested against the geometry around them, which is what
  /// makes the view worth having: a probe that ended up inside a wall disappears behind it, and a
  /// probe lighting a surface is seen from the same side that surface sees it. A separate target
  /// would have shown the field floating in isolation, with nothing to relate it to.
  ///
  /// One instanced draw of a unit sphere, one instance per probe, with the vertex shader reading
  /// positions out of the probe buffer directly. Nothing is read back and no per-probe transform
  /// is built, so the whole visualisation costs one draw call whatever the probe count.
  ///
  /// @param viewProjection The camera transform, in the same layout the frame constants carry.
  auto DrawProbes(const DXRenderTarget &, const float *) -> void;
  /// @brief Uploads the sphere the probes are drawn with, once.
  ///
  /// Its own mesh rather than the shared `Sphere` primitive, which subdivides for a surface a
  /// camera gets close to. A probe is a few pixels across and there are hundreds of them, so the
  /// count that matters here is the instance count, not the silhouette: an icosphere at
  /// `PROBE_DEBUG_SPHERE_LEVEL` is round enough at that size and keeps the whole visualisation
  /// cheaper than a single character mesh.
  auto EnsureProbeMesh() -> DXMesh *;
  /// @brief Creates the scene colour copy, or recreates it when the target's size or format moved.
  ///
  /// @return False when the copy could not be created, leaving transmission with nothing to read.
  auto EnsureSceneColor(const DXRenderTarget &) -> bool;
  /// @brief Resolves the colour target into that copy, mid-pass, and hands the target back.
  ///
  /// Runs between the opaque draws and the deferred ones, which is the only moment the copy is
  /// both complete and not yet needed. The target is transitioned out of `RENDER_TARGET` and back
  /// again around the resolve; the render target views stay bound throughout, since a barrier
  /// changes a resource's state and not what is bound to the pipeline.
  ///
  /// Every deferred draw reads the same copy, so a transmissive surface behind another one refracts
  /// what was there before either was drawn rather than the one in front of it.
  auto CaptureSceneColor(DXRenderTarget &) -> bool;
  /// @brief Copies a bone palette into the frame's instance arena, for the skinned vertex shader.
  auto AllocateBones(const std::vector<glm::mat4> &) -> D3D12_GPU_VIRTUAL_ADDRESS;
  /// @brief Creates a texture a compute shader writes, with a view of every mip it will write.
  ///
  /// @param arraySize Six for a cubemap, which also selects a cube view for sampling.
  auto CreateComputeTexture(DXComputeTexture &, const DXGI_FORMAT, const uint32_t, const uint32_t, const uint32_t, const std::string &) -> bool;
  auto ReleaseComputeTexture(DXComputeTexture &) -> void;
  /// @brief Points the compute root signature's tables at one dispatch's resources.
  ///
  /// Allocates a fresh block of descriptors each time rather than rewriting one block, because the
  /// dispatches are only recorded here: the GPU reads these descriptors later, so overwriting a
  /// block still referenced by an earlier dispatch would corrupt it. The blocks are released once
  /// the whole chain has been waited on.
  ///
  /// @param sources Views for `t0` to `t3`; null entries fall back to the stand-in resources.
  /// @param destination The mip this dispatch writes, as an index into the target's `mipUavIndices`.
  auto BindComputeResources(const std::array<uint32_t, COMPUTE_SRV_SLOTS> &, const uint32_t) -> bool;
  /// @brief Moves every array slice of one mip between resource states.
  ///
  /// The mip chain is built level by level, reading the level above while writing the level below,
  /// which are contradictory states for one resource. Barriers are per subresource for exactly this
  /// reason, so the levels can disagree while the build is in flight.
  auto TransitionMip(ID3D12Resource *, const uint32_t, const uint32_t, const uint32_t, const D3D12_RESOURCE_STATES, const D3D12_RESOURCE_STATES) -> void;
  /// @brief Builds the environment maps the ambient term samples, from a skybox texture.
  ///
  /// Runs the same sequence as the OpenGL backend: project the equirectangular source onto a cube,
  /// filter its mip chain, reduce it to nine spherical harmonic coefficients for the diffuse
  /// irradiance, prefilter it by roughness for the specular reflection, and integrate the
  /// split-sum lookup table.
  ///
  /// Dispatches are recorded and then waited on in one go, so this stalls the pipeline. It happens
  /// once per skybox texture, at load time.
  ///
  /// @return False when anything failed, leaving the ambient term on its analytic fallback.
  auto BuildEnvironmentMaps(const AssetID, const DXTexture &) -> bool;
  /// @brief Creates the one-texel cube and the coefficient buffer the compute tables always bind.
  auto EnsureComputeFallbacks() -> bool;
  /// @brief Renders an asset once into a small cached target, framed by its own bounds.
  ///
  /// Runs in the middle of the frame, from the editor's UI code rather than from the render graph,
  /// so it takes care to leave nothing behind: it binds its own target, draws, and transitions the
  /// result to a shader resource for the UI to sample in the same frame. Uploading the geometry may
  /// flush the command list, which is why it happens before any pipeline state is bound.
  ///
  /// The light is fixed rather than taken from the scene, so an asset looks the same in the browser
  /// no matter what the open scene happens to be lit by. This mirrors the OpenGL backend.
  ///
  /// @return The cached target, or null when the asset has nothing to draw.
  auto RenderPreview(const AssetID, const std::vector<std::pair<const DXMesh *, const DXMaterial *>> &, const BoundingBox &) -> RenderTarget *;
  /// @brief Resolves a standalone material asset to its shared texture table, uploading as needed.
  auto EnsurePreviewMaterial(const AssetID, const MaterialAsset &) -> const DXMaterial *;
  /// @brief Builds the view and projection one spot light's shadow map is rendered with.
  ///
  /// The cone's full angle is widened slightly so the shadow map covers a little more than the lit
  /// region, which keeps the cone's edge from sampling outside the map. Mirrors
  /// `GLRenderer::FitSpotShadowFrustum`, with a zero-to-one projection for Direct3D's clip range.
  auto FitSpotShadowFrustum(const Light &, glm::mat4 &, glm::mat4 &) const -> void;
  /// @brief Fills the target with the environment behind the scene, before any geometry is drawn.
  ///
  /// Drawn first with depth testing off rather than last against the far plane, which saves the
  /// scene pass from having to leave the depth buffer readable and costs one full-target overdraw.
  ///
  /// Samples the cubemap the compute path builds rather than the equirectangular source it was
  /// built from, so the background and the ambient light reflecting off the scene can only ever
  /// come from the same texels.
  ///
  /// Falls back the way `skybox.frag` does: the procedural gradient when the scene has a skybox
  /// entity but no environment map, and flat grey when it has no skybox entity at all.
  auto DrawSkybox(const Camera &, const DXRenderTarget &) -> void;
  /// @brief Builds the light-space view-projection the shadow pass renders with.
  ///
  /// Fits the frustum to the scene's world bounds the way `GLRenderer::FitShadowFrustum` does,
  /// so the map's texels are spent on geometry that exists rather than on a fixed extent that is
  /// either wasteful or clips the scene. Falls back to the light's own orthographic size when the
  /// scene has no bounded geometry.
  ///
  /// Uses a zero-to-one orthographic projection, because Direct3D clips depth to [0,1] where
  /// OpenGL uses [-1,1]. A GL-style matrix here would clip half the depth range.
  ///
  /// @return False when the scene has no directional light to cast from.
  auto BuildShadowMatrix(Scene &, float *) const -> bool;
};
} // namespace kuki
#endif
