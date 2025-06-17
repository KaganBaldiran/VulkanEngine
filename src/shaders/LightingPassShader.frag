#version 450

layout(location = 0) out vec4 outColor;

layout(location = 0) in vec2 OutUVcoords;

layout(set = 0,binding = 0) uniform sampler2D NormalBuffer;
layout(set = 0,binding = 1) uniform sampler2D PositionBuffer;
void main() {

    outColor = vec4(texture(NormalBuffer,OutUVcoords).rgb,1.0f);
}
