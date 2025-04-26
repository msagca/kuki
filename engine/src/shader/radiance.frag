#version 460 core
out vec4 color;
in vec3 localPos;
uniform sampler2D equirectangularMap;
const vec2 invAtan = vec2(0.1591, 0.3183); // 1/(2*PI), 1/PI
vec2 SampleSphericalMap(vec3 v) {
    vec2 uv = vec2(atan(v.z, v.x), asin(v.y)); // [-PI, PI], [-PI/2, PI/2]
    uv *= invAtan; // [-0.5, 0.5]
    uv += 0.5; // [0, 1]
    return uv;
}
void main() {
    vec2 uv = SampleSphericalMap(normalize(localPos));
    color = vec4(texture(equirectangularMap, uv).rgb, 1.0);
}
