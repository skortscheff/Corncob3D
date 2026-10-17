cmake_minimum_required(VERSION 3.15)
project(Corncob3D VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# SDL2
find_package(SDL2 REQUIRED)

# --- Original data segment, generated from the MASM sources at build time ---
# Modules in shareware link order (3S.LNK) with the MAKEFILE's per-module defines
add_executable(asmdata tools/asmdata/asmdata.cpp)
set(ASM_MODULES
    "3.ASM:shareflag=1,mbflag=0" "F3DVEC.ASM:shareflag=1,mbflag=0" "HLINE.ASM"
    "DEFOBJS.ASM:shareflag=1,mbflag=0" "KLINE.ASM" "EGALINE.ASM" "STRTEXP.ASM:shareflag=1"
    "INST.ASM" "MAT.ASM" "SIN.ASM" "CSQRT.ASM" "ATN.ASM" "DSQRT.ASM" "CMDLINE.ASM:shareflag=1"
    "MAP.ASM:shareflag=1,mbflag=0" "PICT.ASM" "ADL.ASM")
file(GLOB ASM_SOURCES ${CMAKE_SOURCE_DIR}/*.ASM ${CMAKE_SOURCE_DIR}/*.INC)
set(GEN_DIR ${CMAKE_BINARY_DIR}/generated)
add_custom_command(
    OUTPUT ${GEN_DIR}/DGroup.cpp ${GEN_DIR}/DGroup.hpp
    COMMAND ${CMAKE_COMMAND} -E make_directory ${GEN_DIR}
    COMMAND asmdata ${CMAKE_SOURCE_DIR} ${GEN_DIR}/DGroup.cpp ${GEN_DIR}/DGroup.hpp ${ASM_MODULES}
    DEPENDS asmdata ${ASM_SOURCES}
    COMMENT "Extracting DGROUP from the original ASM sources")
add_library(dgroup STATIC ${GEN_DIR}/DGroup.cpp)
target_include_directories(dgroup PUBLIC ${GEN_DIR})

add_executable(corncob3d
    src/main.cpp
    src/core/Fixed.cpp
    src/core/Vec3.cpp
    src/math/Mat3x3.cpp
    src/math/Trig.cpp
    src/math/Sqrt.cpp
    src/platform/Window.cpp
    src/platform/Input.cpp
    src/engine/AsmTables.cpp
    src/engine/F3dVec.cpp
    src/engine/F3dObj.cpp
    src/game/World.cpp
    src/game/Keyboard.cpp
    src/game/Inst.cpp
    src/game/Main3.cpp
    src/game/Main3Flight.cpp
    src/game/Main3Ground.cpp
    src/game/Main3Frame.cpp
    src/game/Main3Misc.cpp
    src/game/Main3Objects.cpp
    src/game/Main3Panel.cpp
    src/render/EgaScreen.cpp
    src/assets/Assets.cpp
    src/render/FrameBuffer.cpp
    src/ui/Intro.cpp
)

target_include_directories(corncob3d PRIVATE src/)
if(TARGET SDL2::SDL2main)
    target_link_libraries(corncob3d PRIVATE SDL2::SDL2main)
endif()
target_link_libraries(corncob3d PRIVATE SDL2::SDL2 dgroup)

# Unit tests for the bit-exact ASM ports
enable_testing()
add_executable(test_asmmath tests/test_asmmath.cpp)
target_include_directories(test_asmmath PRIVATE src/)
add_test(NAME asmmath COMMAND test_asmmath)
add_executable(test_dgroup tests/test_dgroup.cpp)
target_include_directories(test_dgroup PRIVATE src/)
target_link_libraries(test_dgroup PRIVATE dgroup)
add_test(NAME dgroup COMMAND test_dgroup)
add_executable(test_engine tests/test_engine.cpp
    src/engine/AsmTables.cpp src/engine/F3dVec.cpp src/engine/F3dObj.cpp src/render/EgaScreen.cpp)
target_include_directories(test_engine PRIVATE src/)
target_link_libraries(test_engine PRIVATE SDL2::SDL2 dgroup)
add_test(NAME engine COMMAND test_engine)
add_executable(test_world tests/test_world.cpp src/game/World.cpp
    src/engine/AsmTables.cpp src/engine/F3dVec.cpp src/engine/F3dObj.cpp src/render/EgaScreen.cpp)
target_include_directories(test_world PRIVATE src/)
target_compile_definitions(test_world PRIVATE CORNCOB_ASSET_DIR="${CMAKE_SOURCE_DIR}/assets/original")
target_link_libraries(test_world PRIVATE SDL2::SDL2 dgroup)
add_test(NAME world COMMAND test_world)
add_executable(test_flight tests/test_flight.cpp
    src/game/World.cpp src/game/Keyboard.cpp src/game/Main3.cpp src/game/Main3Flight.cpp
    src/game/Main3Ground.cpp src/game/Main3Frame.cpp src/game/Main3Misc.cpp src/game/Main3Objects.cpp
    src/game/Main3Panel.cpp src/game/Inst.cpp
    src/engine/AsmTables.cpp src/engine/F3dVec.cpp src/engine/F3dObj.cpp src/render/EgaScreen.cpp)
target_include_directories(test_flight PRIVATE src/)
target_compile_definitions(test_flight PRIVATE CORNCOB_ASSET_DIR="${CMAKE_SOURCE_DIR}/assets/original")
target_link_libraries(test_flight PRIVATE SDL2::SDL2 dgroup)
add_test(NAME flight COMMAND test_flight)

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
