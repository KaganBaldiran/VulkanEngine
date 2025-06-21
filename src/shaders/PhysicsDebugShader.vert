#version 450

layout(location = 0) in vec3 InPosition;
layout(location = 1) in vec3 InColor;

layout(location = 0) out vec3 VertexColor;

layout(binding = 0,set = 0) uniform Matrixes{
    mat4 ViewMatrix;
    mat4 ProjectionMatrix;
};

void main() {
    vec4 Pos = ProjectionMatrix * ViewMatrix * vec4(InPosition.xyz, 1.0);
    gl_Position = Pos;
    VertexColor = InColor;
}
