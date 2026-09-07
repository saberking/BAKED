# DISTRHO Plugin Framework (DPF)
# Copyright (C) 2021 Jean Pierre Cimalando <jp-dev@inbox.ru>
# Copyright (C) 2021-2025 Filipe Coelho <falktx@falktx.com>
# SPDX-License-Identifier: ISC

cmake_minimum_required(VERSION 3.7...3.31)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_BUILD_TYPE Debug)
set(NAME Baked)

if(NOT CMAKE_TOOLCHAIN_FILE AND DEFINED ENV{CMAKE_TOOLCHAIN_FILE})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{CMAKE_TOOLCHAIN_FILE}" CACHE FILEPATH "Toolchain file")
endif()
project(${NAME})
execute_process(
    COMMAND git apply "${CMAKE_CURRENT_SOURCE_DIR}/dpf_changes.patch"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/dpf"
    ERROR_QUIET
)

add_subdirectory(dpf)

list(APPEND TARGETS_LIST "clap")
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    list(APPEND TARGETS_LIST "jack")
endif()

dpf_add_plugin(${NAME}
  TARGETS ${TARGETS_LIST}
  FILES_DSP
      src/PluginDSP.cpp
  FILES_UI
      src/PluginUI.cpp
      src/external/implot.cpp
      src/external/implot_items.cpp
      dpf-widgets/opengl/DearImGui.cpp)

target_sources(Baked
  PRIVATE
    src/Parameters.hpp
    src/DragAndDrop.hpp
    src/WinConsoleOutput.hpp
    src/Editor.hpp
    src/AudioData.hpp

    src/PluginDSP.hpp
    src/Defines.hpp
)

target_include_directories(${NAME} PUBLIC src)
target_include_directories(${NAME} PUBLIC dpf-widgets/generic)
target_include_directories(${NAME} PUBLIC dpf-widgets/opengl)
target_include_directories(${NAME} PUBLIC dpf-widgets/opengl/DearImGui)
add_compile_definitions(ImDrawIdx=unsigned\ int)
