#version 330 core
in vec3 fragPos;
in vec3 normal;
out vec4 fragColor;
struct Material {
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    float shininess;
};
struct Light {
    vec3 position;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};
uniform vec3 viewPos;
uniform Material material;
uniform Light light;
void main() {
    vec3 normDir = normalize(normal);
    vec3 lightDir = normalize(light.position - fragPos);
    vec3 viewDir = normalize(viewPos - fragPos);
    vec3 reflectDir = reflect(lightDir, normDir);
    float diffuseStrength = max(dot(normDir, lightDir), 0.0);
    float specularStrength = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
    vec3 ambientColor = light.ambient * material.ambient;
    vec3 diffuseColor = light.diffuse * (diffuseStrength * material.diffuse);
    vec3 specularColor = light.specular * (specularStrength * material.specular);
    vec3 finalColor = ambientColor + diffuseColor + specularColor;
    fragColor = vec4(finalColor, 1.0);
}
