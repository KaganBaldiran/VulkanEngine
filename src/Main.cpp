#include "Renderer/Renderer.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "../include/stbi/stb_image.h"

#include <btBulletDynamicsCommon.h>
#include <LinearMath/btVector3.h>
#include <LinearMath/btAlignedObjectArray.h>

#include "Physics/PhysicsContext.hpp"
#include "Common/MemoryArenaAllocator.hpp"
#include "Scene/ShadowMapManager.hpp"

#include <random>
#include <chrono>

void DoSpriteAnimation(
    glm::vec2 &DestinationSize, 
    glm::vec2& DestinationPosition,
    float &CurrentFrame,
    float DeltaTime,
    float Speed,
    uint32_t RowCount,
    uint32_t ColumnCount
)
{
    uint32_t TotalFrameCount = RowCount * ColumnCount;
    CurrentFrame = (CurrentFrame + Speed * DeltaTime);
    if (CurrentFrame >= TotalFrameCount)
    {
        CurrentFrame = 0.0f;
    }
    uint32_t Xposition = (uint32_t)CurrentFrame % ColumnCount;
    uint32_t Yposition = (uint32_t)CurrentFrame / ColumnCount;

    DestinationSize = glm::vec2(1.0f / ColumnCount, 1.0f / RowCount);
    DestinationPosition = DestinationSize * glm::vec2(Xposition, Yposition);
}

