#version 450

layout(location = 0) out vec4 FragColor;

layout(location = 2) in vec2 OutUVcoords;
layout(location = 3) flat in int MeshIndex;

struct MaterialTextureIndexes
{
    int AlbedoTextureIndex;
    int RoughnessTextureIndex;
    int NormalMapTextureIndex;
    int MetallicTextureIndex;
    int OpacityTextureIndex;
};

layout(set = 1,binding = 0) uniform sampler2D Textures[];
layout(std430 ,set = 2,binding = 0) readonly buffer TextureIndexBuffer{
    MaterialTextureIndexes TextureIndexes[];
};

void main()
{
    MaterialTextureIndexes Indexes = TextureIndexes[MeshIndex];
    vec2 FlippedUV = vec2(OutUVcoords.x,1.0f - OutUVcoords.y);

    FragColor = vec4(1.0f,0.0f,0.0f,1.0f); 
    if(Indexes.AlbedoTextureIndex >= 0)  FragColor = texture(Textures[nonuniformEXT(Indexes.AlbedoTextureIndex)],FlippedUV);
}