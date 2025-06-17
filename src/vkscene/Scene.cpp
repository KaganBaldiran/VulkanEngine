#include "Scene.hpp"
#include "Mesh.hpp"
#include "../vkcore/VulkanBuffer.hpp"
#include "Camera.hpp"
#include "Mesh.hpp"

void VKSCENE::MeshImporter::AppendImportTask(ModelImportInfo ImportInfo)
{
    ImportQueue.push(ImportInfo);
}

void VKSCENE::MeshImporter::SubmitImport()
{
    StartingTime = glfwGetTime();
    while (!ImportQueue.empty())
    {
        auto& Import = std::move(ImportQueue.front());
        Futures.push_back(std::async(std::launch::async, VKSCENE::Import3Dmodel, Import.ModelFilePath, std::ref(*Import.DestinationModel)));
        ImportQueue.pop();
    }
}

void VKSCENE::MeshImporter::WaitImportIdle()
{
    for (auto& future : Futures)
    {
        future.get();
    }
    Futures.clear();

    double DeltaTime = glfwGetTime() - StartingTime;
    std::cout << "Models were imported in: " << DeltaTime << " seconds" << std::endl;
}
