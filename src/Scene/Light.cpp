#include "Light.hpp"

SCENE::Light::Light()
{}

void SCENE::Light::SetColor(const glm::vec4& Color)
{
	this->Data.Color = Color;
	Updated = true;
}

void SCENE::Light::SetPosition(const glm::vec4& Position)
{
	this->Data.PositionOrDirection = Position;
	Updated = true;
}

void SCENE::Light::SetDirection(const glm::vec4& Direction)
{
	this->Data.PositionOrDirection = Direction;
	Updated = true;
}

void SCENE::Light::SetIntensity(const float& Intensity)
{
	this->Data.Intensity = Intensity;
	Updated = true;
}

void SCENE::Light::SetType(const LightType& Type)
{
	this->Data.Type = Type;
	Updated = true;
}
