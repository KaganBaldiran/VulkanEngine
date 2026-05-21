#include "Renderer/Renderer.hpp"
#include "Renderer/ResourceManager.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "../include/stbi/stb_image.h"

#include <btBulletDynamicsCommon.h>
#include <LinearMath/btVector3.h>
#include <LinearMath/btAlignedObjectArray.h>

#include "Physics/PhysicsContext.hpp"
#include "Common/MemoryArenaAllocator.hpp"
#include "Renderer/ShadowMapManager.hpp"
#include "Renderer/RenderGraph.hpp"

#include <random>
#include <chrono>

int main()
{
    try
    {
        if (!glfwInit())
        {
            throw std::runtime_error("Unable to initialize GLFW");
        }

        RENDERER::RendererSettings RendererSettings{};
        RendererSettings.EnableValidationLayers = false;
        RendererSettings.BuildRayTracingAccelerationStructures = true;

        RENDERER::RendererContext RendererContext(1000, 800,"HelloWorld",RendererSettings);
        RENDERER::Renderer Renderer(RendererContext, false);
        
        //RendererContext.Window.SetFullScreen(true);

        SCENE::ModelHandle SponzaModel;
        SCENE::ModelHandle ShovelModel;
        SCENE::ModelHandle SceneModel;
        SCENE::ModelHandle Quad;

        std::vector<SCENE::ModelInstance> Shovels;

        std::mt19937 rng(std::random_device{}());
        std::uniform_real_distribution<float> distX(-1000.0f, 1000.0f);
        std::uniform_real_distribution<float> distY(10.0f, 30.0f);
        std::uniform_real_distribution<float> distZ(-1000.0f, 1000.0f);
        std::uniform_real_distribution<float> distRot(0.0f, glm::two_pi<float>());

        SCENE::Texture SomeTexture{};
        SCENE::Texture AnimationSprite{};
        
        RENDERER::ResourceManager ResourceManager(RendererContext);

        ResourceManager.AppendModelImportTask({ &Quad , "Resources\\Quad.fbx" });
        //ResourceManager.AppendModelImportTask({ &SceneModel , "C:\\Users\\kbald\\Desktop\\SunTemple\\SunTemple.fbx" });
       // ResourceManager.AppendModelImportTask({ &SponzaModel , "Resources\\sponza.obj" });
        //ResourceManager.AppendModelImportTask({ &ShovelModel , "Resources\\shovel2.obj" });
        //ResourceManager.AppendModelImportTask({ &SceneModel , "C:\\Users\\kbald\\Desktop\\SunTemple\\SunTemple.fbx" });
        //ResourceManager.AppendModelImportTask({ &Quad , "Resources\\Quad.fbx" });
        ResourceManager.SubmitModelImports();
        ResourceManager.WaitModelImportsIdle();

        ResourceManager.AppendTextureImportTask({ "Resources\\SpriteAnimation3.jpg",AnimationSprite.GetHandleID()});
        ResourceManager.SubmitTextureImports();
        ResourceManager.WaitTextureImportsIdle();

        SCENE::SceneOptions Options{};
        Options.UploadMode = SCENE::SCENE_DYNAMIC_UPLOAD_MODE_DEVICE_LOCAL;

        SCENE::Scene Scene0;
        Scene0.Create(RendererContext, ResourceManager, Options);

        SCENE::Scene Scene1;
        Scene1.Create(RendererContext, ResourceManager, Options);

        SCENE::ModelInstance QuadInstance0(Quad);
        Scene0.LinkModelInstance(QuadInstance0);

        /*
        Scene0.FlushPendingUpdates(
            SCENE::SCENE_UPDATE_TYPE_ALL_PENDING,
            FRAME_INDEX_ALL_FRAMES
        );
        */
        ResourceManager.AppendModelImportTask({ &SponzaModel , "Resources\\sponza.obj" });
        ResourceManager.AppendModelImportTask({ &ShovelModel , "Resources\\shovel2.obj" });
        //ResourceManager.AppendModelImportTask({ &SponzaModel , "C:\\Users\\kbald\\Downloads\\main_sponza\\main_sponza\\NewSponza_Main_glTF_003.gltf" });
        ResourceManager.SubmitModelImports();
        ResourceManager.WaitModelImportsIdle();

        ResourceManager.AppendTextureImportTask({ "C:\\Users\\kbald\\Downloads\\wallhaven-lmmeqq_2560x1080.png",SomeTexture.GetHandleID() });
        ResourceManager.SubmitTextureImports();
        ResourceManager.WaitTextureImportsIdle();
        
        SCENE::ModelInstance Sponza(SponzaModel);
        SCENE::ModelInstance Shovel(ShovelModel);
        SCENE::ModelInstance Shovel1(ShovelModel);
        SCENE::ModelInstance SceneModelInstance(SceneModel);

        SCENE::ModelInstance SponzaTextured(SponzaModel);

        auto ShovelMaterial = Shovel.GetMaterial(0);
        ShovelMaterial->TextureSampleSize = glm::vec2(20);
        ShovelMaterial->ReferenceTexture(SomeTexture.GetHandleID(), SCENE::MATERIAL_TEXTURE_TYPE_ALBEDO);

        auto QuadMaterial = QuadInstance0.GetMaterial(0);
        //QuadMaterial->ReferenceTexture(AnimationSprite.GetHandleID(), SCENE::MATERIAL_TEXTURE_TYPE_ALBEDO);

        QuadMaterial->TextureSampleSize = glm::vec2(1);
        QuadMaterial->ReferenceTexture(AnimationSprite.GetHandleID(), SCENE::MATERIAL_TEXTURE_TYPE_ALBEDO);
        QuadInstance0.Transformations.TranslationMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 300.0f, 0.0f));
        QuadInstance0.Transformations.RotationMatrix = glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));

        SCENE::Material NewMaterial{};
        NewMaterial.Metallic = 0.7f;
        NewMaterial.Roughness = 0.5f;
        NewMaterial.Albedo = glm::vec4(0.2f, 0.2f, 0.2f, 1.0f);
        for (size_t i = 0; i < 1500; i++)
        {
            SCENE::ModelInstance NewShovel(ShovelModel);
            *NewShovel.GetMaterial(0) = NewMaterial;

            glm::vec3 position(distX(rng), distY(rng), distZ(rng));
            float rotationAngle = distRot(rng);

            glm::mat4 transform = glm::translate(glm::mat4(1.0f), position);
            transform = glm::rotate(transform, rotationAngle, glm::vec3(0.0f, 1.0f, 0.0f));

            NewShovel.Transformations.TranslationMatrix = transform;
            NewShovel.Transformations.ScalingMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(4.0f));
            Shovels.push_back(NewShovel);
        }

        Sponza.Transformations.ScalingMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(0.1f));
        //SponzaTextured.Transformations.ScalingMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(5.0f));
        Sponza.Transformations.RotationMatrix = glm::rotate(glm::mat4(1.0f), glm::radians(45.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        Shovel.Transformations.ScalingMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(4.0f));

        SCENE::Cubemap Cubemap0(RendererContext, 1024, 1024);
        SCENE::ImportHDRI("resources\\boma_4k.hdr", Cubemap0, RendererContext);

        SCENE::Cubemap Cubemap1(RendererContext, 1024, 1024);
        SCENE::ImportHDRI("resources\\rustig_koppie_puresky_2k.hdr", Cubemap1, RendererContext);

        SCENE::Light Light0;
        Light0.SetColor(glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
        Light0.SetIntensity(1.0f);
        Light0.SetDirection(glm::vec4(0.0f, 1.0f, 0.7f, 0.0f));
        Light0.SetType(SCENE::DIRECTIONAL_LIGHT);

        SCENE::Light Light1;
        Light1.SetColor(glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
        Light1.SetIntensity(5.0f);
        Light1.SetDirection(glm::vec4(0.3f, 0.5f, 0.0f, 0.0f));
        Light1.SetType(SCENE::DIRECTIONAL_LIGHT);

        SCENE::Light Light2;
        Light2.SetColor(glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
        Light2.SetIntensity(1.0f);
        Light2.SetDirection(glm::vec4(0.8f * glm::cos(glfwGetTime()), 0.4f, 1.0f * glm::sin(glfwGetTime()), 0.0f));
        Light2.SetType(SCENE::DIRECTIONAL_LIGHT);

        Shovel1.Transformations.TranslationMatrix = glm::translate(glm::mat4(1.0f), { 30,100,40.0f });

        Scene0.LinkStaticLight(Light0);
        Scene0.LinkStaticLight(Light1);
        Scene0.LinkDynamicLight(Light2);

        Scene1.LinkStaticLight(Light0);
        Scene1.LinkStaticLight(Light1);
        Scene1.LinkDynamicLight(Light2);

        //Scene0.LinkModelInstance(SceneModelInstance);
        //Scene0.LinkModelInstance(Shovel1);

        Scene0.LinkModelInstance(Shovel);
        Scene0.LinkModelInstance(Sponza);
        Scene0.LinkModelInstance(SponzaTextured);

        for (size_t i = 0; i < 100; i++)
        {
            Scene0.LinkModelInstance(Shovels[i]);
        }

        /*
        Scene0.FlushPendingUpdates(
            SCENE::SCENE_UPDATE_TYPE_ALL_PENDING,
            FRAME_INDEX_ALL_FRAMES
        );
        */

        for (size_t i = 101; i < 200; i++)
        {
            Scene0.LinkModelInstance(Shovels[i]);
        }

        /*
        Scene0.FlushPendingUpdates(
            SCENE::SCENE_UPDATE_TYPE_ALL_PENDING,
            FRAME_INDEX_ALL_FRAMES
        );
        */


        Scene1.LinkModelInstance(SceneModelInstance);
        Scene1.LinkModelInstance(Shovel1);
        /*
        Scene1.FlushPendingUpdates(
            SCENE::SCENE_UPDATE_TYPE_ALL_PENDING,
            FRAME_INDEX_ALL_FRAMES
        );
        */
        Scene0.LinkCubemap(Cubemap0);
        Scene1.LinkCubemap(Cubemap0);

        

        /*
        VKPHYSICS::PhysicsContext PhyContext;
        PhyContext.DynamicsWorld->setGravity({ 0,-10,0 });
        btAlignedObjectArray<btCollisionShape*> CollisionShapes;

        std::unique_ptr<btTriangleMesh> TriangleMesh = std::make_unique<btTriangleMesh>();
        for (auto &Mesh : Sponza.Source->Meshes)
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

        VKPHYSICS::DebugDrawer PhysicsDebugDrawer;


        PhysicsDebugDrawer.setDebugMode(btIDebugDraw::DBG_DrawAabb | btIDebugDraw::DBG_DrawWireframe);
        PhyContext.DynamicsWorld->setDebugDrawer(&PhysicsDebugDrawer);
        Scene0.DebugDrawer = &PhysicsDebugDrawer;
         */
       

        auto Stats = RendererContext.QueryMemoryStats();
        std::cout << "Memory usage is " << Stats.TotalUsedBytes / (1024.0f * 1024.0f) << "/" << Stats.TotalBudgetBytes / (1024.0f * 1024.0f) << "(" << Stats.UsageRate << "%)." << std::endl;

        std::string ShadeFunction =
            "vec3 ShadePixel(in vec3 CameraPosition,in vec3 CameraDirection,in vec3 Normal, in vec3 Position, in vec3 Albedo, in float Roughness, in float Metallic, in float Time) \n\
            {  \n\
                \n\
                 return CalculateLighting(Normal,Position,Albedo,Roughness,Metallic);\n\
                 //return Normal;\n\
            }";

        RENDERER::DeferredRenderPipeline DeferredPipeline(RendererContext);
        DeferredPipeline.CompileCustomPipeline(ShadeFunction, "Shaders/CustomShaders/CustomShader0");
        RENDERER::DeferredRenderPipeline DeferredPipelineNormal(RendererContext);

        VkViewport Viewport0{};
        Viewport0.x = 0.0f;
        Viewport0.y = 0.0f;
        Viewport0.width = static_cast<float>(RendererContext.SwapChain.Extent.width);
        Viewport0.height = static_cast<float>(RendererContext.SwapChain.Extent.height);
        Viewport0.minDepth = 0.0f;
        Viewport0.maxDepth = 1.0f;

        VkRect2D Scissor0{};
        Scissor0.offset = { static_cast<int32_t>(RendererContext.SwapChain.Extent.width * 0.5),0 };
        Scissor0.extent = { RendererContext.SwapChain.Extent.width / 2,RendererContext.SwapChain.Extent.height };

        VkViewport Viewport1{};
        Viewport1.x = 0.0f;
        Viewport1.y = 0.0f;
        Viewport1.width = static_cast<float>(RendererContext.SwapChain.Extent.width);
        Viewport1.height = static_cast<float>(RendererContext.SwapChain.Extent.height);
        Viewport1.minDepth = 0.0f;
        Viewport1.maxDepth = 1.0f;

        VkRect2D Scissor1{};
        Scissor1.offset = { 0,0 };
        Scissor1.extent = { RendererContext.SwapChain.Extent.width ,RendererContext.SwapChain.Extent.height };

        SCENE::CameraFreeModeInfo ModeInfo;
        //ModeInfo.KeyBindings.BackKey = GLFW_KEY_W;
        //ModeInfo.KeyBindings.ForwardKey = GLFW_KEY_S;

        SCENE::CameraSettingsInfo CameraSettings{};
        CameraSettings.Mode = SCENE::CAMERA_MODE_FREE_CAMERA;
        CameraSettings.CameraModeInfo = &ModeInfo;
        SCENE::Camera3D Camera(CameraSettings);

        RENDERER::RenderPassConfiguration PassConfiguration0{};
        PassConfiguration0.Name = "DeferredPass";
        PassConfiguration0.Pipeline = &DeferredPipelineNormal;
        PassConfiguration0.Scene = &Scene0;
        PassConfiguration0.EnableDepthTesting = true;
        PassConfiguration0.Camera = &Camera;
        PassConfiguration0.Scissor = (Scissor1);
        PassConfiguration0.Viewport = (Viewport0);
        Renderer.AddRenderPass(PassConfiguration0);

        SCENE::Camera3D Camera1;
        Camera1.Create(CameraSettings);

        RENDERER::RenderPassConfiguration PassConfiguration1{};
        PassConfiguration1.Name = "DeferredPass1";
        PassConfiguration1.Pipeline = &DeferredPipelineNormal;
        PassConfiguration1.Scene = &Scene1;
        PassConfiguration1.EnableDepthTesting = true;
        PassConfiguration1.Scissor = (Scissor1);
        PassConfiguration1.Viewport = (Viewport1);
        PassConfiguration1.Camera = &Camera1;
        Renderer.AddRenderPass(PassConfiguration1);

        float DeltaTime = 0.0f;
        float LastFrame = 0.0f;

        float CurrentSpriteFrame = 0.0f;
        while (!glfwWindowShouldClose(RendererContext.Window.Handle))
        {
            float CurrentTime = glfwGetTime();
            DeltaTime = CurrentTime - LastFrame;
            LastFrame = CurrentTime;

            //std::cout << "Delta time: " << DeltaTime << std::endl;

            /*
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
          */
          //Camera.AllowMove = AllowMove;
          //PhyContext.DynamicsWorld->debugDrawWorld();
          //PhyContext.DynamicsWorld->debugDrawObject(GroundTransform, StaticMeshShape.get(), { 1.0f,0.0f,0.0f });
            Light2.SetDirection(glm::vec4(1.0f * glm::cos(glfwGetTime()), 0.4f, 1.0f * glm::sin(glfwGetTime()), 0.0f));
            Scene0.MarkResourceChanged(&Light2, SCENE::MARK_CHANGED_TYPE_DYNAMIC_LIGHT, Renderer.CurrentFrame);
            //Scene0.UpdateDynamicFrameLightBuffers(Renderer.CurrentFrame);

            //Shovel.Transformations.RotationMatrix = glm::rotate(glm::mat4(1.0f), 10 * (float)glm::max(0.0,glm::cos(glfwGetTime())), glm::vec3(0.0, 1.0, 0.0));
            Shovel.GetTransformations()->Rotate((float)glm::max(0.0, glm::cos(glfwGetTime())), glm::vec3(0.0, 1.0, 0.0));
            Shovel.GetMaterial(0)->TextureSamplePosition = glm::vec2(glm::cos(CurrentTime), glm::sin(CurrentTime));

            SCENE::DoSpriteAnimation(QuadInstance0.GetMaterial(0)->TextureSampleSize,
                QuadInstance0.GetMaterial(0)->TextureSamplePosition, CurrentSpriteFrame, DeltaTime, 17.0f, 2, 3);

            Scene0.MarkResourceChanged(&Shovel, SCENE::MARK_CHANGED_TYPE_MESH_TRANSFORMATION | SCENE::MARK_CHANGED_TYPE_MESH_MATERIAL, Renderer.CurrentFrame);
            Scene0.MarkResourceChanged(&QuadInstance0, SCENE::MARK_CHANGED_TYPE_MESH_MATERIAL, Renderer.CurrentFrame);

            /*
            Scene0.FlushPendingUpdates(
                SCENE::SCENE_UPDATE_TYPE_UPDATE_MESH_TRANSFORMATIONS |
                SCENE::SCENE_UPDATE_TYPE_UPDATE_DYNAMIC_LIGHT_BUFFERS |
                SCENE::SCENE_UPDATE_TYPE_UPDATE_MESH_MATERIALS,
                Renderer.CurrentFrame
            );
            */

            Camera.Update(
                RendererContext.Window,
                50.0f,
                DeltaTime,
                { RendererContext.SwapChain.Extent.width,RendererContext.SwapChain.Extent.height },
                0.1f,
                2000.0f,
                45.0f
            );

            Camera1.CameraDirection = -Camera.CameraDirection;

           // auto Stats = RendererContext.QueryMemoryStats();
           // std::cout << "Memory usage is " << Stats.TotalUsedBytes / (1024.0f * 1024.0f) << "/" << Stats.TotalBudgetBytes / (1024.0f * 1024.0f) << "(" << Stats.UsageRate << "%)." << std::endl;

            Renderer.RenderFrame();
            glfwPollEvents();
            //PhysicsDebugDrawer.ClearDebugBuffers();
        }

        COMMON::DestroyResources(RendererContext);
        glfwTerminate();
    }
    catch (const std::exception& e)
    {
        std::cout << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}