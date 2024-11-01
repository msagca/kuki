#include "../common/camera.hpp"
#include "../common/shader.hpp"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/detail/func_trigonometric.inl>
#include <glm/ext/matrix_clip_space.inl>
#include <glm/ext/matrix_transform.inl>
#include <iostream>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
const unsigned int WINDOW_HEIGHT = 600;
const unsigned int WINDOW_WIDTH = 800;
const auto NEAR_PLANE = .1f;
const auto FAR_PLANE = 100.0f;
const auto VIEW_ANGLE = 45.0f;
const auto MOUSE_SENSITIVITY = 0.1f;
auto deltaTime = .0f;
auto mouseFirstEnter = true;
auto xPosLast = (float)WINDOW_WIDTH / 2;
auto yPosLast = (float)WINDOW_HEIGHT / 2;
Camera camera(glm::vec3(.0f, .0f, 3.0f));
static void FramebufferSizeCallback(GLFWwindow*, int, int);
static void MouseCallback(GLFWwindow*, double, double);
static void ProcessInput(GLFWwindow*);
int main() {
  glfwInit();
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  auto window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Getting Started", NULL, NULL);
  if (window == NULL)
    std::cout << "Error: Failed to create GLFW window." << std::endl;
  glfwMakeContextCurrent(window);
  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    std::cout << "Error: Failed to initialize GLAD." << std::endl;
  glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
  glfwSetFramebufferSizeCallback(window, FramebufferSizeCallback);
  Shader objectShader("vertShader.glsl", "fragShaderObject.glsl");
  Shader lightShader("vertShader.glsl", "fragShaderLight.glsl");
  //
  //              0-----1
  //              |     |
  //              |     |
  //  1-----0-----4-----5-----1
  //  |     |     |     |     |
  //  |     |     |     |     |
  //  2-----3-----7-----6-----2
  //              |     |
  //              |     |
  //              3-----2
  //
  float vertices[] = {-.5f, -.5f, -.5f, .0f, .0f, -1.0f, .5f, -.5f, -.5f, .0f, .0f, -1.0f, .5f, .5f, -.5f, .0f, .0f, -1.0f, .5f, .5f, -.5f, .0f, .0f, -1.0f, -.5f, .5f, -.5f, .0f, .0f, -1.0f, -.5f, -.5f, -.5f, .0f, .0f, -1.0f, -.5f, -.5f, .5f, .0f, .0f, 1.0f, .5f, -.5f, .5f, .0f, .0f, 1.0f, .5f, .5f, .5f, .0f, .0f, 1.0f, .5f, .5f, .5f, .0f, .0f, 1.0f, -.5f, .5f, .5f, .0f, .0f, 1.0f, -.5f, -.5f, .5f, .0f, .0f, 1.0f, -.5f, .5f, .5f, -1.0f, .0f, .0f, -.5f, .5f, -.5f, -1.0f, .0f, .0f, -.5f, -.5f, -.5f, -1.0f, .0f, .0f, -.5f, -.5f, -.5f, -1.0f, .0f, .0f, -.5f, -.5f, .5f, -1.0f, .0f, .0f, -.5f, .5f, .5f, -1.0f, .0f, .0f, .5f, .5f, .5f, 1.0f, .0f, .0f, .5f, .5f, -.5f, 1.0f, .0f, .0f, .5f, -.5f, -.5f, 1.0f, .0f, .0f, .5f, -.5f, -.5f, 1.0f, .0f, .0f, .5f, -.5f, .5f, 1.0f, .0f, .0f, .5f, .5f, .5f, 1.0f, .0f, .0f, -.5f, -.5f, -.5f, .0f, -1.0f, .0f, .5f, -.5f, -.5f, .0f, -1.0f, .0f, .5f, -.5f, .5f, .0f, -1.0f, .0f, .5f, -.5f, .5f, .0f, -1.0f, .0f, -.5f, -.5f, .5f, .0f, -1.0f, .0f, -.5f, -.5f, -.5f, .0f, -1.0f, .0f, -.5f, .5f, -.5f, .0f, 1.0f, .0f, .5f, .5f, -.5f, .0f, 1.0f, .0f, .5f, .5f, .5f, .0f, 1.0f, .0f, .5f, .5f, .5f, .0f, 1.0f, .0f, -.5f, .5f, .5f, .0f, 1.0f, .0f, -.5f, .5f, -.5f, .0f, 1.0f, .0f};
  unsigned int objectVAO, lightVAO, VBO;
  glGenVertexArrays(1, &objectVAO);
  glGenVertexArrays(1, &lightVAO);
  glGenBuffers(1, &VBO);
  // set up object VAO
  glBindVertexArray(objectVAO);
  glBindBuffer(GL_ARRAY_BUFFER, VBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
  objectShader.Use();
  auto posAttr = glGetAttribLocation(objectShader.ID, "posIn");
  auto normAttr = glGetAttribLocation(objectShader.ID, "normIn");
  glVertexAttribPointer(posAttr, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
  glEnableVertexAttribArray(posAttr);
  glVertexAttribPointer(normAttr, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
  glEnableVertexAttribArray(normAttr);
  // set up light VAO
  glBindVertexArray(lightVAO);
  glBindBuffer(GL_ARRAY_BUFFER, VBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
  lightShader.Use();
  posAttr = glGetAttribLocation(lightShader.ID, "posIn");
  glVertexAttribPointer(posAttr, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
  glEnableVertexAttribArray(posAttr);
  glEnable(GL_DEPTH_TEST);
  glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
  glfwSetCursorPosCallback(window, MouseCallback);
  auto model = glm::mat4(1.0f);
  auto view = camera.GetViewMatrix();
  auto projection = glm::perspective(glm::radians(VIEW_ANGLE), (float)WINDOW_WIDTH / WINDOW_HEIGHT, NEAR_PLANE, FAR_PLANE);
  auto lightColor = glm::vec3(1.0f, 1.0f, 1.0f);
  auto objectColor = glm::vec3(1.0f, .5f, .31f);
  objectShader.Use();
  objectShader.SetMat4("model", model);
  objectShader.SetMat4("view", view);
  objectShader.SetMat4("projection", projection);
  objectShader.SetVec3("lightColor", lightColor);
  objectShader.SetVec3("objectColor", objectColor);
  auto lightPos = glm::vec3(1.2f, 1.0f, 2.0f);
  objectShader.SetVec3("lightPos", lightPos);
  auto lightScale = glm::vec3(.2f);
  model = glm::translate(model, lightPos);
  model = glm::scale(model, lightScale);
  lightShader.Use();
  lightShader.SetMat4("model", model);
  lightShader.SetMat4("view", view);
  lightShader.SetMat4("projection", projection);
  lightShader.SetVec3("viewPos", camera.position);
  glClearColor(.1f, .1f, .1f, 1.0f);
  auto timeLast = .0f;
  while (!glfwWindowShouldClose(window)) {
    auto timeNow = glfwGetTime();
    deltaTime = timeNow - timeLast;
    timeLast = timeNow;
    ProcessInput(window);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    view = camera.GetViewMatrix();
    auto lightX = cos(timeNow) * 5;
    auto lightY = sin(timeNow) * 5;
    auto lightPosNew = lightPos + glm::vec3(lightX, lightY, 0);
    objectShader.Use();
    objectShader.SetMat4("view", view);
    objectShader.SetVec3("lightPos", lightPosNew);
    glBindVertexArray(objectVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    auto lightModel = glm::translate(model, lightPosNew);
    lightShader.Use();
    lightShader.SetMat4("model", lightModel);
    lightShader.SetMat4("view", view);
    lightShader.SetVec3("viewPos", camera.position);
    glBindVertexArray(lightVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glfwSwapBuffers(window);
    glfwPollEvents();
  }
  glDeleteVertexArrays(1, &objectVAO);
  glDeleteVertexArrays(1, &lightVAO);
  glDeleteBuffers(1, &VBO);
  glDeleteProgram(objectShader.ID);
  glDeleteProgram(lightShader.ID);
  glfwTerminate();
  return 0;
}
static void FramebufferSizeCallback(GLFWwindow* window, int width, int height) {
  glViewport(0, 0, width, height);
}
static void MouseCallback(GLFWwindow* window, double xPos, double yPos) {
  if (mouseFirstEnter) {
    xPosLast = xPos;
    yPosLast = yPos;
    mouseFirstEnter = false;
  }
  auto xOffset = xPos - xPosLast;
  auto yOffset = yPosLast - yPos;
  xPosLast = xPos;
  yPosLast = yPos;
  xOffset *= MOUSE_SENSITIVITY;
  yOffset *= MOUSE_SENSITIVITY;
  camera.ProcessMouse(xOffset, yOffset);
}
static void ProcessInput(GLFWwindow* window) {
  if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
    glfwSetWindowShouldClose(window, true);
  uint8_t wasd = 0;
  if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
    wasd |= 1;
  if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
    wasd |= 2;
  if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
    wasd |= 4;
  if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
    wasd |= 8;
  camera.ProcessKeyboard(wasd, deltaTime);
}
