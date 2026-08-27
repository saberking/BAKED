# DISTRHO Plugin Framework (DPF)
# Copyright (C) 2021 Jean Pierre Cimalando <jp-dev@inbox.ru>
# Copyright (C) 2021-2025 Filipe Coelho <falktx@falktx.com>
# SPDX-License-Identifier: ISC

cmake_minimum_required(VERSION 3.7...3.31)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_BUILD_TYPE Debug)

set(NAME Baked)

# Allow toolchain file to be specified via -DCMAKE_TOOLCHAIN_FILE=
if(NOT CMAKE_TOOLCHAIN_FILE AND DEFINED ENV{CMAKE_TOOLCHAIN_FILE})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{CMAKE_TOOLCHAIN_FILE}" CACHE FILEPATH "Toolchain file")
endif()
project(${NAME})


# add_compile_definitions(BOOST_USE_WINDOWS_H)
# add_compile_definitions(WIN32_LEAN_AND_MEAN)
add_subdirectory(dpf)

set(TARGETS_LIST "vst3")

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    list(APPEND TARGETS_LIST "jack")
endif()

dpf_add_plugin(${NAME}
  TARGETS ${TARGETS_LIST}
  FILES_DSP
      src/PluginDSP.cpp
  FILES_UI
      src/PluginUI.cpp

      dpf-widgets/opengl/DearImGui.cpp)

target_sources(Baked
  PRIVATE
    src/Parameters.hpp
)

target_include_directories(${NAME} PUBLIC src)
target_include_directories(${NAME} PUBLIC dpf-widgets/generic)
target_include_directories(${NAME} PUBLIC dpf-widgets/opengl)
target_include_directories(${NAME} PUBLIC dpf-widgets/opengl/DearImGui)

