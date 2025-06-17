#version 450

layout(location = 0) in vec3 InPosition;
layout(location = 1) in vec2 UVcoords;

layout(location = 0) out vec2 OutUVcoords;

void main() {
    vec4 Pos = vec4(InPosition.xyz, 1.0);
    gl_Position = Pos;
    OutUVcoords = UVcoords;
}
