#version 330 core
out vec3 pos;
out float scale;
layout(location = 0) in vec3 i_position;
uniform mat4 view;
uniform mat4 projection;
uniform vec3 cameraPos;
uniform vec3 cameraFront;
uniform float cameraFOV;
void main() {
    vec3 pos3 = normalize(i_position);
    float angle = acos(length(cameraFront.y) / length(cameraFront));
    float dist = cameraPos.y * tan(angle + radians(cameraFOV / 2));
    scale = cameraPos.y * dist;
    pos3.x *= scale;
    pos3.z *= scale;
    pos = pos3;
    vec4 pos4 = vec4(pos3, 1.0);
    gl_Position = projection * view * pos4;
}
