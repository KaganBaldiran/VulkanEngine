#version 450

layout(location = 0) out vec4 Normal;
layout(location = 1) out vec4 Position;
layout(location = 2) out vec4 Albedo;
layout(location = 3) out vec4 RoughnessMetallic;

layout(location = 0) in vec3 OutNormals;
layout(location = 1) in vec3 OutPosition;

layout(location = 2) in vec2 OutUVcoords;

layout(set = 2,binding = 0) uniform sampler2D Textures[];
layout(std140 ,set = 2,binding = 1) readonly buffer TextureIndexBuffer{
    int TextureIndexes[];
};

void main() {
    Normal = vec4(OutNormals,1.0f);
    Position = vec4(OutPosition,1.0f);
    Albedo = vec4(texture(Textures[0],OutUVcoords).xyz,1.0f);
    RoughnessMetallic = vec4(1.0f,0.0f,0.0f,1.0f);
}
