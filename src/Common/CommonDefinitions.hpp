#pragma once

//Compile time application settings
//{
#ifndef VKCORE_GLOBAL_PREFERENCES_COMPILE_SHADERS 
#define VKCORE_GLOBAL_PREFERENCES_COMPILE_SHADERS true
#endif // !VKCORE_GLOBAL_PREFERENCES_COMPILE_SHADERS 

#ifndef GLOBAL_PREFERENCES_HIGH_PRECISION_POSITIONS 
#define GLOBAL_PREFERENCES_HIGH_PRECISION_POSITIONS false
#endif // !GLOBAL_PREFERENCES_HIGH_PRECISION_POSITIONS 

//Max count of frames in flight
constexpr int MAX_FRAMES_IN_FLIGHT = 2;
//Internal texture descriptor growth step 
constexpr size_t TextureDescriptorBlockSize = 250;
//Initial texture descriptor limit used during initial descriptor creation
constexpr size_t InitialTextureDescriptorSize = 1000;
//The buffer size for geometry buffer page allocation.
//Geometry buffers are used to store the mesh data (vertex,index) in the centralized resource managers.
//In order to keep the ray tracing acceleration structures intact (which are closely tided to the mesh buffers) geometry buffers
//use a page growth system where each page is a separate buffer. In this growth model, a new page is allocated whenever the existing ones are full and can't accommodate
//any more data.
constexpr size_t GeometryBufferPageSize = 1024 * 1024 * 64; 

#ifndef GLOBAL_LOG_FILE_PATH 
#define GLOBAL_LOG_FILE_PATH "Log.txt" 
#endif
//}

//Frame indexes
constexpr uint32_t FRAME_INDEX_ALL_FRAMES = std::numeric_limits<uint32_t>::max();
constexpr uint32_t FRAME_INDEX_0 = 0;
constexpr uint32_t FRAME_INDEX_1 = 1;
constexpr uint32_t FRAME_INDEX_2 = 2;
constexpr uint32_t FRAME_INDEX_3 = 3;
