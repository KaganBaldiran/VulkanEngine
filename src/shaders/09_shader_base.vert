#version 450

layout(location = 0) in vec3 InPosition;
layout(location = 1) in vec3 InColor;
layout(location = 2) in vec2 UVcoords;
layout(location = 3) in vec3 Normals;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 OutUVcoords;

layout(binding = 0,set = 0) uniform Matrixes{
    mat4 ModelMatrix;
    mat4 ViewMatrix;
    mat4 ProjectionMatrix;
};

void main() {
    vec4 Pos = ProjectionMatrix * ViewMatrix * ModelMatrix * vec4(InPosition.xyz, 1.0);
    gl_Position = Pos;
    fragColor = transpose(inverse(mat3(ModelMatrix))) * Normals;
    OutUVcoords = UVcoords;
}
