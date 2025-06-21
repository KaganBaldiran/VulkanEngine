#include "app/Renderer.hpp"

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#define STB_IMAGE_IMPLEMENTATION
#include "../include/stbi/stb_image.h"


#include <btBulletDynamicsCommon.h>
#include <LinearMath/btVector3.h>
#include <LinearMath/btAlignedObjectArray.h>

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

int main()
{
    try
    {
        if (!glfwInit())
        {
            throw std::runtime_error("Unable to initialize GLFW");
        }

        VKAPP::RendererContext RendererContext(true);
        VKAPP::Renderer Renderer;
        Renderer.Initialize(RendererContext,false);

        VKSCENE::Entity Sponza;
        VKSCENE::Entity Shovel;

        VKSCENE::MeshImporter Importer;
        Importer.AppendImportTask({ &Sponza.Model , "resources\\sponza.obj" });
        Importer.AppendImportTask({ &Shovel.Model , "resources\\shovel2.obj" });
        Importer.SubmitImport();
        Importer.WaitImportIdle();
       
        Sponza.Model.transformation.ScalingMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(0.1f));
        Shovel.Model.transformation.ScalingMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(4.0f));

        VKSCENE::Camera3D Camera(RendererContext.Window);

        VKSCENE::Light Light0;
        Light0.SetColor(glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
        Light0.SetIntensity(5.0f);
        Light0.SetDirection(glm::vec4(0.0f, 0.0f, 1.0f, 0.0f));
        Light0.SetType(VKSCENE::DIRECTIONAL_LIGHT);

        VKSCENE::Light Light1;
        Light1.SetColor(glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
        Light1.SetIntensity(5.0f);
        Light1.SetDirection(glm::vec4(0.3f, 0.5f, 0.0f, 0.0f));
        Light1.SetType(VKSCENE::DIRECTIONAL_LIGHT);

        VKSCENE::Light Light2;
        Light2.SetColor(glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
        Light2.SetIntensity(5.0f);
        Light2.SetDirection(glm::vec4(0.8f * glm::cos(glfwGetTime()), 0.4f, 1.0f * glm::sin(glfwGetTime()), 0.0f));
        Light2.SetType(VKSCENE::DIRECTIONAL_LIGHT);

        VKSCENE::Scene Scene0;
        Scene0.Create(RendererContext);
        Scene0.Entities.push_back(&Sponza);
        Scene0.Entities.push_back(&Shovel);
        
        Scene0.CreateLightBuffers(RendererContext, 2, 1);
        Scene0.CreateMeshBuffers(RendererContext);

        Scene0.StaticLights.push_back(&Light0);
        Scene0.StaticLights.push_back(&Light1);
        Scene0.UpdateStaticLightBuffers(RendererContext);

        Scene0.DynamicLights.push_back(&Light2);
        Scene0.UpdateDynamicLightBuffers();

        PhysicsContext PhyContext;
        PhyContext.DynamicsWorld->setGravity({ 0,-10,0 });
        btAlignedObjectArray<btCollisionShape*> CollisionShapes;

        std::unique_ptr<btTriangleMesh> TriangleMesh = std::make_unique<btTriangleMesh>();        
        for (auto &Mesh : Sponza.Model.Meshes)
        {
            for (size_t y = 0; y < Mesh.Indices.size(); y += 3)
            {
                auto &Triangle0 = Mesh.Vertices[Mesh.Indices[y]].Position;
                auto &Triangle1 = Mesh.Vertices[Mesh.Indices[y + 1]].Position;
                auto &Triangle2 = Mesh.Vertices[Mesh.Indices[y + 2]].Position;
                TriangleMesh->addTriangle(
                    { Triangle0.x,Triangle0.y ,Triangle0.z }, 
                    { Triangle1.x,Triangle1.y ,Triangle1.z },
                    { Triangle2.x,Triangle2.y ,Triangle2.z }
                );
            }
        }
        std::unique_ptr<btBvhTriangleMeshShape> StaticMeshShape = std::make_unique<btBvhTriangleMeshShape>(TriangleMesh.get(),true);
        //StaticMeshShape->setLocalScaling(btVector3(4.0f,4.0f,4.0f));
        StaticMeshShape->setLocalScaling(btVector3(0.1f,0.1f,0.1f));
        CollisionShapes.push_back(StaticMeshShape.get());

        btTransform GroundTransform;
        GroundTransform.setIdentity();
        GroundTransform.setOrigin({ 0,0,0 });

        std::unique_ptr<btDefaultMotionState> GroundMotionState = std::make_unique<btDefaultMotionState>(GroundTransform);
        btRigidBody::btRigidBodyConstructionInfo GroundRigidBodyCreateInfo(0, GroundMotionState.get(), StaticMeshShape.get());
        std::unique_ptr<btRigidBody> GroundRigidBody = std::make_unique<btRigidBody>(GroundRigidBodyCreateInfo);

        PhyContext.DynamicsWorld->addRigidBody(GroundRigidBody.get());

        float DeltaTime = 0.0f;
        float LastFrame = 0.0f;

        VKPHYSICS::DebugDrawer PhysicsDebugDrawer;

        PhysicsDebugDrawer.setDebugMode(btIDebugDraw::DBG_DrawAabb | btIDebugDraw::DBG_DrawWireframe);
        PhyContext.DynamicsWorld->setDebugDrawer(&PhysicsDebugDrawer);
        Scene0.DebugDrawer = &PhysicsDebugDrawer;
         
        while (!glfwWindowShouldClose(RendererContext.Window.window))
        {
            float CurrentTime = glfwGetTime();
            DeltaTime = CurrentTime - LastFrame;
            LastFrame = CurrentTime;

            //std::cout << "Delta time: " << DeltaTime << std::endl;

            glm::vec4 AllowMove = { 1,1,1,1 };
            btVector3 From = { Camera.CameraPosition.x,Camera.CameraPosition.y ,Camera.CameraPosition.z };
            btVector3 To = From + btVector3{ Camera.CameraDirection.x, Camera.CameraDirection.y, Camera.CameraDirection.z };
            btCollisionWorld::ClosestRayResultCallback RayCallBack0(From, To);
            PhyContext.DynamicsWorld->rayTest(From, To, RayCallBack0);
            if (RayCallBack0.hasHit()) AllowMove.x = 0;

            To = From + btVector3{ -Camera.CameraRight.x, -Camera.CameraRight.y, -Camera.CameraRight.z };
            btCollisionWorld::ClosestRayResultCallback RayCallBack1(From, To);
            PhyContext.DynamicsWorld->rayTest(From, To, RayCallBack1);
            if (RayCallBack1.hasHit()) AllowMove.y = 0;
           
            To = From + btVector3{ Camera.CameraRight.x, Camera.CameraRight.y, Camera.CameraRight.z };
            btCollisionWorld::ClosestRayResultCallback RayCallBack2(From, To);
            PhyContext.DynamicsWorld->rayTest(From, To, RayCallBack2);
            if (RayCallBack2.hasHit()) AllowMove.z = 0;

            To = From + btVector3{ -Camera.CameraDirection.x, -Camera.CameraDirection.y, -Camera.CameraDirection.z };
            btCollisionWorld::ClosestRayResultCallback RayCallBack3(From, To);
            PhyContext.DynamicsWorld->rayTest(From, To, RayCallBack3);
            if (RayCallBack3.hasHit()) AllowMove.w = 0;
          
            Camera.AllowMove = AllowMove;
            //PhyContext.DynamicsWorld->debugDrawWorld();
            Light2.SetDirection(glm::vec4(1.0f * glm::cos(glfwGetTime()), 0.4f, 1.0f * glm::sin(glfwGetTime()), 0.0f));
            Scene0.UpdateDynamicFrameLightBuffers(Renderer.CurrentFrame);

            Camera.Update(RendererContext.Window,50.0f,DeltaTime);
            Camera.UpdateMatrix({ RendererContext.SwapChain.Extent.width,RendererContext.SwapChain.Extent.height });
            Renderer.RenderFrame(Scene0, Camera);

            //PhysicsDebugDrawer.ClearDebugBuffers();
        }

        RendererContext.WaitDeviceIdle();
        Scene0.Destroy(RendererContext);
        Renderer.Destroy();
        RendererContext.Destroy();
        glfwTerminate();
    }
    catch (const std::exception& e)
    {
        std::cout << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}