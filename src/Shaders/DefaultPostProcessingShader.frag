#version 450

layout(location = 0) out vec4 outColor;
layout(location = 0) in vec2 OutUVcoords;

layout(push_constant) uniform FrameData{
    vec3 CameraDirection;
    float FogIntensity;
    vec3 CameraPosition;
    float CameraFrustumLength;
    float Time;
};

layout(set = 0,binding = 0) uniform sampler2D SceneColor;
layout(set = 0,binding = 1) uniform sampler2D DepthBuffer;
layout(set = 0,binding = 2) uniform sampler2D NormalBuffer;

void main()
{
/*
    vec3 Normal = texture(NormalBuffer,OutUVcoords).xyz;
    float Result = 0.0f;
    int SampleCount = 4;
    for(int x = -SampleCount; x <= SampleCount;x++)
    {
        for(int y = -SampleCount; y <= SampleCount;y++)
        {
            vec3 SampleNormal = texture(NormalBuffer,OutUVcoords + vec2(x,y) * clamp((1.0f - NormalizedDistance),0.0001f,0.0005f)).xyz;
            Result += max(0.0f,dot(SampleNormal,Normal));
        }
    }
    float sampleCountTotal = float((2*SampleCount)*(2*SampleCount));
    vec3 OutlineColor = vec3(Result / sampleCountTotal);
   */
    vec3 Scene = texture(SceneColor,OutUVcoords).xyz;
    outColor = vec4(Scene,1.0f);

   

}