int main()
{
    try
    {
        if (!glfwInit())
        {
            throw std::runtime_error("Unable to initialize GLFW");
        }

        RENDERER::RendererContext RendererContext(true);
        RENDERER::Renderer Renderer(RendererContext, false);

        SCENE::ModelHandle SponzaModel;
        SCENE::ModelHandle ShovelModel;
        SCENE::ModelHandle SceneModel;
        SCENE::ModelHandle Quad;

        SCENE::ModelInstance Sponza(SponzaModel);
        SCENE::ModelInstance Shovel(ShovelModel);
        SCENE::ModelInstance Shovel1(ShovelModel);
        SCENE::ModelInstance SceneModelInstance(SceneModel);
        SCENE::ModelInstance QuadInstance0(Quad);

        std::vector<SCENE::ModelInstance> Shovels;

        std::mt19937 rng(std::random_device{}());
        std::uniform_real_distribution<float> distX(-1000.0f, 1000.0f);
        std::uniform_real_distribution<float> distY(10.0f, 30.0f);
        std::uniform_real_distribution<float> distZ(-1000.0f, 1000.0f);
        std::uniform_real_distribution<float> distRot(0.0f, glm::two_pi<float>());

        SCENE::Texture SomeTexture{};
        SCENE::Texture AnimationSprite{};

        SCENE::TextureManager TextureImportManager(RendererContext);
        SCENE::MeshManager Importer(TextureImportManager, RendererContext);
        Importer.AppendImportTask({ &SponzaModel , "Resources\\sponza.obj" });
        Importer.AppendImportTask({ &ShovelModel , "Resources\\shovel2.obj" });
        Importer.AppendImportTask({ &SceneModel , "C:\\Users\\kbald\\Desktop\\SunTemple\\SunTemple.fbx" });
        Importer.AppendImportTask({ &Quad , "Resources\\Quad.fbx" });
        Importer.SubmitImport();
        Importer.WaitImportIdle();
        
        TextureImportManager.AppendImportTask({ "C:\\Users\\kbald\\Downloads\\wallhaven-lmmeqq_2560x1080.png",SomeTexture.ResourceID });
        TextureImportManager.AppendImportTask({ "Resources\\SpriteAnimation3.jpg",AnimationSprite.ResourceID });
        TextureImportManager.SubmitImport();

        SCENE::ModelInstance SponzaTextured(SponzaModel);
        SCENE::Material SponzaOverride{};

        SponzaOverride.TextureSampleSize = glm::vec2(20);
        SponzaOverride.ReferenceTexture(SomeTexture.ResourceID, SCENE::MATERIAL_TEXTURE_TYPE_ALBEDO);
        Shovel.Materials.push_back(SponzaOverride);

        SponzaOverride.TextureSampleSize = glm::vec2(1);
        SponzaOverride.ReferenceTexture(AnimationSprite.ResourceID, SCENE::MATERIAL_TEXTURE_TYPE_ALBEDO);
        QuadInstance0.Materials.push_back(SponzaOverride);
        QuadInstance0.Transformations.TranslationMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f,300.0f,0.0f));
        QuadInstance0.Transformations.RotationMatrix = glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));

        SponzaTextured.Materials.push_back(SponzaOverride);

        SCENE::Material NewMaterial{};
        NewMaterial.Metallic = 0.7f;
        NewMaterial.Roughness = 0.5f;
        NewMaterial.Albedo = glm::vec4(0.2f, 0.2f, 0.2f, 1.0f);
        for (size_t i = 0; i < 1500; i++)
        {
            SCENE::ModelInstance NewShovel(ShovelModel);
            NewShovel.Materials.push_back(NewMaterial);

            glm::vec3 position(distX(rng), distY(rng), distZ(rng));
            float rotationAngle = distRot(rng);

            glm::mat4 transform = glm::translate(glm::mat4(1.0f), position);
            transform = glm::rotate(transform, rotationAngle, glm::vec3(0.0f, 1.0f, 0.0f));

            NewShovel.Transformations.TranslationMatrix = transform;
            NewShovel.Transformations.ScalingMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(4.0f));
            Shovels.push_back(NewShovel);
        }

        Sponza.Transformations.ScalingMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(0.1f));
        Sponza.Transformations.RotationMatrix = glm::rotate(glm::mat4(1.0f), glm::radians(45.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        Shovel.Transformations.ScalingMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(4.0f));

        SCENE::CameraFreeModeInfo ModeInfo;
        //ModeInfo.KeyBindings.BackKey = GLFW_KEY_W;
        //ModeInfo.KeyBindings.ForwardKey = GLFW_KEY_S;

        SCENE::CameraSettingsInfo CameraSettings{};
        CameraSettings.Mode = SCENE::CAMERA_MODE_FREE_CAMERA;
        CameraSettings.CameraModeInfo = &ModeInfo;
        SCENE::Camera3D Camera(RendererContext.Window, CameraSettings);

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

      

        SCENE::Scene Scene0;
        Scene0.Create(RendererContext, TextureImportManager, Importer);

        SCENE::Scene Scene1;
        Scene1.Create(RendererContext, TextureImportManager, Importer);

        Shovel1.Transformations.TranslationMatrix = glm::translate(glm::mat4(1.0f), { 30,100,40.0f });

        Scene0.LinkModelInstance(Sponza);
        Scene0.LinkModelInstance(SponzaTextured);
        Scene0.LinkModelInstance(QuadInstance0);

        Scene0.LinkModelInstance(Shovel);
        Scene0.LinkStaticLight(Light0);
        Scene0.LinkStaticLight(Light1);
        Scene0.LinkDynamicLight(Light2); 

        Scene1.LinkStaticLight(Light0);
        Scene1.LinkStaticLight(Light1);
        Scene1.LinkDynamicLight(Light2);

        //Scene0.LinkModelInstance(SceneModelInstance);
        //Scene0.LinkModelInstance(Shovel1);


        Scene0.FlushPendingUpdates(
            SCENE::SCENE_UPDATE_TYPE_ALL_PENDING,
            FRAME_INDEX_ALL_FRAMES
        );

       

        Scene0.FlushPendingUpdates(
            SCENE::SCENE_UPDATE_TYPE_ALL_PENDING,
            FRAME_INDEX_ALL_FRAMES
        );

        Scene1.LinkModelInstance(Shovel1);
        Scene1.FlushPendingUpdates(
            SCENE::SCENE_UPDATE_TYPE_ALL_PENDING,
            FRAME_INDEX_ALL_FRAMES
        );

        for (size_t i = 0; i < 100; i++)
        {
            Scene1.LinkModelInstance(Shovels[i]);
        }
        Scene1.LinkModelInstance(SceneModelInstance);

        Scene1.FlushPendingUpdates(
            SCENE::SCENE_UPDATE_TYPE_ALL_PENDING,
            FRAME_INDEX_ALL_FRAMES
        );

        Scene0.LinkCubemap(Cubemap0);
        Scene0.LinkCamera(Camera);

        Scene1.LinkCubemap(Cubemap0);
        Scene1.LinkCamera(Camera);


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
        std::string ShadeFunction = 
            "vec3 ShadePixel(in vec3 CameraPosition,in vec3 CameraDirection,in vec3 Normal, in vec3 Position, in vec3 Albedo, in float Roughness, in float Metallic, in float Time) \n\
            {  \n\
                \n\
                 return CalculateLighting(Normal,Position,Albedo,Roughness,Metallic);\n\
            }";
            

        RENDERER::DeferredRenderPipeline DeferredPipeline(RendererContext);
        DeferredPipeline.CompileCustomPipeline(ShadeFunction,"CustomShader0");
        RENDERER::DeferredRenderPipeline DeferredPipelineNormal(RendererContext);

        RENDERER::RenderPassConfiguration PassConfiguration0{};
        PassConfiguration0.Name = "DeferredPass";
        PassConfiguration0.Pipeline = &DeferredPipeline;
        PassConfiguration0.Scene = &Scene0;
        PassConfiguration0.EnableDepthTesting = true;
        Renderer.AddRenderPass(PassConfiguration0);

        RENDERER::RenderPassConfiguration PassConfiguration1{};
        PassConfiguration1.Name = "DeferredPass1";
        PassConfiguration1.Pipeline = &DeferredPipeline;
        PassConfiguration1.Scene = &Scene1;
        PassConfiguration1.EnableDepthTesting = true;
        Renderer.AddRenderPass(PassConfiguration1);

        float DeltaTime = 0.0f;
        float LastFrame = 0.0f;

        float CurrentSpriteFrame = 0.0f;
        while (!glfwWindowShouldClose(RendererContext.Window.window))
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
          
            Shovel.Transformations.RotationMatrix = glm::rotate(glm::mat4(1.0f), 10 * (float)glm::max(0.0,glm::cos(glfwGetTime())), glm::vec3(0.0, 1.0, 0.0));
            Shovel.Materials[0].TextureSamplePosition = glm::vec2(glm::cos(CurrentTime), glm::sin(CurrentTime));
            QuadInstance0.Materials[0].TextureSamplePosition = glm::vec2(glm::cos(CurrentTime), glm::sin(CurrentTime));

            DoSpriteAnimation(QuadInstance0.Materials[0].TextureSampleSize,
                QuadInstance0.Materials[0].TextureSamplePosition, CurrentSpriteFrame, DeltaTime,13.0f, 2, 3);

            Scene0.MarkResourceChanged(&Shovel, SCENE::MARK_CHANGED_TYPE_MESH_TRANSFORMATION | SCENE::MARK_CHANGED_TYPE_MESH_MATERIAL, Renderer.CurrentFrame);
            Scene0.MarkResourceChanged(&QuadInstance0, SCENE::MARK_CHANGED_TYPE_MESH_MATERIAL, Renderer.CurrentFrame);

            Scene0.FlushPendingUpdates(
                SCENE::SCENE_UPDATE_TYPE_UPDATE_MESH_TRANSFORMATIONS | 
                SCENE::SCENE_UPDATE_TYPE_UPDATE_DYNAMIC_LIGHT_BUFFERS | 
                SCENE::SCENE_UPDATE_TYPE_UPDATE_MESH_MATERIALS, 
                Renderer.CurrentFrame
            );

           Camera.Update(
                RendererContext.Window,
                50.0f,
                DeltaTime,
                { RendererContext.SwapChain.Extent.width,RendererContext.SwapChain.Extent.height }, 
                0.1f,
                2000.0f,
                45.0f
            );

      
            Renderer.RenderFrame();
            glfwPollEvents();

            //PhysicsDebugDrawer.ClearDebugBuffers();
        }

        RendererContext.WaitDeviceIdle();
        COMMON::DestructionQueue::Get()->Destroy();
        glfwTerminate();
    }
    catch (const std::exception& e)
    {
        std::cout << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}