#include "display_traits.hpp"
using namespace kuki;
void Editor::DisplayProperties(IComponent* component) {
  if (!component)
    return;
  if (auto camera = component->As<Camera>())
    DisplayTraits<Camera>::DisplayProperties(camera, context);
  else if (auto light = component->As<Light>())
    DisplayTraits<Light>::DisplayProperties(light, context);
  else if (auto material = component->As<Material>())
    DisplayTraits<Material>::DisplayProperties(material, context);
  else if (auto mesh = component->As<Mesh>())
    DisplayTraits<Mesh>::DisplayProperties(mesh, context);
  else if (auto filter = component->As<MeshFilter>())
    DisplayTraits<MeshFilter>::DisplayProperties(filter, context);
  else if (auto renderer = component->As<MeshRenderer>())
    DisplayTraits<MeshRenderer>::DisplayProperties(renderer, context);
  else if (auto script = component->As<Script>())
    DisplayTraits<Script>::DisplayProperties(script, context);
  else if (auto skybox = component->As<Skybox>())
    DisplayTraits<Skybox>::DisplayProperties(skybox, context);
  else if (auto texture = component->As<Texture>())
    DisplayTraits<Texture>::DisplayProperties(texture, context);
  else if (auto transform = component->As<Transform>())
    DisplayTraits<Transform>::DisplayProperties(transform, context);
}
