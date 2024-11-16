#version 330 core
in vec3 fragPos;
in vec3 normal;
out vec4 fragColor;
uniform vec3 objectColor;
uniform vec3 lightColor;
uniform vec3 lightPos;
uniform vec3 viewPos;
void main() {
  vec3 norm = normalize(normal);
  vec3 lightDir = normalize(lightPos - fragPos);
  vec3 viewDir = normalize(viewPos - fragPos);
  vec3 reflectDir = reflect(lightDir, norm);
  float specularStrength = 0.5;
  float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
  vec3 specularColor = specularStrength * spec * lightColor;
  float ambientStrength = 0.1;
  vec3 ambientColor = ambientStrength * lightColor;
  float diff = max(dot(norm, lightDir), 0.0);
  vec3 diffuseColor = diff * lightColor;
  vec3 finalColor = (ambientColor + diffuseColor + specularColor) * objectColor;
  fragColor = vec4(finalColor, 1.0);
}
