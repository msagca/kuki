#pragma once
#include "asset_loader.hpp"
#include "asset_manager.hpp"
#include "camera_controller.hpp"
#include "entity_manager.hpp"
#include "imgui_impl_glfw.h"
#include "render_system.hpp"
class Editor {
private:
  AssetManager& assetManager;
  AssetLoader& assetLoader;
  EntityManager& entityManager;
  CameraController& cameraController;
  RenderSystem& renderSystem;
  int selectedEntity = -1;
  void LoadDefaultScene();
  void DisplayGame();
  void DisplayHierarchy();
  void DisplayEntityHierarchy(unsigned int, int&, int&);
  void DisplayProperties(unsigned int);
  void DisplayCreateMenu();
public:
  Editor(AssetManager&, AssetLoader&, EntityManager&, CameraController&, RenderSystem&);
  void Init(GLFWwindow*);
  void Render();
  void CleanUp();
};
