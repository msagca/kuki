#pragma once
/// Every macro the engine defines for its own use or a game's, in one place.
///
/// A macro is a name the compiler never sees, so it cannot be found by looking for a declaration,
/// and one defined in the middle of an unrelated header is one nobody knows is there. Kept together
/// they can be read at once, and a name chosen for a new one can be checked against the others.
///
/// This header includes nothing. A macro here expands to names declared elsewhere -- a trait, a
/// struct, a class -- and is used where those are in scope: the header each belongs with includes
/// this one, so including that header is all a user of the macro ever does. Which header that is
/// is said beside each.
///
/// What is deliberately not here:
///
/// - Configuration of the libraries the engine uses (`GLM_ENABLE_EXPERIMENTAL`, `GLFW_INCLUDE_NONE`,
///   `NOMINMAX`, `WIN32_LEAN_AND_MEAN`), which has to be seen before those libraries' headers and
///   so is given to the compiler by `engine/CMakeLists.txt` rather than written in any file.
/// - The single-header libraries' `..._IMPLEMENTATION` switches, each of which must be defined in
///   exactly one source file and is the only reason that file exists (`stb_image.cpp` and the like).
/// - What a shader is compiled with, which is `SHADER_DEFINITIONS` (`shader_definitions.hpp`).
/// - `KUKI_ENGINE_API`, which CMake generates into `kuki_engine_export.h`.
/// - The editor's own (`KUKI_FIELD`, and the icon font's generated names).
// --- Game modules (`game_module.hpp`)
#if defined(_WIN32)
#define KUKI_MODULE_EXPORT __declspec(dllexport)
#else
#define KUKI_MODULE_EXPORT __attribute__((visibility("default")))
#endif
/// @brief Declares a library to be a game. Once, in one source file of the game.
#define KUKI_GAME_MODULE(Name, Setup) \
  extern "C" KUKI_MODULE_EXPORT auto KukiGameModule() -> const ::kuki::GameModule * { \
    static const ::kuki::GameModule module{Name, Setup}; \
    return &module; \
  }
/// @brief Gives a game's own executable something to call, so its `main` needs no header of the game's.
///
/// `App` is the game's application class. It stays inside the library, where nothing has to be
/// exported for it; the executable sees one C function.
#define KUKI_GAME_MAIN(App) \
  extern "C" KUKI_MODULE_EXPORT auto KukiGameRun() -> int { \
    App app; \
    app.Run(); \
    return 0; \
  }
// --- Components (`component_traits.hpp`)
/// @brief Declares one of a game's types to be a component, with its place among the game's.
///
/// At namespace scope, once, in a header every user of the type includes. `Index` counts from 0 and
/// is the game's to hand out; two of a game's components given the same one are caught where its
/// `World` is declared, which is a compile error naming both.
#define KUKI_COMPONENT(Type, Index) \
  template <> \
  struct kuki::ComponentTraits<Type> { \
    static constexpr ::kuki::ComponentId id = ::kuki::FIRST_GAME_COMPONENT + (Index); \
    static constexpr std::string_view name = #Type; \
    static_assert((Index) >= 0 && ::kuki::FIRST_GAME_COMPONENT + (Index) < ::kuki::MAX_COMPONENTS, "a game component's index is out of range"); \
  }
/// @brief Declares one of the engine's own components, ahead of its definition, with the id its
/// `ComponentType` enumerator gives it. Inside `namespace kuki`, and for the engine alone: a game's
/// are `KUKI_COMPONENT`.
#define KUKI_ENGINE_COMPONENT(Type) \
  struct Type; \
  template <> \
  struct ComponentTraits<Type> { \
    static constexpr ComponentId id = static_cast<ComponentId>(ComponentType::Type); \
    static constexpr std::string_view name = #Type; \
  };
// --- Profiling (`profiler.hpp`)
#ifdef KUKI_DISABLE_PROFILING
#define KUKI_PROFILE_SCOPE(name) ((void)0)
#define KUKI_PROFILE_FUNCTION() ((void)0)
#define KUKI_PROFILE_MARK(name) ((void)0)
#else
#define KUKI_PROFILE_CONCAT_IMPL(a, b) a##b
#define KUKI_PROFILE_CONCAT(a, b) KUKI_PROFILE_CONCAT_IMPL(a, b)
/// @brief Times the enclosing block under `name`, which must outlive the block if it is a pointer.
#define KUKI_PROFILE_SCOPE(name) const kuki::ProfileScope KUKI_PROFILE_CONCAT(profileScope, __LINE__)(name)
#define KUKI_PROFILE_FUNCTION() KUKI_PROFILE_SCOPE(__func__)
#define KUKI_PROFILE_MARK(name) kuki::Profiler::Get().Mark(name)
#endif
