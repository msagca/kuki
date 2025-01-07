#version 330 core
#define MAX_POINT_LIGHTS 8
in vec3 position;
in vec3 normal;
in vec2 texCoord;
out vec4 color;
struct Material {
    vec3 diffuse;
    vec3 specular;
    float shininess;
};
struct DirectionalLight {
    vec3 direction;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};
struct PointLight {
    vec3 position;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    float constant;
    float linear;
    float quadratic;
};
uniform vec3 viewPos;
uniform Material material;
uniform bool hasDirectionalLight;
uniform DirectionalLight directionalLight;
uniform int numPointLights;
uniform PointLight pointLights[MAX_POINT_LIGHTS];
vec3 CalculateDirectionalLight(DirectionalLight light, Material material, vec3 normalDir, vec3 viewDir) {
    vec3 lightDir = normalize(light.direction);
    vec3 reflectDir = reflect(-lightDir, normalDir);
    float diffuseStrength = max(dot(normalDir, lightDir), 0.0);
    float specularStrength = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
    vec3 ambientColor = light.ambient * material.diffuse;
    vec3 diffuseColor = light.diffuse * diffuseStrength * material.diffuse;
    vec3 specularColor = light.specular * specularStrength * material.specular;
    return ambientColor + diffuseColor + specularColor;
}
vec3 CalculatePointLight(PointLight light, Material material, vec3 normalDir, vec3 viewDir, vec3 position) {
    vec3 lightDir = normalize(light.position - position);
    float distance = length(lightDir);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * distance * distance);
    vec3 reflectDir = reflect(-lightDir, normalDir);
    float diffuseStrength = max(dot(normalDir, lightDir), 0.0);
    float specularStrength = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
    vec3 ambientColor = light.ambient * material.diffuse * attenuation;
    vec3 diffuseColor = light.diffuse * diffuseStrength * material.diffuse * attenuation;
    vec3 specularColor = light.specular * specularStrength * material.specular * attenuation;
    return ambientColor + diffuseColor + specularColor;
}
void main() {
    vec3 normalDir = normalize(normal);
    vec3 viewDir = normalize(viewPos - position);
    vec3 finalColor;
    if (hasDirectionalLight)
        finalColor += CalculateDirectionalLight(directionalLight, material, normalDir, viewDir);
    for (int i = 0; i < numPointLights; ++i)
        finalColor += CalculatePointLight(pointLights[i], material, normalDir, viewDir, position);
    color = vec4(finalColor, 1.0);
}
