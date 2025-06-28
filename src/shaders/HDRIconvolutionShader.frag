#version 450

layout(location = 0) out vec4 FragColor;
layout(location = 0) in vec3 LocalPos;
layout(set=0,binding = 0) uniform samplerCube CubeMapimage;

const float PI = 3.14159265359;

vec3 Convolute(vec3 Normal)
{
    vec3 Irradiance = vec3(0.0f);

    vec3 Up = Normal.y < 0.9999f ? vec3(0.0f,1.0f,0.0f) : vec3(0.0f,0.0f,1.0f);
    vec3 Right = normalize(cross(Up,Normal));
    Up = normalize(cross(Normal,Right));

    float Delta = 0.025f;
    float SampleCount = 0.0f;
    for(float phi = 0.0; phi < 2.0 * PI; phi += Delta)
    {
        for(float theta = 0.0; theta < 0.5 * PI; theta += Delta)
        {
            vec3 TangentSample = vec3(sin(theta) * cos(phi),sin(theta) * sin(phi),cos(theta));
            vec3 SampleVector = mat3(Right,Up,Normal) * TangentSample;

            Irradiance += texture(CubeMapimage,SampleVector).rgb * cos(theta) * sin(theta); 
            SampleCount++;
        }
    }

    Irradiance = PI * Irradiance * (1.0f / SampleCount);
    return Irradiance;
}

void main() {
    vec3 Normal = normalize(LocalPos);

    vec3 Color = Convolute(Normal);
    FragColor = vec4(Color,1.0f);
}
