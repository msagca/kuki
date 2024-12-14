#pragma once
#include <vector>
#include <component_types.hpp>
MeshFilter CreateMesh(const std::vector<float>&);
MeshFilter CreateMesh(const std::vector<float>&, const std::vector<unsigned int>&);
