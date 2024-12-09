#pragma once
#include <vector>
#define IMGUI_IMAGE_IMPLEMENTATION
#include <stb_image.h>
class Cube {
public:
  static std::vector<float> GetVertices(float size = 1.0f) {
    auto halfSize = size / 2;
    return {-halfSize, -halfSize, -halfSize, halfSize, -halfSize, -halfSize, halfSize, halfSize, -halfSize, -halfSize, halfSize, -halfSize, -halfSize, -halfSize, halfSize, halfSize, -halfSize, halfSize, halfSize, halfSize, halfSize, -halfSize, halfSize, halfSize};
  }
  static std::vector<unsigned int> GetIndices() {
    return {0, 1, 2, 2, 3, 0, 4, 5, 6, 6, 7, 4, 4, 5, 1, 1, 0, 4, 7, 6, 2, 2, 3, 7, 4, 0, 3, 3, 7, 4, 5, 1, 2, 2, 6, 5};
  }
};
