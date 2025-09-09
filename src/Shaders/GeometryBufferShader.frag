#version 450
#extension GL_EXT_nonuniform_qualifier : require
layout(early_fragment_tests) in;

layout(location = 0) out vec4 Position;
layout(location = 1) out vec4 Normal;
layout(location = 2) out int Albedo;
layout(location = 3) out vec4 RoughnessMetallic;

layout(location = 0) in vec3 OutNormals;
layout(location = 1) in vec3 OutPosition;
layout(location = 2) in vec2 OutUVcoords;
layout(location = 3) flat in int MeshIndex;
layout(location = 4) in vec3 OutTangent;
layout(location = 5) in vec3 OutBitangent;
layout(location = 6) in vec3 OutNDCVelocity;

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

void main() {
    Position = vec4(OutPosition,1.0f);
    Albedo = MeshIndex;
    MaterialTextureIndexes Indexes = TextureIndexes[MeshIndex];

    //Albedo = vec4(MeshIndex,vec3(1));
    //Normal = vec4(OutNormals,1.0f);
    vec2 FlippedUV = vec2(OutUVcoords.x,1.0f - OutUVcoords.y);
    RoughnessMetallic = vec4(FlippedUV,1.0f,1.0f);
    //RoughnessMetallic = FlippedUV;

    if(Indexes.NormalMapTextureIndex >= 0)  
    {
        vec3 TextureNormal = texture(Textures[nonuniformEXT(Indexes.NormalMapTextureIndex)],FlippedUV).xyz * 2.0 - 1.0; 
        mat3 TBNmatrix = mat3(OutTangent,OutBitangent,OutNormals);
        Normal = vec4(normalize(TBNmatrix * TextureNormal),1.0f);
    }
    else Normal = vec4(OutNormals,1.0f);
    

    //Albedo = Indexes.AlbedoTextureIndex;

    /*
    if(Indexes.AlbedoTextureIndex >= 0)  Albedo = vec4(texture(Textures[nonuniformEXT(Indexes.AlbedoTextureIndex)],FlippedUV).xyz,1.0f);
    else Albedo = vec4(1.0f);
    
    
    
    float Roughness = 0.5f;
    float Metallic = 0.0f;
    if(Indexes.RoughnessTextureIndex >= 0)  Roughness = texture(Textures[nonuniformEXT(Indexes.RoughnessTextureIndex)],FlippedUV).x;
    if(Indexes.MetallicTextureIndex >= 0)  Metallic = texture(Textures[nonuniformEXT(Indexes.MetallicTextureIndex)],FlippedUV).x;

    //RoughnessMetallic = vec4(OutNDCVelocity * 0.5f + 0.5f,1.0f);
    RoughnessMetallic = vec4(Roughness,Metallic,1.0f,1.0f);
    */
}
