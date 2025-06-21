#include "Light.hpp"

void VKSCENE::Light::SetColor(const glm::vec4& Color)
{
	this->Data.Color = Color;
	Updated = true;
}

void VKSCENE::Light::SetPosition(const glm::vec4& Position)
{
	this->Data.PositionOrDirection = Position;
	Updated = true;
}

void VKSCENE::Light::SetDirection(const glm::vec4& Direction)
{
	this->Data.PositionOrDirection = Direction;
	Updated = true;
}

void VKSCENE::Light::SetIntensity(const float& Intensity)
{
	this->Data.Intensity = Intensity;
	Updated = true;
}

void VKSCENE::Light::SetType(const LightType& Type)
{
	this->Data.Type = Type;
	Updated = true;
}
