#version 450

layout(location = 0) in vec3 InPosition;

layout(location = 0) out vec3 LocalPos;

layout(push_constant) uniform PushConstants {
    mat4 ViewMatrix;
    mat4 ProjectionMatrix;
};

void main() {
    vec4 Pos = ProjectionMatrix * ViewMatrix * vec4(InPosition.xyz, 1.0);
    gl_Position = Pos;
    LocalPos = InPosition.xyz;
}
