---
name: add-graphics-setting
description: Checklist for adding a graphics setting to Kuki -- the EngineConfig field, its load and save in kuki.config.json, applying it to the renderer, its entry on the settings menu's graphics page, and the RendererCapabilities flag that hides it where the backend cannot act on it. Use whenever an effect becomes something a player can turn down or off.
---

# Add a graphics setting

A graphics setting says how this machine draws, not anything about a game, so it lives in
`EngineConfig` (`kuki.config.json`, beside the executable) and not in a game's `Preferences`.
An effect that is not cheap becomes one (the root `CLAUDE.md`'s "Performance before looks"):
measure it with `KUKI_PROFILE_SCOPE` and give the cost in the commit.

1. `engine/include/engine_config.hpp` -- the field on `EngineConfig`, defaulted to what a capable
   machine should get. A level is an enum (`LightingQuality`), a switch a `bool`.
2. `engine/src/engine_config.cpp` -- `EngineConfig::Load` reads it (a `bool` through the `Flag`
   lambda; an enum by name, so a level added between two others reads back as itself) and
   `EngineConfig::Save` writes it. A missing or malformed value keeps the default.
3. `engine/src/settings_menu.cpp`:
   - `SettingsMenu::ApplyGraphics` puts it into effect -- usually
     `rendering->SetPassEnabled(RenderPass::X, graphics.field)` (a disabled pass is stood down to
     its bypass, not taken out of the graph), or a setter on `RenderingSystem`.
   - `SettingsMenu::SaveGraphics` copies it into the config it reads back, or it is never saved:
     only the fields listed there are written, so the editor's backend and tone mapper survive.
   - `SettingsMenu::AddGraphicsEntries` adds its row: `Flag("Label", &EngineConfig::field)` for a
     switch, `AddChoice` for a level, in the order the page should read.
4. If not every backend can act on it, a flag in `RendererCapabilities`
   (`renderer_capabilities.hpp`, false by default), set in `DXRenderer::GetCapabilities`, and
   both the `ApplyGraphics` call and the entry behind `capabilities.<flag>` -- as `volumetrics` is.
   A control that does nothing is worse than none.
5. A new pass behind it follows the `add-render-pass` skill.

Then open the page with `--settings graphics` and the `capture` skill, turn it off and on, and
check `kuki.config.json` beside the executable holds it after a restart.
