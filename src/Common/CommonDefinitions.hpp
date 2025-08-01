#pragma once

#ifndef VKCORE_GLOBAL_PREFERENCES_COMPILE_SHADERS 
#define VKCORE_GLOBAL_PREFERENCES_COMPILE_SHADERS true
#endif // !VKCORE_GLOBAL_PREFERENCES_COMPILE_SHADERS 

//Max count of frames in flight
constexpr int MAX_FRAMES_IN_FLIGHT = 3;

//Frame indexes
constexpr uint32_t FRAME_INDEX_ALL_FRAMES = std::numeric_limits<uint32_t>::max();
constexpr uint32_t FRAME_INDEX_0 = 0;
constexpr uint32_t FRAME_INDEX_1 = 1;
constexpr uint32_t FRAME_INDEX_2 = 2;
constexpr uint32_t FRAME_INDEX_3 = 3;

#define GLOBAL_LOG_FILE_PATH "Log.txt" 