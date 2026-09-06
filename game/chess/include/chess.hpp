#pragma once
#include <application_description.hpp>
#include <rendering_system.hpp>
#include <scripting_system.hpp>
#include <system_application.hpp>
/// @brief The typeface the game sets all of its text in, staged beside the binary.
///
/// One font for the clocks and the board's labels alike. `Application::SetOverlayFont` bakes it
/// and registers the atlas, and `GameManager` reads that same atlas back rather than baking its
/// own -- a second bake of the same file would be a second four-megabyte texture saying exactly
/// what the first one says.
inline constexpr const char *CHESS_FONT = "font/Inter-VariableFont_opsz,wght.ttf";
/// @brief The chess application: a window, a renderer, scripts, and nothing else.
///
/// `SystemApplication` takes the systems as template parameters, so the ones a game does not use
/// cost it nothing. There is no `AnimationSystem` and no `PhysicsSystem` here: chess has neither
/// skinned meshes nor anything that falls, and the editor's list is not the list every application
/// wants. `RenderingSystem` is the one that has to be present -- `Application`'s own startup
/// depends on it, and leaving it out is a `static_assert` rather than a crash.
class Chess final : public kuki::SystemApplication<kuki::ScriptingSystem, kuki::RenderingSystem> {
public:
  Chess();
private:
  auto Start() -> void override;
};
