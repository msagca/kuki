#ifndef SHADER_H
#define SHADER_H
#include <string>
using namespace std;
class Shader {
public:
  unsigned int progID;
  Shader(const char* vertexPath, const char* fragmentPath);
  void use() const;
  void setBool(const string& name, bool value) const;
  void setInt(const string& name, int value) const;
  void setFloat(const string& name, float value) const;
};
#endif
