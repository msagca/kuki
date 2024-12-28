#pragma once
#include <asset_manager.hpp>
#include <entity_manager.hpp>
#include <input_manager.hpp>
extern bool showCreateMenu;
extern bool showHierarchyWindow;
extern bool showFPS;
struct Hint {
  std::string text;
  std::function<bool()> condition;
};
static const std::vector<Hint> hints = {
  {"Hold down the right mouse button and use the WASD keys to fly around.", []() { return true; }},
  {"Press [Space] to open/close the create menu.", []() { return !showCreateMenu; }},
  {"Press [H] to show/hide the hierarchy window.", []() { return !showHierarchyWindow; }},
  {"Press [R] to change the render mode.", []() { return true; }},
  {"Press [F] to display the FPS counter.", []() { return !showFPS; }}};
void ShowHints(float, float);
void ShowFPS(unsigned int, float);
void ShowCreateMenu(EntityManager&, AssetManager&);
void ShowHierarchyWindow(EntityManager&, InputManager&);
