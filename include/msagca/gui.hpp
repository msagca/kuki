#pragma once
#include <entity_manager.hpp>
#include <input_manager.hpp>
extern bool showCreateMenu;
extern bool showHierarchyWindow;
struct Hint {
  std::string text;
  std::function<bool()> condition;
};
static const std::vector<Hint> hints = {
  {"Hold down the right mouse button and use the WASD keys to fly around.", []() { return true; }},
  {"Press [Space] to open/close the create menu.", []() { return !showCreateMenu; }},
  {"Press [H] to show/hide the hierarchy window.", []() { return !showHierarchyWindow; }},
  {"Press [V] to change the view mode.", []() { return true; }}};
void ShowHints(float, float);
void ShowCreateMenu(EntityManager&);
void ShowHierarchyWindow(EntityManager&, InputManager&);
