#include <glm/gtc/type_ptr.hpp>
#include <mesh.hpp>
#include <glad/glad.h>
#include <utility.hpp>
#include <iostream>
#include <imgui.h>
#include <camera.hpp>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
static const auto MOUSE_SENSITIVITY = 0.1f;
static auto mousePosXLast = static_cast<float>(WINDOW_WIDTH) / 2;
static auto mousePosYLast = static_cast<float>(WINDOW_HEIGHT) / 2;
static auto showCreateMenu = false;
static auto mouseFirstEnter = true;
static auto mouseRightClicked = false;
static auto spaceKeyPressed = false;
void ProcessInput(GLFWwindow* window) {
  uint8_t wasd = 0;
  if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
    wasd |= 1;
  if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
    wasd |= 2;
  if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
    wasd |= 4;
  if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
    wasd |= 8;
  auto shift = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
  if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
    if (!spaceKeyPressed) {
      showCreateMenu = !showCreateMenu;
      spaceKeyPressed = true;
    }
  } else
    spaceKeyPressed = false;
  if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
    showCreateMenu = false;
  if (cameraPtr)
    cameraPtr->ProcessKeyboard(wasd, shift, deltaTime);
}
void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
  if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS) {
    mouseRightClicked = true;
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
  }
  if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_RELEASE) {
    mouseRightClicked = false;
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    mouseFirstEnter = true;
  }
}
void MousePosCallback(GLFWwindow* window, double xPos, double yPos) {
  if (!mouseRightClicked)
    return;
  if (mouseFirstEnter) {
    mousePosXLast = xPos;
    mousePosYLast = yPos;
    mouseFirstEnter = false;
  }
  auto xOffset = xPos - mousePosXLast;
  auto yOffset = mousePosYLast - yPos;
  mousePosXLast = xPos;
  mousePosYLast = yPos;
  xOffset *= MOUSE_SENSITIVITY;
  yOffset *= MOUSE_SENSITIVITY;
  if (cameraPtr)
    cameraPtr->ProcessMouse(xOffset, yOffset);
}
void WindowCloseCallback(GLFWwindow* window) {
  glfwSetWindowShouldClose(window, GLFW_TRUE);
}
void FramebufferSizeCallback(GLFWwindow* window, int width, int height) {
  glViewport(0, 0, width, height);
  auto ratio = static_cast<float>(width) / height;
  if (renderSystemPtr)
    renderSystemPtr->SetAspectRatio(ratio);
}
unsigned int LoadTexture(char const* path) {
  unsigned int textureID;
  glGenTextures(1, &textureID);
  int width, height, nrComponents;
  auto data = stbi_load(path, &width, &height, &nrComponents, 0);
  if (data) {
    GLenum format = 0;
    if (nrComponents == 1)
      format = GL_RED;
    else if (nrComponents == 3)
      format = GL_RGB;
    else if (nrComponents == 4)
      format = GL_RGBA;
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  } else
    std::cerr << "Failed to load the texture at " << path << "." << std::endl;
  stbi_image_free(data);
  return textureID;
}
void SetWindowIcon(GLFWwindow* window, const char* iconPath) {
  int width, height, channels;
  unsigned char* data = stbi_load(iconPath, &width, &height, &channels, 4);
  if (data) {
    GLFWimage images[1]{};
    images[0].width = width;
    images[0].height = height;
    images[0].pixels = data;
    glfwSetWindowIcon(window, 1, images);
    stbi_image_free(data);
  } else
    std::cerr << "Error: Failed to load the icon at " << iconPath << "." << std::endl;
}
void ShowCreateMenu(EntityManager& entityManager) {
  if (!showCreateMenu)
    return;
  static const char* primitives[] = {"Cube"};
  static int selectedPrimitive = -1;
  static auto position = glm::vec3(.0f, .0f, .0f);
  static auto rotation = glm::vec3(.0f, .0f, .0f);
  static auto scale = glm::vec3(1.0f, 1.0f, 1.0f);
  static bool showProperties = false;
  ImGui::Begin("Create Primitive");
  if (ImGui::ListBox("Primitives", &selectedPrimitive, primitives, IM_ARRAYSIZE(primitives)))
    showProperties = true;
  if (showProperties && selectedPrimitive != -1) {
    ImGui::InputFloat3("Position", glm::value_ptr(position));
    ImGui::InputFloat3("Rotation", glm::value_ptr(rotation));
    ImGui::InputFloat3("Scale", glm::value_ptr(scale));
    if (ImGui::Button("Confirm")) {
      auto id = entityManager.CreateEntity();
      std::vector<float> vertices;
      std::vector<unsigned int> indices;
      if (strcmp(primitives[selectedPrimitive], "Cube") == 0) {
        vertices = {-.5f, -.5f, -.5f, .5f, -.5f, -.5f, .5f, .5f, -.5f, -.5f, .5f, -.5f, -.5f, -.5f, .5f, .5f, -.5f, .5f, .5f, .5f, .5f, -.5f, .5f, .5f};
        indices = {0, 1, 2, 2, 3, 0, 4, 5, 6, 6, 7, 4, 4, 5, 1, 1, 0, 4, 7, 6, 2, 2, 3, 7, 4, 0, 3, 3, 7, 4, 5, 1, 2, 2, 6, 5};
      }
      auto mesh = CreateMesh(vertices, indices);
      auto& filter = entityManager.AddComponent<MeshFilter>(id);
      filter = mesh;
      entityManager.AddComponent<MeshRenderer>(id);
      auto& transform = entityManager.AddComponent<Transform>(id);
      transform.position = position;
      transform.rotation = rotation;
      transform.scale = scale;
      selectedPrimitive = -1;
      showProperties = false;
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      selectedPrimitive = -1;
      showProperties = false;
    }
  }
  ImGui::End();
}
