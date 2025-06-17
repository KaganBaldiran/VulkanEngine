#version 450

layout(location = 0) in vec3 InPosition;
layout(location = 1) in vec3 InColor;
layout(location = 2) in vec2 UVcoords;
layout(location = 3) in vec3 Normals;

layout(location = 0) out vec3 OutNormals;
layout(location = 1) out vec3 OutPosition;

layout(location = 2) out vec2 OutUVcoords;

layout(binding = 0,set = 0) uniform Matrixes{
    mat4 ViewMatrix;
    mat4 ProjectionMatrix;
};

layout(push_constant) uniform PushConstants {
    mat4 ModelMatrix;
} PC;

void main() {
    vec4 Pos = ProjectionMatrix * ViewMatrix * PC.ModelMatrix * vec4(InPosition.xyz, 1.0);
    OutPosition = InPosition.xyz;
    gl_Position = Pos;
    OutNormals = normalize(transpose(inverse(mat3(PC.ModelMatrix))) * Normals);
    OutUVcoords = UVcoords;
}
