#version 450
#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) out vec4 outColor;
layout(location = 0) in vec2 OutUVcoords;

layout(push_constant) uniform FrameData{
    vec3 CameraDirection;
    float FogIntensity;
    vec3 CameraPosition;
    float CameraFrustumLength;
    int StaticLightCount;
    int DynamicLightCount;
    float Time;
};

layout(set = 0,binding = 0) uniform sampler2D PositionBuffer;
layout(set = 0,binding = 1) uniform sampler2D NormalBuffer;
layout(set = 0,binding = 2) uniform isampler2D AlbedoBuffer;
layout(set = 0,binding = 3) uniform sampler2D RoughnessMetallicBuffer;

struct Light
{
	vec4 Color;
	vec4 PositionOrDirection;
	float Intensity;
	int Type;
};

layout(std430 ,set = 1,binding = 0) readonly buffer StaticLightBuffers{
    Light StaticLights[];
};

layout(std430 ,set = 1,binding = 1) readonly buffer DynamicLightBuffers{
    Light DynamicLights[];
};

layout(set = 1,binding = 2) uniform samplerCube Cubemap;

struct Material
{
    int AlbedoTextureIndex;
    int RoughnessTextureIndex;
    int NormalMapTextureIndex;
    int MetallicTextureIndex;
    int OpacityTextureIndex;

    //Parameters
	float Metallic;
	float Roughness;
    float Padding;
    vec4 Albedo;

    vec2 TextureSamplePosition;
    vec2 TextureSampleSize;
};

layout(set = 3,binding = 0) uniform sampler2D Textures[];
layout(std430,set = 2,binding = 0) readonly buffer TextureIndexBuffer{
    Material Materials[];
};

const float PI = 3.14159265359;
const float Inv_PI = 1.0f / PI;

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a      = clamp(roughness*roughness,1e-4,1.0f);
    float a2     = a*a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;
	
    float num   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
	
    return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float num   = NdotV;
    float denom = NdotV * (1.0 - k) + k;
	
    return num / denom;
}
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2  = GeometrySchlickGGX(NdotV, roughness);
    float ggx1  = GeometrySchlickGGX(NdotL, roughness);
	
    return ggx1 * ggx2;
}

vec3 FresnelSchlick(float cosTheta , vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta,0.0,1.0),5.0);
}

float FresnelSchlick(float cosTheta , float F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta,0.0,1.0),5.0);
}

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
} 

vec3 CookTorranceBRDF(vec3 Normal, vec3 ViewDirection,vec3 Position,vec3 LightDirection,vec3 LightColor,int LightType,float Roughness,float Metallic,vec3 Albedo,in float IOR)
{
    vec3 L;
    if(LightType == 0)
    {
       L = LightDirection;
    }
    else if(LightType == 1)
    {
       L = normalize(LightDirection - Position);    
    }

    float NdotL = dot(Normal, L);
    if (NdotL <= 0.0)
        return vec3(0.0);

    //float R0 = pow((1.0 - IOR) / (1.0 + IOR), 2.0);
    //vec3 F0 = mix(vec3(R0), Albedo, Metallic);
        vec3 F0 = mix(vec3(0.04f), Albedo, Metallic);
    vec3 H = normalize(ViewDirection + L);

    vec3 F = fresnelSchlickRoughness(max(dot(Normal, H), 0.0), F0, Roughness);
    float NDF = DistributionGGX(Normal, H, Roughness);
    float G = GeometrySmith(Normal, ViewDirection, L, Roughness);

    float NdotV = max(dot(Normal, ViewDirection), 0.0);
    vec3 kD = (1.0 - F) * (1.0 - Metallic);

    vec3 specular = (NDF * G * F) / max(4.0 * max(NdotL, 0.0) * NdotV, 1e-4);
    return (kD * Albedo * Inv_PI + specular) * LightColor * max(NdotL, 0.0);
}

vec3 CalculateLighting(in vec3 Normal,in vec3 Position,in vec3 Albedo,in float Roughness,in float Metallic)
{
    vec3 N = normalize(Normal.xyz);
    vec3 V = normalize(CameraPosition.xyz - Position);
    vec3 Lo = vec3(0.0f);

    Light CurrentLight;
    for(int i=0;i < StaticLightCount;i++)
    {
       CurrentLight = StaticLights[i];
       Lo += CookTorranceBRDF(
                N, 
                V,
                Position,
                CurrentLight.PositionOrDirection.xyz,
                CurrentLight.Color.xyz * CurrentLight.Intensity,
                CurrentLight.Type,
                Roughness,
                Metallic,
                Albedo,
                1.5f
            );
    }
    for(int i=0;i < DynamicLightCount;i++)
    {
       CurrentLight = DynamicLights[i];
       Lo += CookTorranceBRDF(
                N, 
                V,
                Position,
                CurrentLight.PositionOrDirection.xyz,
                CurrentLight.Color.xyz * CurrentLight.Intensity,
                CurrentLight.Type,
                Roughness,
                Metallic,
                Albedo,
                1.5f
       );   
    }
    //vec3 I = normalize(Position.xyz - CameraPosition.xyz);
    //vec3 R = reflect(I, normalize(Normal.xyz));
    vec3 F0 = mix(vec3(0.04f), Albedo, Metallic);
    vec3 FresnelSpecular = fresnelSchlickRoughness(max(dot(N,V),0.0f),F0,Roughness);
    vec3 FresnelDiffuse = (1.0 - FresnelSpecular) * (1.0 - Metallic);
    vec3 Irradiance = texture(Cubemap,N).xyz;
    vec3 Diffuse = Irradiance * Albedo;
    vec3 Ambient = (FresnelDiffuse * Diffuse);
    return Lo + Ambient;
}

/*
vec3 ShadePixel(in vec3 Normal,in vec3 Position,in vec3 Albedo,in float Roughness,in float Metallic,double Time)
{

    return CalculateLighting(Normal,Position,Albedo,Roughness,Metallic);
}
*/

//APPENDSPOT



void main() {
    int MeshIndex = int(texture(AlbedoBuffer,OutUVcoords).x);
    if(MeshIndex < 0) discard;

    vec2 UV = texture(RoughnessMetallicBuffer,OutUVcoords).xy;
    vec3 Normal = texture(NormalBuffer,OutUVcoords).xyz;
    vec3 Position = texture(PositionBuffer,OutUVcoords).xyz;

    Material MaterialData = Materials[MeshIndex];

    vec3 Albedo = MaterialData.Albedo.xyz;
    if(MaterialData.AlbedoTextureIndex >= 0) Albedo = texture(Textures[nonuniformEXT(MaterialData.AlbedoTextureIndex)],UV).xyz;

    float Roughness = MaterialData.Roughness;
    float Metallic = MaterialData.Metallic;
    if(MaterialData.RoughnessTextureIndex >= 0)  Roughness = texture(Textures[nonuniformEXT(MaterialData.RoughnessTextureIndex)],UV).x;
    if(MaterialData.MetallicTextureIndex >= 0)  Metallic = texture(Textures[nonuniformEXT(MaterialData.MetallicTextureIndex)],UV).x;

    vec3 ShadedPixel = ShadePixel(CameraPosition,CameraDirection,Normal,Position,Albedo,Roughness,Metallic,Time);
   
    float Distance = dot(Position - CameraPosition,Position - CameraPosition);
    float FogAmount = clamp(exp(-Distance / (CameraFrustumLength * CameraFrustumLength) * FogIntensity),0.0f,1.0f);

    outColor = vec4(mix(vec3(0.6f,0.7f,0.6f),ShadedPixel,FogAmount),1.0f);
}
