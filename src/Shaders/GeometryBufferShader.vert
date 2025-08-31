#version 450

layout(location = 0) in vec3 InPosition;
layout(location = 1) in vec2 UVcoords;
layout(location = 2) in vec3 Normals;
layout(location = 3) in vec3 Tangent;
layout(location = 4) in vec3 Bitangent;

layout(location = 0) out vec3 OutNormals;
layout(location = 1) out vec3 OutPosition;
layout(location = 2) out vec2 OutUVcoords;
layout(location = 3) flat out int MeshIndex;
layout(location = 4) out vec3 OutTangent;
layout(location = 5) out vec3 OutBitangent;

layout(push_constant) uniform Matrixes{
    mat4 ViewMatrix;
    mat4 ProjectionMatrix;
};

struct DrawMetadata {
	int MeshID;       
	int ModelMatrixIndex;
};

layout(std430,binding = 0,set = 0) readonly buffer ModelMatrixesBuffer{
    mat4 ModelMatrixes[];
};

layout(std430,binding = 2,set = 0) readonly buffer DrawMetaDataBuffer{
    DrawMetadata DrawDatas[];
};

void main() {
    DrawMetadata DrawData = DrawDatas[gl_InstanceIndex];
    mat4 ModelMatrix = ModelMatrixes[DrawData.ModelMatrixIndex];
    MeshIndex = DrawData.MeshID;

    vec3 GlobalPosition = vec3(ModelMatrix * vec4(InPosition.xyz, 1.0));
    vec4 Pos = ProjectionMatrix * ViewMatrix * vec4(GlobalPosition.xyz, 1.0);
    OutPosition = GlobalPosition;
    gl_Position = Pos;
    OutUVcoords = UVcoords;

    mat3 NormalMatrix = transpose(inverse(mat3(ModelMatrix)));
    OutNormals = normalize(NormalMatrix * Normals);
    OutTangent = normalize(NormalMatrix * Tangent);
    OutBitangent = normalize(NormalMatrix * Bitangent);    
}
