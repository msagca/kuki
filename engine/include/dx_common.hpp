#pragma once
#ifdef KUKI_HAS_DIRECTX
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
/// @file
/// @brief Single entry point for every Direct3D 12 header, because inclusion order is load-bearing.
///
/// The `d3d12.h` below is DirectX-Headers' own rather than the Windows SDK's, and deliberately so:
/// the `CD3DX12_*` helpers are written against the submodule's headers and reach for symbols that
/// the installed SDK will not have until it catches up, so letting the SDK's `__d3d12_h__` guard win
/// leaves those helpers referring to declarations nobody made. Order is still load-bearing, for the
/// mirror of the old reason: nothing may reach the SDK's `d3d12.h` first, or its guard shuts the
/// submodule's copy out and two translation units end up disagreeing about the same types. Always
/// include this header, never the raw ones.
///
/// Building against headers newer than the runtime holds only while the engine keeps to APIs the
/// installed runtime already has. Anything the submodule adds ahead of the SDK needs the Agility SDK
/// redistributable staged beside the executable before it may be called.
///
/// The `#undef`s come last, and must: they clear legacy macros the Windows headers leak, but the
/// SDK headers themselves still write `far` and `near` textually and rely on those macros
/// expanding to nothing, so undefining them any earlier is a syntax error inside `d3d12.h`.
/// `near`/`far` collide with `Frustum`'s plane members; `CreateWindow`, `LoadImage` and `GetObject`
/// are function-like macros that collide with engine method names.
#include <cstdint>
#include <directx/d3d12.h>
#include <directx/d3dx12.h>
#include <dxgi1_6.h>
#include <kuki_engine_export.h>
#include <string>
#include <windows.h>
#include <wrl/client.h>
#undef near
#undef far
#undef CreateWindow
#undef LoadImage
#undef GetObject
namespace kuki {
template <typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;
/// @brief Number of frames the CPU is allowed to run ahead of the GPU. Sizes every per-frame pool.
inline constexpr uint32_t DX_FRAME_COUNT = 3;
/// @brief Logs a failed HRESULT with context and returns whether it failed.
/// @return True when the call failed, so call sites can read as `if (DXFailed(hr, "..."))`.
auto DXFailed(const HRESULT, const std::string &) -> bool;
/// @brief Formats an HRESULT as an 0x-prefixed code plus the system message, for logging.
auto DXResultToString(const HRESULT) -> std::string;
/// @brief Whether an HRESULT means the device is gone rather than that one call went wrong.
///
/// Worth telling apart because nothing after it can succeed: once the device is removed every
/// subsequent call fails with the same code, so a log fills with consequences of one cause. The
/// first one to notice is the one worth asking `GetDeviceRemovedReason`.
auto DXDeviceIsGone(const HRESULT) -> bool;
/// @brief Logs why the device was removed, and the GPU's own account of it where that is enabled.
///
/// `GetDeviceRemovedReason` gives the class of failure -- a hang, a page fault, a driver upgrade
/// underneath a running process. That is usually enough to say whose problem it is, and it costs
/// nothing to ask, so it is asked whenever a call fails with a device-removed code.
///
/// The GPU's own account -- which draw was in flight, what address faulted -- comes from Device
/// Removed Extended Data, and that has to be switched on before the device exists. See
/// `DXEnableDeviceRemovedDiagnostics`.
auto DXReportDeviceRemoved(ID3D12Device *, const std::string &) -> void;
/// @brief Turns on Device Removed Extended Data, if this run has asked for it.
///
/// Off unless `KUKI_DX_DIAGNOSTICS` is set in the environment, because auto-breadcrumbs make the
/// driver record every draw and that is not a cost to pay on every run of every game. Set it when
/// chasing a device removal and the next one names the draw it died on.
///
/// Must be called before the device is created; there is no way to switch it on afterwards, which
/// is the whole difficulty with diagnosing this class of bug and the reason it is a switch rather
/// than something the crash handler can turn on for itself.
auto DXEnableDeviceRemovedDiagnostics() -> bool;
/// @brief What the adapter and its driver can do beyond the feature level the device was created at.
///
/// A Direct3D 12 device is created at a feature level and then interrogated for everything else,
/// because the optional features are versioned independently of it. Raytracing is the reason this
/// exists: the passes that need it must be skipped rather than attempted on hardware without it,
/// and skipping is only possible if the answer is known before a pipeline is built.
struct KUKI_ENGINE_API DXCapabilities {
  D3D_SHADER_MODEL shaderModel{D3D_SHADER_MODEL_5_1};
  D3D12_RAYTRACING_TIER raytracingTier{D3D12_RAYTRACING_TIER_NOT_SUPPORTED};
  D3D12_RESOURCE_BINDING_TIER bindingTier{D3D12_RESOURCE_BINDING_TIER_1};
  /// @brief Whether a shader may trace rays inline, through `RayQuery` rather than a separate pass.
  ///
  /// Wants tier 1.1 rather than 1.0: tier 1.0 traces only from a raytracing pipeline, whose shader
  /// tables and dispatch are a second way to organise a pass. Inline tracing keeps the work inside
  /// the compute shaders the engine already dispatches. Shader Model 6.5 is where `RayQuery` was
  /// added to the language, so the driver has to agree on both counts.
  auto SupportsInlineRaytracing() const -> bool;
  /// @brief Whether a shader may index an unbounded array of resources, choosing one per lane.
  ///
  /// Resource binding tier 3 is the entire requirement. A tier 3 device puts no size limit on a
  /// descriptor table, so a range declared unbounded resolves against a heap as large as memory
  /// allows, and `NonUniformResourceIndex` lets each lane in a wave pick a different entry. Every
  /// Shader Model 6 profile can express both.
  ///
  /// This is what a traced ray needs. A hit reports an instance and a primitive and nothing else,
  /// so the shader has to reach the geometry and the material of whatever it happened to find,
  /// which is not knowable when the dispatch is recorded.
  ///
  /// Not to be confused with `SupportsDynamicResources`, a later syntax for the same hardware
  /// capability rather than a capability of its own.
  auto SupportsBindless() const -> bool;
  /// @brief Whether a shader may reach the heap through `ResourceDescriptorHeap`, with no table.
  ///
  /// An ergonomic gain over `SupportsBindless` and nothing more: one global index rather than a
  /// root signature table plus an offset within it. Windows 10 ships no runtime that reports
  /// Shader Model 6.6, so this is false there whatever the adapter is capable of, and nothing
  /// should be built on it that an unbounded table could carry instead.
  auto SupportsDynamicResources() const -> bool;
};
/// @brief Asks the device what it supports. Cheap, but the result is meant to be queried once.
auto QueryDXCapabilities(ID3D12Device *) -> DXCapabilities;
/// @brief Picks the first hardware adapter that gives a feature level 11_0 device, most capable
/// first, skipping the software renderer.
///
/// The choice is written once because it is asked twice, for different reasons. The context asks
/// while bringing the backend up and keeps what it gets. `DXDeviceAvailable` asks before there is a
/// backend at all, to find out whether choosing this one would work, and throws the device away. If
/// those two ever disagreed the second would be answering about a device the first would not have
/// picked, which is the one thing a probe must not do.
///
/// The software adapter is skipped deliberately. WARP will create a device on a machine with no
/// usable GPU at all, so accepting it would have this report Direct3D 12 as available everywhere
/// and default a headless build onto a software rasteriser.
///
/// @return False when no adapter provides a device, leaving both arguments empty.
auto SelectDXDevice(IDXGIFactory6 *, ComPtr<IDXGIAdapter1> &, ComPtr<ID3D12Device> &) -> bool;
/// @brief Whether this machine can actually run the Direct3D 12 backend, asked of the driver.
///
/// Compiling the backend in and being able to run it are different questions, and only the first
/// has a compile-time answer. `KUKI_HAS_DIRECTX` says a Windows build with the SDK headers present
/// produced these files; it says nothing about the machine the executable was then copied to, which
/// may have a GPU too old for feature level 11_0, no D3D12 runtime, or no usable adapter at all.
/// Answering the second question means asking the driver, which means creating a device.
///
/// Cached after the first call. Creating a throwaway device costs a few milliseconds, and the
/// graphics panel asks this once per API per frame while its menu is open.
auto DXDeviceAvailable() -> bool;
/// @brief Formats a shader model as its `6_6` form, for logging.
auto DXShaderModelToString(const D3D_SHADER_MODEL) -> std::string;
} // namespace kuki
#endif
