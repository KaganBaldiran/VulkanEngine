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
    float zNear = 0.1f;
    float zFar = 2000.0f;

    float Depth = texture(DepthBuffer,OutUVcoords).x;
    Depth = zNear * zFar / (zFar + Depth * (zNear - zFar));
    Depth = (Depth - zNear) / (zFar - zNear);
    float ClampedDepth = clamp(1.0f - Depth,0.0001f,0.0005f);

    float Step = clamp(1.0f - Depth,0.003f,0.007f);
   // vec2 UV = ivec2(OutUVcoords / Step) * Step; 
    vec2 UV = OutUVcoords; 
    vec4 Scene = texture(SceneColor,UV).xyzw;

    if(Scene.w < 1.0f) discard;  
   
    vec3 Normal = texture(NormalBuffer,UV).xyz;
    float Result = 1.0f;
    int SampleCount = 4;
    for(int x = -SampleCount; x <= SampleCount;x++)
    {
        for(int y = -SampleCount; y <= SampleCount;y++)
        {
            //vec3 SampleNormal = texture(NormalBuffer,OutUVcoords + vec2(x,y) * clamp((1.0f - NormalizedDistance),0.0001f,0.0005f)).xyz;
            vec3 SampleNormal = texture(NormalBuffer,UV + vec2(x,y) * ClampedDepth).xyz;
            Result += max(0.0f,dot(SampleNormal,Normal));
        }
    }
    float sampleCountTotal = float((2*SampleCount)*(2*SampleCount));
    float OutlineColor = Result / sampleCountTotal;
   

    //float Depth = texture(DepthBuffer,OutUVcoords).x;
    //Depth = zNear * zFar / (zFar + Depth * (zNear - zFar));
    //Depth = (Depth - zNear) / (zFar - zNear);

    float OulineSq = floor(OutlineColor * OutlineColor);
    */
   // outColor = vec4(mix(vec3(0.0f,0.0f,0.0f),Scene.xyz,OulineSq),1.0f);

    vec4 Scene = texture(SceneColor,OutUVcoords).xyzw;
    //outColor = vec4(1.0,0.0,1.0,1.0f);
    outColor = vec4(Scene.xyz,1.0f);
}