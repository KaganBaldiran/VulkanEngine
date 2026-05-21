#version 460 core

struct DrawMetadata {
    int MaterialID;
	int MeshID;
	int ModelMatrixIndex;
};

struct BoundingBoxAABB
{
    vec3 Center;
	vec4 Extends;
};

struct IndirectCommand
{
	uint IndexCount;
	uint InstanceCount;
	uint FirstIndex;
	int VertexOffset;
	uint FirstInstance;
	
	float CenterX, CenterY, CenterZ;
    float ExtentsX, ExtentsY, ExtentsZ, ExtentsW;
};

layout(push_constant) uniform MetaData{
   uint TotalInstanceCount;
   uint Padding0; 
   uint Padding1; 
   uint Padding2;
   vec4 FrustumPlanes[6];
};

struct ModelTransformMatrixes
{
    mat4 ModelMatrix;
    mat4 NormalMatrix;
};

layout(std430,binding = 0,set = 0) readonly buffer ModelTransformMatricesBuffer{
    ModelTransformMatrixes TransformMatrixesData[];
};

layout(std430,binding = 1,set = 0) buffer IndirectCommandBuffer{
    IndirectCommand IndirectCommands[];
};

layout(std430,binding = 2,set = 0) readonly buffer DrawMetaDataBuffer{
    DrawMetadata DrawDatas[];
};

layout(std430,binding = 3,set = 0) buffer VisibleDataBuffer{
    uint VisibleIndices[];
};

layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;
void main()
{
    uint GlobalInstID = gl_GlobalInvocationID.x;
	if(GlobalInstID >= TotalInstanceCount) return;
	DrawMetadata InstanceDrawData = DrawDatas[GlobalInstID];
	
	vec3 BoundingBoxCenter = vec3(
			IndirectCommands[InstanceDrawData.MeshID].CenterX,
			IndirectCommands[InstanceDrawData.MeshID].CenterY,
			IndirectCommands[InstanceDrawData.MeshID].CenterZ);

	vec3 BoundingBoxExtends = vec3(
			IndirectCommands[InstanceDrawData.MeshID].ExtentsX,
			IndirectCommands[InstanceDrawData.MeshID].ExtentsY,
			IndirectCommands[InstanceDrawData.MeshID].ExtentsZ);

    mat4 ModelMatrix = TransformMatrixesData[InstanceDrawData.ModelMatrixIndex].ModelMatrix;
	vec3 WorldBoundingBoxCenter = (ModelMatrix * vec4(BoundingBoxCenter,1.0f)).xyz;

	mat3 AbsModelMatrix = mat3(
		abs(ModelMatrix[0].xyz),
		abs(ModelMatrix[1].xyz),
		abs(ModelMatrix[2].xyz)
	);
	vec3 WorldBoundingBoxExtends = AbsModelMatrix * BoundingBoxExtends;

	bool IsVisible = true;
	for(int i = 0;i < 6;i++)
	{
	    vec4 FrustumPlane = FrustumPlanes[i];

		float Distance = dot(WorldBoundingBoxCenter,FrustumPlane.xyz) + FrustumPlane.w;
		float Radius = dot(WorldBoundingBoxExtends,abs(FrustumPlane.xyz));

		if(Distance < -Radius)
		{
			IsVisible = false;
			break;
		}
	}

	if(IsVisible)
	{
		uint LocalIndex = atomicAdd(IndirectCommands[InstanceDrawData.MeshID].InstanceCount,1); 
		VisibleIndices[IndirectCommands[InstanceDrawData.MeshID].FirstInstance + LocalIndex] = GlobalInstID;
	}
}