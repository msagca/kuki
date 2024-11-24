#version 330 core
in vec3 fragPos;
in vec3 normal;
in vec2 texCoords;
out vec4 fragColor;
struct Material {
    sampler2D diffuse;
    sampler2D specular;
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
    vec3 diffuseSample = vec3(texture(material.diffuse, texCoords));
    vec3 ambientColor = light.ambient * diffuseSample;
    vec3 diffuseColor = light.diffuse * (diffuseStrength * diffuseSample);
    vec3 specularSample = vec3(texture(material.specular, texCoords));
    vec3 specularColor = light.specular * (specularStrength * specularSample);
    fragColor = vec4(ambientColor + diffuseColor + specularColor, 1.0);
}
