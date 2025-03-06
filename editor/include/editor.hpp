#pragma once
#include <application.hpp>
#include <camera_controller.hpp>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imfilebrowser.h>
#include <utility/pool.hpp>
class Editor : public Application {
private:
  GLFWwindow* window = nullptr;
  CameraController cameraController;
  ImGui::FileBrowser fileBrowser;
  InputManager inputManager;
  Pool<unsigned int> texturePool;
  int clickedEntity = -1;
  int selectedEntity = -1;
  int entityToDelete = -1;
  bool updateThumbnails = true;
  static unsigned int CreateTexture();
  static void DeleteTexture(unsigned int);
  static void CursorPosCallback(GLFWwindow*, double, double);
  static void FramebufferSizeCallback(GLFWwindow*, int, int);
  static void KeyCallback(GLFWwindow*, int, int, int, int);
  static void MouseButtonCallback(GLFWwindow*, int, int, int);
  static void WindowCloseCallback(GLFWwindow*);
  bool Status() override;
  void DisplayEntity(unsigned int, EntityManager&);
  void DisplayHierarchy();
  void DisplayProperties();
  void DisplayAssets();
  void DisplayScene();
  void DrawGrid();
  void DrawManipulator();
  void InitImGui();
  void InitLayout();
  void InitOpenGL(int, int);
  void LoadDefaultAssets();
  void LoadDefaultScene();
  void SetWindowIcon(const char*);
  void Shutdown() override;
  void Start() override;
  void Update() override;
  void UpdateThumbnails();
  void UpdateView();
public:
  Editor();
};
