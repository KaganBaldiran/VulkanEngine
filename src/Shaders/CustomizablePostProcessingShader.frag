#version 450

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

layout(set = 0,binding = 0) uniform sampler2D SceneColor;
layout(set = 0,binding = 1) uniform sampler2D DepthBuffer;
layout(set = 0,binding = 2) uniform sampler2D NormalBuffer;

void main()
{
   outColor = vec4(texture(SceneColor,OutUVcoords).xyz,1.0f);
}