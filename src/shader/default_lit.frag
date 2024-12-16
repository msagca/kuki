#version 330 core
in vec3 position;
in vec3 normal;
out vec4 fragColor;
struct Material {
    vec3 diffuse;
    vec3 specular;
    float shininess;
};
struct Light {
    vec4 vector;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    float constant;
    float linear;
    float quadratic;
};
uniform vec3 viewPos;
uniform Material material;
uniform Light light;
void main() {
    vec3 normalDir = normalize(normal);
    vec3 viewDir = normalize(viewPos - position);
    vec3 lightDir;
    float attenuation = 1.0;
    if(light.vector.w == 0.0) // directional light
        lightDir = normalize(light.vector.xyz);
    else { // point light
        lightDir = normalize(light.vector.xyz - position);
        float distance = length(lightDir);
        float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));
    }
    vec3 reflectDir = reflect(lightDir, normalDir);
    float diffuseStrength = max(dot(normalDir, lightDir), 0.0);
    float specularStrength = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
    vec3 ambientColor = light.ambient * material.diffuse * attenuation;
    vec3 diffuseColor = light.diffuse * (diffuseStrength * material.diffuse) * attenuation;
    vec3 specularColor = light.specular * (specularStrength * material.specular) * attenuation;
    fragColor = vec4(ambientColor + diffuseColor + specularColor, 1.0);
}
