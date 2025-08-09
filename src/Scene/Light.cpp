#include "Light.hpp"
#include "DependencyManager.hpp"

SCENE::Light::Light(ResourceDependencyManager& DependencyManager)
{
	Create(DependencyManager);
}

void SCENE::Light::Create(ResourceDependencyManager& DependencyManager)
{
	this->dependencyManager = &DependencyManager;
	resourceType = RESOURCE_TYPE_LIGHT;
}

void SCENE::Light::SetColor(const glm::vec4& Color)
{
	this->Data.Color = Color;
	this->SetDirty();
	Updated = true;
}

void SCENE::Light::SetPosition(const glm::vec4& Position)
{
	this->Data.PositionOrDirection = Position;
	this->SetDirty();
	Updated = true;
}

void SCENE::Light::SetDirection(const glm::vec4& Direction)
{
	this->Data.PositionOrDirection = Direction;
	this->SetDirty();
	Updated = true;
}

void SCENE::Light::SetIntensity(const float& Intensity)
{
	this->Data.Intensity = Intensity;
	this->SetDirty();
	Updated = true;
}

void SCENE::Light::SetType(const LightType& Type)
{
	this->Data.Type = Type;
	this->SetDirty();
	Updated = true;
}
