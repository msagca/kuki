#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <iostream>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <camera.hpp>
#include <shader.hpp>
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
static unsigned int LoadTexture(const char*);
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
  Shader objectShader("object-vert.glsl", "object-frag.glsl");
  Shader lightShader("light-vert.glsl", "light-frag.glsl");
  auto diffuseMap = LoadTexture("container.png");
  auto specularMap = LoadTexture("container-specular.png");
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
  float vertices[] = {-.5f, -.5f, -.5f, .0f, .0f, -1.0f, .0f, .0f, .5f, -.5f, -.5f, .0f, .0f, -1.0f, 1.0f, .0f, .5f, .5f, -.5f, .0f, .0f, -1.0f, 1.0f, 1.0f, .5f, .5f, -.5f, .0f, .0f, -1.0f, 1.0f, 1.0f, -.5f, .5f, -.5f, .0f, .0f, -1.0f, .0f, 1.0f, -.5f, -.5f, -.5f, .0f, .0f, -1.0f, .0f, .0f, -.5f, -.5f, .5f, .0f, .0f, 1.0f, .0f, .0f, .5f, -.5f, .5f, .0f, .0f, 1.0f, 1.0f, .0f, .5f, .5f, .5f, .0f, .0f, 1.0f, 1.0f, 1.0f, .5f, .5f, .5f, .0f, .0f, 1.0f, 1.0f, 1.0f, -.5f, .5f, .5f, .0f, .0f, 1.0f, .0f, 1.0f, -.5f, -.5f, .5f, .0f, .0f, 1.0f, .0f, .0f, -.5f, .5f, .5f, -1.0f, .0f, .0f, 1.0f, .0f, -.5f, .5f, -.5f, -1.0f, .0f, .0f, 1.0f, 1.0f, -.5f, -.5f, -.5f, -1.0f, .0f, .0f, .0f, 1.0f, -.5f, -.5f, -.5f, -1.0f, .0f, .0f, .0f, 1.0f, -.5f, -.5f, .5f, -1.0f, .0f, .0f, .0f, .0f, -.5f, .5f, .5f, -1.0f, .0f, .0f, 1.0f, .0f, .5f, .5f, .5f, 1.0f, .0f, .0f, 1.0f, .0f, .5f, .5f, -.5f, 1.0f, .0f, .0f, 1.0f, 1.0f, .5f, -.5f, -.5f, 1.0f, .0f, .0f, .0f, 1.0f, .5f, -.5f, -.5f, 1.0f, .0f, .0f, .0f, 1.0f, .5f, -.5f, .5f, 1.0f, .0f, .0f, .0f, .0f, .5f, .5f, .5f, 1.0f, .0f, .0f, 1.0f, .0f, -.5f, -.5f, -.5f, .0f, -1.0f, .0f, .0f, 1.0f, .5f, -.5f, -.5f, .0f, -1.0f, .0f, 1.0f, 1.0f, .5f, -.5f, .5f, .0f, -1.0f, .0f, 1.0f, .0f, .5f, -.5f, .5f, .0f, -1.0f, .0f, 1.0f, .0f, -.5f, -.5f, .5f, .0f, -1.0f, .0f, .0f, .0f, -.5f, -.5f, -.5f, .0f, -1.0f, .0f, .0f, 1.0f, -.5f, .5f, -.5f, .0f, 1.0f, .0f, .0f, 1.0f, .5f, .5f, -.5f, .0f, 1.0f, .0f, 1.0f, 1.0f, .5f, .5f, .5f, .0f, 1.0f, .0f, 1.0f, .0f, .5f, .5f, .5f, .0f, 1.0f, .0f, 1.0f, .0f, -.5f, .5f, .5f, .0f, 1.0f, .0f, .0f, .0f, -.5f, .5f, -.5f, .0f, 1.0f, .0f, .0f, 1.0f};
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
  auto texCoordsAttr = glGetAttribLocation(objectShader.ID, "texCoordsIn");
  glVertexAttribPointer(posAttr, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
  glEnableVertexAttribArray(posAttr);
  glVertexAttribPointer(normAttr, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
  glEnableVertexAttribArray(normAttr);
  glVertexAttribPointer(texCoordsAttr, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
  glEnableVertexAttribArray(texCoordsAttr);
  // set up light VAO
  glBindVertexArray(lightVAO);
  glBindBuffer(GL_ARRAY_BUFFER, VBO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
  lightShader.Use();
  posAttr = glGetAttribLocation(lightShader.ID, "posIn");
  glVertexAttribPointer(posAttr, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
  glEnableVertexAttribArray(posAttr);
  glEnable(GL_DEPTH_TEST);
  glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
  glfwSetCursorPosCallback(window, MouseCallback);
  auto model = glm::mat4(1.0f);
  auto view = camera.GetViewMatrix();
  auto projection = glm::perspective(glm::radians(VIEW_ANGLE), (float)WINDOW_WIDTH / WINDOW_HEIGHT, NEAR_PLANE, FAR_PLANE);
  auto lightAmbient = glm::vec3(.2f, .2f, .2f);
  auto lightDiffuse = glm::vec3(.5f, .5f, .5f);
  auto lightSpecular = glm::vec3(1.0f, 1.0f, 1.0f);
  objectShader.Use();
  objectShader.SetMat4("model", model);
  objectShader.SetMat4("view", view);
  objectShader.SetMat4("projection", projection);
  objectShader.SetVec3("light.ambient", lightAmbient);
  objectShader.SetVec3("light.diffuse", lightDiffuse);
  objectShader.SetVec3("light.specular", lightSpecular);
  objectShader.SetInt("material.diffuse", 0);
  objectShader.SetInt("material.specular", 1);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, diffuseMap);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, specularMap);
  objectShader.SetFloat("material.shininess", 32.0f);
  auto lightPos = glm::vec3(1.2f, 1.0f, 2.0f);
  objectShader.SetVec3("light.position", lightPos);
  auto lightScale = glm::vec3(.2f);
  model = glm::translate(model, lightPos);
  model = glm::scale(model, lightScale);
  lightShader.Use();
  lightShader.SetMat4("model", model);
  lightShader.SetMat4("view", view);
  lightShader.SetMat4("projection", projection);
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
    /* glm::vec3 lightColor; */
    /* lightColor.x = sin(timeNow * 1.1f); */
    /* lightColor.y = sin(timeNow * .3f); */
    /* lightColor.z = sin(timeNow * .7f); */
    /* auto diffuseColor = lightColor * .5f; */
    /* auto ambientColor = diffuseColor * .2f; */
    objectShader.Use();
    objectShader.SetMat4("view", view);
    objectShader.SetVec3("light.position", lightPosNew);
    /* objectShader.SetVec3("light.diffuse", diffuseColor); */
    /* objectShader.SetVec3("light.ambient", ambientColor); */
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
static unsigned int LoadTexture(char const* path) {
  unsigned int textureID;
  glGenTextures(1, &textureID);
  int width, height, nrComponents;
  auto data = stbi_load(path, &width, &height, &nrComponents, 0);
  if (data) {
    GLenum format;
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
    std::cout << "Failed to load the texture at " << path << std::endl;
  stbi_image_free(data);
  return textureID;
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
