#pragma once
#include <btBulletDynamicsCommon.h>
#include <LinearMath/btVector3.h>
#include <LinearMath/btAlignedObjectArray.h>
#include <memory>

namespace VKPHYSICS
{
    class PhysicsContext
    {
    public:
        std::shared_ptr<btDefaultCollisionConfiguration> CollisionConfiguration;
        std::shared_ptr<btCollisionDispatcher> Dispatcher;
        std::shared_ptr<btBroadphaseInterface> OverlappingPairCache;
        std::shared_ptr<btSequentialImpulseConstraintSolver> Solver;
        std::shared_ptr<btDiscreteDynamicsWorld> DynamicsWorld;

        PhysicsContext()
        {
            CollisionConfiguration = std::make_shared<btDefaultCollisionConfiguration>();
            Dispatcher = std::make_shared < btCollisionDispatcher>(CollisionConfiguration.get());
            OverlappingPairCache = std::make_shared<btDbvtBroadphase>();
            Solver = std::make_shared<btSequentialImpulseConstraintSolver>();
            DynamicsWorld = std::make_shared<btDiscreteDynamicsWorld>(Dispatcher.get(), OverlappingPairCache.get(),
                Solver.get(), CollisionConfiguration.get());
        };
    };

}
