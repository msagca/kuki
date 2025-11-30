#version 460 core
const float PI = 3.14159265359;
const float SAMPLE_DELTA = 0.002;
in vec3 worldPos;
out vec4 color;
uniform samplerCube cubeMap;
void main() {
        vec3 normal = normalize(worldPos);
        vec3 irradiance = vec3(0.0);
        vec3 up = vec3(0.0, 1.0, 0.0);
        vec3 right = normalize(cross(up, normal));
        up = normalize(cross(normal, right));
        float nrSamples = 0.0;
        for (float phi = 0.0; phi < 2.0 * PI; phi += SAMPLE_DELTA)
                for (float theta = 0.0; theta < 0.5 * PI; theta += SAMPLE_DELTA) {
                        vec3 tangentSample = vec3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));
                        vec3 sampleVec = tangentSample.x * right + tangentSample.y * up + tangentSample.z * normal;
                        irradiance += texture(cubeMap, sampleVec).rgb * cos(theta) * sin(theta);
                        nrSamples++;
                }
        irradiance = PI * irradiance * (1.0 / float(nrSamples));
        color = vec4(irradiance, 1.0);
}
