#version 460 core

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
   uint IndirectCommandCount;
};

layout(std430,binding = 1,set = 0) buffer IndirectCommandBuffer{
    IndirectCommand IndirectCommands[];
};

layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;
void main()
{
    uint GlobalCommandID = gl_GlobalInvocationID.x;
	if(GlobalCommandID >= IndirectCommandCount) return;
	IndirectCommands[GlobalCommandID].InstanceCount = 0;
}