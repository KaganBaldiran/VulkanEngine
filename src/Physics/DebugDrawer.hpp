#pragma once
#include <btBulletCollisionCommon.h>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vector>

namespace VKPHYSICS
{
    struct DebugLineVertexInfo
    {
        glm::vec3 Position;
        glm::vec3 Color;
    };

    class DebugDrawer : public btIDebugDraw
    {
        int m_debugMode;
    public:
        std::vector<DebugLineVertexInfo> DebugLines;
        void ClearDebugBuffers();

        DebugDrawer();

        virtual void   drawLine(const btVector3& from, const btVector3& to, const btVector3& color);
        virtual void   drawContactPoint(const btVector3& PointOnB, const btVector3& normalOnB, btScalar distance, int lifeTime, const btVector3& color);
        virtual void   reportErrorWarning(const char* warningString);
        virtual void   draw3dText(const btVector3& location, const char* textString);
        virtual void   setDebugMode(int debugMode);
        virtual int    getDebugMode() const { return m_debugMode; };
    };
}