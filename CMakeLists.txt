cmake_minimum_required(VERSION 3.24)
project(NPCStudio VERSION 0.6.1 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
include(FetchContent)
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ENABLE_SKYRIM_SE OFF CACHE BOOL "" FORCE)
set(ENABLE_SKYRIM_AE ON CACHE BOOL "" FORCE)
set(ENABLE_SKYRIM_VR OFF CACHE BOOL "" FORCE)
FetchContent_Declare(CommonLibSSE
    GIT_REPOSITORY https://github.com/alandtse/CommonLibVR.git
    GIT_TAG 8eaddedd1542290701ccb0e9e88b2af8b3cea597
    GIT_SUBMODULES ""
)
FetchContent_MakeAvailable(CommonLibSSE)
include("${commonlibsse_SOURCE_DIR}/cmake/CommonLibSSE.cmake")
find_package(nlohmann_json CONFIG REQUIRED)
add_commonlibsse_plugin(NPCStudio AUTHOR "NPCStudio" USE_ADDRESS_LIBRARY SOURCES
    src/plugin.cpp
    src/core/StudioService.cpp
    src/core/EquipmentService.cpp
    src/core/NameCompatibility.cpp
    src/core/RaceService.cpp
    src/preview/ActorPreview.cpp
    src/ui/MagelightBridge.cpp
)
target_compile_definitions(NPCStudio PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_compile_options(NPCStudio PRIVATE /utf-8 /W4 /permissive- /Zc:preprocessor)
target_include_directories(NPCStudio PRIVATE include src)
target_link_libraries(NPCStudio PRIVATE nlohmann_json::nlohmann_json bcrypt)
target_precompile_headers(NPCStudio PRIVATE src/PCH.h)
enable_testing()
add_executable(NPCStudioStateTests tests/StateTests.cpp)
target_include_directories(NPCStudioStateTests PRIVATE src)
target_link_libraries(NPCStudioStateTests PRIVATE nlohmann_json::nlohmann_json)
target_compile_options(NPCStudioStateTests PRIVATE /utf-8 /Zc:preprocessor)
add_test(NAME state-codec COMMAND NPCStudioStateTests)

add_executable(NPCStudioPreviewTests tests/PreviewTests.cpp)
target_include_directories(NPCStudioPreviewTests PRIVATE src)
target_compile_options(NPCStudioPreviewTests PRIVATE /utf-8 /W4)
add_test(NAME preview-contract COMMAND NPCStudioPreviewTests)
