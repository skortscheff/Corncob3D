cmake_minimum_required(VERSION 3.15)
project(Corncob3D VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# SDL2
find_package(SDL2 REQUIRED)

add_executable(corncob3d
    src/main.cpp
    src/core/Fixed.cpp
    src/core/Vec3.cpp
    src/math/Mat3x3.cpp
    src/math/Trig.cpp
    src/math/Sqrt.cpp
    src/platform/Window.cpp
    src/platform/Input.cpp
    src/render/Renderer.cpp
    src/game/PlayerObjects.cpp
    src/assets/Assets.cpp
    src/render/FrameBuffer.cpp
    src/ui/Intro.cpp
)

target_include_directories(corncob3d PRIVATE src/)
if(TARGET SDL2::SDL2main)
    target_link_libraries(corncob3d PRIVATE SDL2::SDL2main)
endif()
target_link_libraries(corncob3d PRIVATE SDL2::SDL2)

# Unit tests for the bit-exact ASM ports
enable_testing()
add_executable(test_asmmath tests/test_asmmath.cpp)
target_include_directories(test_asmmath PRIVATE src/)
add_test(NAME asmmath COMMAND test_asmmath)

if(WIN32)
    # Windowed app (no console) in release; keep the console in debug for logging
    set_target_properties(corncob3d PROPERTIES WIN32_EXECUTABLE $<NOT:$<CONFIG:Debug>>)
    if(MINGW)
        # Don't depend on the MinGW runtime DLLs being on PATH
        target_link_options(corncob3d PRIVATE -static-libgcc -static-libstdc++ -static)
    endif()
    # Ship SDL2.dll next to the exe
    add_custom_command(TARGET corncob3d POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:SDL2::SDL2> $<TARGET_FILE_DIR:corncob3d>)
endif()
