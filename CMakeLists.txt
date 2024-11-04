cmake_minimum_required(VERSION 3.10)

# Set the project name and a version
project(BirdGame VERSION 1.0)

# Set C++ standard to cpp17 as it's supported on most platforms now
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Windows API primarily use UTF-16. Add the preprocessor definitions to support use of std::wstring with DirectX.
add_definitions(-D_UNICODE)
add_definitions(-DUNICODE)

# Add the source files that make up the exectuable.
# CMake recommends not using GLOB to add files.
# When a new file is added, the build files (vcxproj, Makefile etc) need to be regenerated.
# When using GLOB, there is no change to the CMakeLists.txt file, so the build files will not be regenerated.
set(BirdGame_SOURCES
    src/Main.cpp
    src/Application.cpp
    src/Renderer.cpp
    src/Window.cpp)

add_executable(BirdGame WIN32 ${BirdGame_SOURCES})

# Set up include directories.
target_include_directories(BirdGame PRIVATE include)

target_link_libraries(BirdGame d3d12.lib DXGI.lib d3dcompiler.lib)
