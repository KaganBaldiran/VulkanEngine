#include "Light.hpp"
#include "DependencyManager.hpp"

VKSCENE::Light::Light(ResourceDependencyManager& DependencyManager)
{
	Create(DependencyManager);
}

void VKSCENE::Light::Create(ResourceDependencyManager& DependencyManager)
{
	this->dependencyManager = &DependencyManager;
	resourceType = RESOURCE_TYPE_LIGHT;
}

void VKSCENE::Light::SetColor(const glm::vec4& Color)
{
	this->Data.Color = Color;
	this->SetDirty();
	Updated = true;
}

void VKSCENE::Light::SetPosition(const glm::vec4& Position)
{
	this->Data.PositionOrDirection = Position;
	this->SetDirty();
	Updated = true;
}

void VKSCENE::Light::SetDirection(const glm::vec4& Direction)
{
	this->Data.PositionOrDirection = Direction;
	this->SetDirty();
	Updated = true;
}

void VKSCENE::Light::SetIntensity(const float& Intensity)
{
	this->Data.Intensity = Intensity;
	this->SetDirty();
	Updated = true;
}

void VKSCENE::Light::SetType(const LightType& Type)
{
	this->Data.Type = Type;
	this->SetDirty();
	Updated = true;
}
