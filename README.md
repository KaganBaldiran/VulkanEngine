![GitHub language count](https://img.shields.io/github/languages/count/KaganBaldiran/VulkanEngine)
![GitHub top language](https://img.shields.io/github/languages/top/KaganBaldiran/VulkanEngine) 
![GitHub last commit](https://img.shields.io/github/last-commit/KaganBaldiran/VulkanEngine)
[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)](https://opensource.org/licenses/MIT)
# VulkanEngine
Vulkan engine is a GPU driven real time PBR renderer powered by Vulkan graphics API written in c++. It's being developed with highly strict memory optimizations and ease of use in mind.
# Currently being worked-on
-Ray traced shadows
# Abilities
- Pbr rendering.
- HDRI support.
- Multi-indirect rendering.
- Chunk-by-chunk multi-frame resource loading/copying. 
- GPU frustum culling.
- Packed cascaded show maps.
- Native instancing control.
- Asynchronous asset loading.
# Current Look
<img width="1919" height="1028" alt="image" src="https://github.com/user-attachments/assets/2ee5ceb6-def8-4a95-bab6-9b032f52de60" />

# Building

The project uses CMake to build the files. 
if you are on linux you can build it using 
```shell
#Using APT
cmake --preset linux-apt
cmake --build --preset linux-apt
#Or using vcpkg
cmake --preset linux-vcpkg
cmake --build --preset linux-vcpkg
```
If you are on windows, you are required to install vcpkg to download the required libraries

```shell
git clone https://github.com/microsoft/vcpkg.git C:/vcpkg
cd C:/vcpkg
.\bootstrap-vcpkg.bat
```
Now you can proceed with generating and compiling the required files.
```shell
cmake --preset windows-vcpkg
cmake --build --preset windows-vcpkg
```

