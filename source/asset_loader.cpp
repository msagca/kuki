#include <asset_loader.hpp>
#include <asset_manager.hpp>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <iostream>
#include <ostream>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <string>
#include <component_types.hpp>
#include <glad/glad.h>
AssetLoader::AssetLoader(AssetManager& assetManager)
  : assetManager(assetManager) {}
void AssetLoader::LoadModel(const std::string& path) {
  Assimp::Importer importer;
  const auto scene = importer.ReadFile(path, aiProcess_CalcTangentSpace | aiProcess_Triangulate | aiProcess_JoinIdenticalVertices | aiProcess_SortByPType);
  if (nullptr == scene) {
    std::cerr << "Assimp: " << importer.GetErrorString() << std::endl;
    return;
  }
}
void AssetLoader::LoadTexture(const std::string& path) {
  auto assetID = assetManager.Create();
  auto& texture = assetManager.AddComponent<Texture>(assetID);
  glGenTextures(1, &texture.id);
  int width, height, nrComponents;
  auto data = stbi_load(path.c_str(), &width, &height, &nrComponents, 0);
  texture.width = width;
  texture.height = height;
  if (data) {
    GLenum format = 0;
    if (nrComponents == 1)
      format = GL_RED;
    else if (nrComponents == 3)
      format = GL_RGB;
    else if (nrComponents == 4)
      format = GL_RGBA;
    glBindTexture(GL_TEXTURE_2D, texture.id);
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  } else
    std::cerr << "Failed to load the texture at " << path << "." << std::endl;
  stbi_image_free(data);
}
