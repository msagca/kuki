#pragma once
#include "component.hpp"
#include "mesh.hpp"
#include <string>
#include <vector>
struct MeshFilter : IComponent {
  Mesh mesh{};
  std::string GetName() const override {
    return "MeshFilter";
  }
  std::vector<Property> GetProperties() const override {
    return mesh.GetProperties();
  }
  void SetProperty(Property property) override {
    mesh.SetProperty(property);
  }
};
