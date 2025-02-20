#pragma once
#include "pool.hpp"
#include <application.hpp>
#include <camera_controller.hpp>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imfilebrowser.h>
class Editor : public Application {
private:
  InputManager inputManager;
  CameraController cameraController;
  GLFWwindow* window = nullptr;
  int selectedEntity = -1;
  bool updateThumbnails = true;
  ImGui::FileBrowser fileDialog;
  Pool<unsigned int> texturePool;
  void InitOpenGL(int, int);
  void InitImGui();
  void SetWindowIcon(const char*);
  void LoadDefaultScene();
  void LoadDefaultAssets();
  void DisplayScene();
  void DisplayHierarchy();
  void DisplayEntityHierarchy(unsigned int, int&, int&);
  void DisplayProperties(unsigned int);
  void DisplayResources();
  void DisplayCreateMenu();
  void Start() override;
  bool Status() override;
  void Update() override;
  void UpdateView();
  void Shutdown() override;
  void AssetAdditionHandler();
  static unsigned int CreateTexture();
  static void DeleteTexture(unsigned int);
  static void WindowCloseCallback(GLFWwindow*);
  static void FramebufferSizeCallback(GLFWwindow*, int, int);
  static void KeyCallback(GLFWwindow*, int, int, int, int);
  static void MouseButtonCallback(GLFWwindow*, int, int, int);
  static void CursorPosCallback(GLFWwindow*, double, double);
public:
  Editor();
};
