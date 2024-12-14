#pragma once
#include <primitive.hpp>
std::vector<float> Cube::GetVertices(float size) {
  auto halfSize = size / 2;
  return {-halfSize, -halfSize, -halfSize, halfSize, -halfSize, -halfSize, halfSize, halfSize, -halfSize, -halfSize, halfSize, -halfSize, -halfSize, -halfSize, halfSize, halfSize, -halfSize, halfSize, halfSize, halfSize, halfSize, -halfSize, halfSize, halfSize};
}
std::vector<unsigned int> Cube::GetIndices() {
  return {0, 1, 2, 2, 3, 0, 4, 5, 6, 6, 7, 4, 4, 5, 1, 1, 0, 4, 7, 6, 2, 2, 3, 7, 4, 0, 3, 3, 7, 4, 5, 1, 2, 2, 6, 5};
}
