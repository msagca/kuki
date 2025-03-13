#version 460 core
out vec3 position;
out vec3 normal;
out vec2 texCoord;
layout(location = 0) in vec3 i_position;
layout(location = 1) in vec3 i_normal;
layout(location = 2) in vec2 i_texCoord;
layout(location = 3) in vec4 i_model0;
layout(location = 4) in vec4 i_model1;
layout(location = 5) in vec4 i_model2;
layout(location = 6) in vec4 i_model3;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform bool useInstancing;
void main() {
    mat4 instanceModel;
    if (useInstancing)
        instanceModel = mat4(i_model0, i_model1, i_model2, i_model3);
    else
        instanceModel = model;
    vec4 worldPosition = instanceModel * vec4(i_position, 1.0);
    position = vec3(worldPosition);
    normal = mat3(transpose(inverse(instanceModel))) * i_normal;
    texCoord = i_texCoord;
    gl_Position = projection * view * worldPosition;
}
