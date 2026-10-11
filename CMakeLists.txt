cmake_minimum_required(VERSION 3.24)

project(
    NPCStudio
    VERSION 0.1.0
    DESCRIPTION "NPC Studio - Full NPC Editor for Skyrim SE"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/utf-8 /W4 /WX- /permissive-)
    add_definitions(-DUNICODE -D_UNICODE)
endif()

include(FetchContent)

# =============================================================================
# Dependencies
# =============================================================================

set(BUILD_TESTS OFF CACHE BOOL "Build CommonLibSSE tests" FORCE)
set(ENABLE_SKYRIM_SE ON CACHE BOOL "Build Skyrim SE support" FORCE)
set(ENABLE_SKYRIM_AE ON CACHE BOOL "Build Skyrim AE support" FORCE)
set(ENABLE_SKYRIM_VR OFF CACHE BOOL "Build Skyrim VR support" FORCE)

message(STATUS "Fetching CommonLibSSE-NG...")
FetchContent_Declare(
    CommonLibSSE
    GIT_REPOSITORY https://github.com/CharmedBaryon/CommonLibSSE-NG.git
    GIT_TAG main
)
FetchContent_MakeAvailable(CommonLibSSE)

include("${commonlibsse_SOURCE_DIR}/cmake/CommonLibSSE.cmake")

find_package(spdlog CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

# nifly - NIF file parser for Skyrim
message(STATUS "Fetching nifly...")
FetchContent_Declare(
    nifly
    GIT_REPOSITORY https://github.com/ousnius/nifly.git
    GIT_TAG main
)
set(NIFLY_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(nifly)

# =============================================================================
# Source Files
# =============================================================================

set(SOURCES
    src/plugin.cpp
    src/api/SkyrimNetBridge.cpp
    src/api/PrismUIBridge.cpp
    src/input/InputHandler.cpp
    src/npc/NPCFactory.cpp
    src/npc/AppearanceManager.cpp
    src/npc/InventoryManager.cpp
    src/ui/UIManager.cpp
    src/preset/PresetManager.cpp
    src/bio/BioGenerator.cpp
    src/naming/NameDistributor.cpp
    src/nifviewer/NifViewerService.cpp
    src/nifviewer/NifModelLoader.cpp
    src/nifviewer/NifRenderBackend.cpp
    src/nifviewer/NifRenderTarget.cpp
    src/nifviewer/NifFrameTransport.cpp
    src/nifviewer/NifViewerBridge.cpp
)

# =============================================================================
# Target
# =============================================================================

add_commonlibsse_plugin(
    ${PROJECT_NAME}
    AUTHOR "Selenter"
    USE_ADDRESS_LIBRARY
    SOURCES ${SOURCES}
)

target_compile_definitions(${PROJECT_NAME} PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)

target_include_directories(
    ${PROJECT_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(
    ${PROJECT_NAME}
    PRIVATE
        spdlog::spdlog
        nlohmann_json::nlohmann_json
        nifly
        d3d11.lib
        dxgi.lib
        d3dcompiler.lib
)

target_precompile_headers(
    ${PROJECT_NAME}
    PRIVATE
        src/PCH.h
)

# =============================================================================
# Output Configuration
# =============================================================================

if(DEFINED ENV{SKYRIM_FOLDER})
    set(OUTPUT_FOLDER "$ENV{SKYRIM_FOLDER}/Data/SKSE/Plugins")
elseif(DEFINED ENV{SKYRIM_MODS_FOLDER})
    set(OUTPUT_FOLDER "$ENV{SKYRIM_MODS_FOLDER}/NPCStudio/SKSE/Plugins")
endif()

if(DEFINED OUTPUT_FOLDER)
    add_custom_command(
        TARGET ${PROJECT_NAME}
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:${PROJECT_NAME}>
            "${OUTPUT_FOLDER}/$<TARGET_FILE_NAME:${PROJECT_NAME}>"
        COMMENT "Copying ${PROJECT_NAME} to ${OUTPUT_FOLDER}"
    )
endif()