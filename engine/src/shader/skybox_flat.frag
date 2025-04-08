#version 460 core
in vec2 texCoord;
out vec4 color;
uniform samplerCube skybox;
const float PI = 3.1415927;
void main() {
    vec2 uv = texCoord * 2.0 - vec2(1.0);
    float longitude = uv.x * PI;
    float latitude = uv.y * (PI / 2.0);
    vec3 dir;
    dir.x = cos(latitude) * sin(longitude);
    dir.y = sin(latitude);
    dir.z = cos(latitude) * cos(longitude);
    color = texture(skybox, dir);
}
