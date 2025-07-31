#version 450
#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) out vec4 Normal;
layout(location = 1) out vec4 Position;
layout(location = 2) out vec4 Albedo;
layout(location = 3) out vec4 RoughnessMetallic;

layout(location = 0) in vec3 OutNormals;
layout(location = 1) in vec3 OutPosition;
layout(location = 2) in vec2 OutUVcoords;
layout(location = 3) flat in int MeshIndex;
layout(location = 4) in vec3 OutTangent;
layout(location = 5) in vec3 OutBitangent;

layout(set = 2,binding = 0) uniform sampler2D Textures[];
layout(std430 ,set = 2,binding = 1) readonly buffer TextureIndexBuffer{
    int TextureIndexes[];
};

void main() {
    Position = vec4(OutPosition,1.0f);

    int AlbedoTextureIndex = TextureIndexes[MeshIndex * 5];
    int RoughnessTextureIndex = TextureIndexes[(MeshIndex * 5) + 1];
    int NormalMapTextureIndex =TextureIndexes[(MeshIndex * 5) + 2];
    int MetallicTextureIndex = TextureIndexes[(MeshIndex * 5) + 3];
    int OpacityTextureIndex = TextureIndexes[(MeshIndex * 5) + 4];

    Normal = vec4(OutNormals,1.0f);
    
    if(AlbedoTextureIndex >= 0)  Albedo = vec4(texture(Textures[nonuniformEXT(AlbedoTextureIndex)],1.0 - OutUVcoords).xyz,1.0f);
    else Albedo = vec4(1.0f);
    
    if(NormalMapTextureIndex >= 0)  
    {
        vec3 TextureNormal = texture(Textures[nonuniformEXT(NormalMapTextureIndex)],1.0 - OutUVcoords).xyz * 2.0 - 1.0; 
        mat3 TBNmatrix = mat3(OutTangent,OutBitangent,OutNormals);
        Normal = vec4(normalize(TBNmatrix * TextureNormal),1.0f);
    }
    else Normal = vec4(OutNormals,1.0f);
    
    float Roughness = 0.5f;
    float Metallic = 0.0f;
    if(RoughnessTextureIndex >= 0)  Roughness = texture(Textures[nonuniformEXT(RoughnessTextureIndex)],1.0 - OutUVcoords).x;
    if(MetallicTextureIndex >= 0)  Metallic = texture(Textures[nonuniformEXT(MetallicTextureIndex)],1.0 - OutUVcoords).x;

    RoughnessMetallic = vec4(Roughness,Metallic,0.0f,1.0f);
}
