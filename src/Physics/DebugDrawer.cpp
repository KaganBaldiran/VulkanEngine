#include "DebugDrawer.hpp"

void VKPHYSICS::DebugDrawer::ClearDebugBuffers()
{
	DebugLines.clear();
}

VKPHYSICS::DebugDrawer::DebugDrawer() :m_debugMode(0)
{
}

void VKPHYSICS::DebugDrawer::drawLine(const btVector3& from, const btVector3& to, const btVector3& color)
{
	DebugLines.insert(DebugLines.end(), {
		{{from.x(),from.y(),from.z()},
		{color.x(),color.y(),color.z()}},
		{{to.x(),to.y(),to.z()},
		{color.x(),color.y(),color.z()}
	}});
}

void VKPHYSICS::DebugDrawer::drawContactPoint(const btVector3& PointOnB, const btVector3& normalOnB, btScalar distance, int lifeTime, const btVector3& color)
{
}

void VKPHYSICS::DebugDrawer::reportErrorWarning(const char* warningString)
{
}

void VKPHYSICS::DebugDrawer::draw3dText(const btVector3& location, const char* textString)
{
}

void VKPHYSICS::DebugDrawer::setDebugMode(int debugMode)
{
	m_debugMode = debugMode;
}
