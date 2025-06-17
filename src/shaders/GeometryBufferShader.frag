#version 450

layout(location = 0) out vec4 Normal;
layout(location = 1) out vec4 Position;

layout(location = 0) in vec3 OutNormals;
layout(location = 1) in vec3 OutPosition;

layout(location = 2) in vec2 OutUVcoords;

void main() {
    Normal = vec4(OutNormals,1.0f);
    Position = vec4(OutPosition,1.0f);
}
