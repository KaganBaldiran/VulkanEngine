#version 450

layout(location = 0) out vec4 FragColor;

layout(location = 0) in vec3 LocalPos;

layout(set=0,binding = 0) uniform sampler2D HDRIimage;

const vec2 invAtan =  vec2(0.1591, 0.3183);

vec2 SampleSphericalMap(vec3 Vector)
{
   vec2 UV = {atan(Vector.z,Vector.x),asin(Vector.y)};
   UV *= invAtan;
   UV += 0.5f;
   return UV;
}

void main() {
    vec2 UV = SampleSphericalMap(normalize(LocalPos));
    vec4 Color = texture(HDRIimage,UV);
    FragColor = Color;
}
