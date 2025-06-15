cmake_minimum_required(VERSION 3.10)

set(PROJECT_NAME VulkanEngine)

project(${PROJECT_NAME} VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

file(GLOB_RECURSE SRC src/*.h src/*.hpp src/*.cpp src/Shaders/* include/Imgui/*)
add_executable(${PROJECT_NAME} src/Main.cpp ${SRC})
include_directories(include/glm src include/Imgui) 

add_compile_definitions(IMGUI_DEFINE_MATH_OPERATORS)

if(WIN32)
    foreach(FILE ${SRC}) 
        get_filename_component(PARENT_DIR "${FILE}" DIRECTORY)

        string(REPLACE "${CMAKE_CURRENT_SOURCE_DIR}" "" GROUP "${PARENT_DIR}")

        string(REPLACE "/" "\\" GROUP "${GROUP}")

        if ("${FILE}" MATCHES ".*\\.cpp")
        set(GROUP "Source Files${GROUP}")
        elseif("${FILE}" MATCHES ".*\\.h" OR "${FILE}" MATCHES ".*\\.hpp")
        set(GROUP "Header Files${GROUP}")
        endif()

        source_group("${GROUP}" FILES "${FILE}")
    endforeach()

    set_target_properties(
        ${PROJECT_NAME} PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "$(SolutionDir)..\\src")

    set_target_properties(${PROJECT_NAME} PROPERTIES
        VS_DEBUGGER_ENVIRONMENT "PATH=$(SolutionDir)..\\dlls;%PATH%"
    )    

    set(VULKAN_SDK_ROOT "C:/VulkanSDK") 
    file(GLOB VULKAN_VERSION_DIR "${VULKAN_SDK_ROOT}/*")

    set(BULLET_PHYSICS_SDK_ROOT "${CMAKE_SOURCE_DIR}/external/BulletPhysics") 

    foreach(PATH ${VULKAN_VERSION_DIR})
        if(IS_DIRECTORY ${PATH})
            set(VULKAN_SDK ${PATH})
            break()
        endif()
    endforeach()

    include_directories(
        external/freeimage/include/Source 
        external/VulkanMemoryAllocator/include external/assimp/include 
        external/glfw-3.3.7.bin.WIN64/glfw-3.3.7.bin.WIN64/include/GLFW 
        ${VULKAN_SDK}/Include
        ${BULLET_PHYSICS_SDK_ROOT}/include
    )
    target_link_libraries(${PROJECT_NAME} 
                                    ${CMAKE_SOURCE_DIR}/external/glfw-3.3.7.bin.WIN64/glfw-3.3.7.bin.WIN64/lib-vc2022/glfw3.lib 
                                    ${CMAKE_SOURCE_DIR}/external/freeimage/lib/release/FreeImage.lib
                                    ${CMAKE_SOURCE_DIR}/external/assimp/lib/assimp-vc143-mt.lib
                                    ${CMAKE_SOURCE_DIR}/external/VulkanMemoryAllocator/lib/VmaSample.lib
                                    ${VULKAN_SDK}/Lib/vulkan-1.lib

                                    debug ${BULLET_PHYSICS_SDK_ROOT}/lib/Debug/BulletCollision_debug.lib
                                    debug ${BULLET_PHYSICS_SDK_ROOT}/lib/Debug/BulletDynamics_debug.lib
                                    debug ${BULLET_PHYSICS_SDK_ROOT}/lib/Debug/LinearMath_debug.lib

                                    optimized ${BULLET_PHYSICS_SDK_ROOT}/lib/Release/BulletCollision.lib
                                    optimized ${BULLET_PHYSICS_SDK_ROOT}/lib/Release/BulletDynamics.lib
                                    optimized ${BULLET_PHYSICS_SDK_ROOT}/lib/Release/LinearMath.lib
                                    )

elseif(UNIX)
    find_package(OpenGL REQUIRED)
    find_package(GLEW REQUIRED)
    find_package(glfw3 REQUIRED)
    find_package(assimp REQUIRED)

    target_link_libraries(${PROJECT_NAME} PRIVATE
        ${OPENGL_LIBRARIES}
        ${GLEW_LIBRARIES}
        glfw
        assimp  
    )
endif()