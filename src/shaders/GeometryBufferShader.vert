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

layout(std140,binding = 0,set = 1) readonly buffer ModelMatrixesBuffer{
    mat4 ModelMatrixes[];
};

void main() {
    mat4 ModelMatrix = ModelMatrixes[gl_InstanceIndex];
    vec3 GlobalPosition = vec3(ModelMatrix * vec4(InPosition.xyz, 1.0));
    vec4 Pos = ProjectionMatrix * ViewMatrix * vec4(GlobalPosition.xyz, 1.0);
    OutPosition = GlobalPosition;
    gl_Position = Pos;
    OutNormals = normalize(transpose(inverse(mat3(ModelMatrix))) * Normals);
    OutUVcoords = UVcoords;
}
