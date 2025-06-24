#version 450

layout(location = 0) in vec3 InPosition;
layout(location = 1) in vec3 InColor;

layout(location = 0) out vec3 VertexColor;

layout(push_constant) uniform PushConstants
{
    mat4 ViewMatrix;
};

void main() {
    vec4 Pos = ViewMatrix * vec4(InPosition.xyz, 1.0);
    gl_Position = Pos;
    VertexColor = InColor;
}
