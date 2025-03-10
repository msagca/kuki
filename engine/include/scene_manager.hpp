#pragma once
#include <engine_export.h>
#include <scene.hpp>
#include <unordered_map>
#include <string>
class ENGINE_API SceneManager {
private:
  unsigned int nextID = 0;
  std::unordered_map<unsigned int, Scene*> idToScene;
  std::unordered_map<std::string, unsigned int> nameToID;
public:
  ~SceneManager();
  unsigned int Create(const std::string&);
  void Delete(unsigned int);
  void Delete(const std::string&);
  Scene* Get(unsigned int);
  Scene* Get(const std::string&);
  bool Has(unsigned int);
  bool Has(const std::string&);
};
