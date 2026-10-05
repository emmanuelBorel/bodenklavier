cmake_minimum_required(VERSION 3.16)

project(Bodenklavier)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --------------------------------------------------
# OpenNI2
# --------------------------------------------------

set(OPENNI_ROOT
    ${CMAKE_SOURCE_DIR}/OpenNI_2.3.0.86_202210111155_4c8f5aa4_beta6_a311d
)

include_directories(
    ${OPENNI_ROOT}/sdk/Include
)

link_directories(
    ${OPENNI_ROOT}/sdk/libs
)

# --------------------------------------------------
# OpenCV
# --------------------------------------------------

find_package(OpenCV REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
pkg_check_modules(SDL2_MIXER REQUIRED IMPORTED_TARGET SDL2_mixer)

include_directories(
    ${OpenCV_INCLUDE_DIRS}
)

# --------------------------------------------------
# Prototype actuel
# --------------------------------------------------

add_executable(
    bodenklavier_visual
    src_cpp/bodenklavier_visual.cpp
    src/piano/MatCalibration.cpp
    src/piano/PianoLayout.cpp
    src/piano/KeyStateManager.cpp
    src/audio/AudioEngine.cpp
    src/piano/NoteMapper.cpp

    src/processing/TouchDetector.cpp
    src/processing/ConflictResolver.cpp
)

target_include_directories(
    bodenklavier_visual
    PRIVATE
    ${CMAKE_SOURCE_DIR}/include


    ${SDL2_INCLUDE_DIRS}
    ${SDL2_MIXER_INCLUDE_DIRS}
)

target_link_libraries(
    bodenklavier_visual
    OpenNI2
    ${OpenCV_LIBS}
    ${SDL2_LIBRARIES}
    ${SDL2_MIXER_LIBRARIES}
)

add_executable(
    bodenklavier_depth
    src_cpp/bodenklavier_depth.cpp
)

target_link_libraries(
    bodenklavier_depth
    OpenNI2
)
