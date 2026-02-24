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
layout(location = 6) out vec3 OutNDCVelocity;

layout(push_constant) uniform Matrixes{
    mat4 ProjViewMatrix;
    mat4 PreviousProjViewMatrix;
};

struct DrawMetadata {
    int MaterialID;
	int MeshID;
	int ModelMatrixIndex;
};

struct ModelTransformMatrixes
{
    mat4 ModelMatrix;
    mat4 NormalMatrix;
};

layout(std430,binding = 0,set = 0) readonly buffer ModelTransformMatricesBuffer{
    ModelTransformMatrixes TransformMatrixesData[];
};

layout(std430,binding = 2,set = 0) readonly buffer DrawMetaDataBuffer{
    DrawMetadata DrawDatas[];
};

layout(std430,binding = 3,set = 0) readonly buffer VisibleDataBuffer{
    uint VisibleIndices[];
};

void main() {
    uint InstanceIndex = VisibleIndices[gl_InstanceIndex];
    DrawMetadata DrawData = DrawDatas[InstanceIndex];
    ModelTransformMatrixes TransformMatrices = TransformMatrixesData[DrawData.ModelMatrixIndex];
    MeshIndex = DrawData.MaterialID;

    vec3 GlobalPosition = vec3(TransformMatrices.ModelMatrix * vec4(InPosition.xyz, 1.0));
    vec4 PreviousPos = PreviousProjViewMatrix * vec4(GlobalPosition.xyz, 1.0);
    vec4 CurrentPos = ProjViewMatrix * vec4(GlobalPosition.xyz, 1.0);
    //CurrentPos /= CurrentPos.w; 
    //PreviousPos /= PreviousPos.w;
    OutNDCVelocity = CurrentPos.xyz - PreviousPos.xyz;

    OutPosition = GlobalPosition;
    gl_Position = CurrentPos;
    OutUVcoords = UVcoords;

    mat3 NormalMatrix = mat3(TransformMatrices.NormalMatrix);
    OutNormals = normalize(NormalMatrix * Normals);
    OutTangent = normalize(NormalMatrix * Tangent);
    OutBitangent = normalize(NormalMatrix * Bitangent);    
}
