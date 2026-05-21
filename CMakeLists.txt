cmake_minimum_required(VERSION 3.24)

set(PROJECT_NAME VulkanEngine)

project(${PROJECT_NAME} VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

file(GLOB_RECURSE SRC src/*.h src/*.hpp src/*.cpp src/Shaders/* include/Imgui/*)
add_executable(${PROJECT_NAME} src/Main.cpp ${SRC})

find_path(GLFW_DIRECT_INCLUDE_DIR NAMES glfw3.h PATH_SUFFIXES GLFW)

target_include_directories(${PROJECT_NAME} PRIVATE include/glm src include/Imgui ${GLFW_DIRECT_INCLUDE_DIR}) 
target_compile_definitions(${PROJECT_NAME} PRIVATE IMGUI_DEFINE_MATH_OPERATORS)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SRC})

if(WIN32)
    set_target_properties(
        ${PROJECT_NAME} PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "$(SolutionDir)..\\src")

    set_target_properties(${PROJECT_NAME} PROPERTIES
        VS_DEBUGGER_ENVIRONMENT "PATH=$(SolutionDir)..\\dlls;%PATH%"
    )   
endif()

find_package(Vulkan COMPONENTS shaderc_combined REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(assimp CONFIG REQUIRED)
find_package(Bullet CONFIG REQUIRED)
find_package(freeimage CONFIG REQUIRED)

target_link_libraries(${PROJECT_NAME} PRIVATE 
    Vulkan::Vulkan
    Vulkan::shaderc_combined
    glfw
    assimp::assimp
    freeimage::FreeImage
    BulletCollision
    BulletDynamics
    LinearMath
)