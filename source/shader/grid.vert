#version 330 core
out vec3 worldPos;
layout(location = 0) in vec3 i_position;
uniform mat4 view;
uniform mat4 projection;
uniform vec3 cameraPos;
uniform vec3 cameraFront;
void main() {
    float t = -cameraPos.y / cameraFront.y;
    vec3 intersection = cameraPos + t * cameraFront;
    float distanceX = intersection.x - cameraPos.x;
    float distanceZ = intersection.z - cameraPos.z;
    vec3 pos3 = i_position;
    pos3.x += cameraPos.x + distanceX;
    pos3.z += cameraPos.z + distanceZ;
    worldPos = pos3;
    vec4 pos4 = vec4(pos3, 1.0);
    gl_Position = projection * view * pos4;
}
