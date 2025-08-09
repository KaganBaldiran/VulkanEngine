#pragma once

//Compile time application settings
//{
#ifndef VKCORE_GLOBAL_PREFERENCES_COMPILE_SHADERS 
#define VKCORE_GLOBAL_PREFERENCES_COMPILE_SHADERS false
#endif // !VKCORE_GLOBAL_PREFERENCES_COMPILE_SHADERS 

//Max count of frames in flight
constexpr int MAX_FRAMES_IN_FLIGHT = 4;
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

