#version 450

layout(location = 0) out vec4 outColor;

layout(location = 0) in vec2 OutUVcoords;

layout(set = 0,binding = 0) uniform sampler2D NormalBuffer;
layout(set = 0,binding = 1) uniform sampler2D PositionBuffer;

layout(set = 0,binding = 2) uniform FrameData{
    vec4 CameraDirection;
    vec4 CameraPosition;
    int StaticLightCount;
    int DynamicLightCount;
};

struct Light
{
	vec4 Color;
	vec4 PositionOrDirection;
	float Intensity;
	int Type;
};

layout(std140 ,set = 1,binding = 0) buffer StaticLightBuffers{
    Light StaticLights[];
};

layout(std140 ,set = 1,binding = 1) buffer DynamicLightBuffers{
    Light DynamicLights[];
};

const float PI = 3.14159265359;
const float Inv_PI = 1.0f / PI;

float DistributionGGX(vec3 N , vec3 H, float roughness)
  {
    float a = clamp(roughness * roughness, 0.03f, 1.0f);         
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0f);       
    float NdotH2 = NdotH * NdotH;

    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0f) + 1.0f);
    denom = max(PI * denom * denom, 0.0001f); 

    return num / denom;
  }

 float GeometrySchlickGGX(float NdotV , float roughness)
  {
      float r = (roughness + 1.0);
      float k = (r*r) / 8.0;

      float num = NdotV;
      float denom = NdotV * (1.0 - k) + k;

      return num / denom;
  }

  float GeometrySmith(vec3 N , vec3 V , vec3 L , float roughness)
  {
      float NdotV = max(dot(N,V),0.0);
      float NdotL = max(dot(N,L),0.0);
      float ggx2 = GeometrySchlickGGX(NdotV,roughness);
      float ggx1 = GeometrySchlickGGX(NdotL,roughness);

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

    float NdotL = dot(Normal, LightDirection);
    if (NdotL <= 0.0)
        return vec3(0.0);

    float R0 = pow((1.0 - IOR) / (1.0 + IOR), 2.0);
    vec3 F0 = mix(vec3(R0), Albedo, Metallic);
    vec3 H = normalize(ViewDirection + L);

    vec3 F = fresnelSchlickRoughness(max(dot(Normal, H), 0.0), F0, Roughness);
    float NDF = DistributionGGX(Normal, H, Roughness);
    float G = GeometrySmith(Normal, ViewDirection, L, Roughness);

    float NdotV = max(dot(Normal, ViewDirection), 0.0);
    vec3 kD = (1.0 - F) * (1.0 - Metallic);

    vec3 specular = (NDF * G * F) / max(4.0 * max(NdotL, 0.0) * NdotV, 1e-4);
    return (kD * Albedo * Inv_PI + specular) * LightColor * max(NdotL, 0.0);
}

void main() {
    vec4 Normal = texture(NormalBuffer,OutUVcoords);
    float Alpha = Normal.w;
    if(Alpha <= 0.0f) discard;
    vec3 Position = texture(PositionBuffer,OutUVcoords).xyz;

    const vec3 LightDirections[2] = {vec3(0.3f,0.8f,0.0f),vec3(0.7f,0.2f,0.5f)};
    const vec3 LightColors[2] = {vec3(0.3f,0.8f,0.7f),vec3(0.9f,0.9f,0.9f)};

    vec3 V = normalize(CameraPosition.xyz - Position);
    vec3 Lo = vec3(0.0f);
    Light CurrentLight;

    


    for(int i=0;i < StaticLightCount;i++)
    {
       CurrentLight = StaticLights[i];
       Lo += CookTorranceBRDF(
                Normal.xyz, 
                V,
                Position,
                CurrentLight.PositionOrDirection.xyz,
                CurrentLight.Color.xyz * CurrentLight.Intensity,
                CurrentLight.Type,
                0.5f,
                0.0f,
                vec3(1.0f),
                0.04f
            );
    }
    for(int i=0;i < DynamicLightCount;i++)
    {
       CurrentLight = DynamicLights[i];
       Lo += CookTorranceBRDF(
                Normal.xyz, 
                V,
                Position,
                CurrentLight.PositionOrDirection.xyz,
                CurrentLight.Color.xyz * CurrentLight.Intensity,
                CurrentLight.Type,
                0.5f,
                0.0f,
                vec3(1.0f),
                0.04f
            );   
    }
    float AmbientLight = 0.2f;
    outColor = vec4(vec3(Lo),1.0f);
}
