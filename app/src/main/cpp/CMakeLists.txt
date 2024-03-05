#[[
 Copyright (C) 2022 The Android Open Source Project

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
#]]

cmake_minimum_required(VERSION 3.18.1)
project(hellovkjni)

set(KTX_DIR external/ktx)
set(KTX_SOURCES
        ${KTX_DIR}/lib/texture.c
        ${KTX_DIR}/lib/hashlist.c
        ${KTX_DIR}/lib/checkheader.c
        ${KTX_DIR}/lib/swap.c
        ${KTX_DIR}/lib/memstream.c
        ${KTX_DIR}/lib/filestream.c
)
set(KTX_INCLUDE
        ${KTX_DIR}/include
        ${KTX_DIR}/lib
        ${KTX_DIR}/other_include
)

add_library(libktx ${KTX_SOURCES})
target_include_directories(libktx PUBLIC ${KTX_INCLUDE})
set_property(TARGET libktx PROPERTY FOLDER "external")

#include_directories(external/ktx/include)
#include_directories(external/ktx/other_include)

# Include the GameActivity static lib to the project.
find_package(game-activity REQUIRED CONFIG)
set(CMAKE_SHARED_LINKER_FLAGS
    "${CMAKE_SHARED_LINKER_FLAGS} -u \
    Java_com_google_androidgamesdk_GameActivity_initializeNativeCode")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

file(GLOB BASE_SRC "base/*.cpp")
add_library(libbase SHARED ${BASE_SRC})
include_directories(${BASE_DIR})
add_subdirectory(base)

# Now build app's shared lib
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -Wall")

add_definitions(-DVK_USE_PLATFORM_ANDROID_KHR=1)

add_library(${PROJECT_NAME} SHARED
    #vk_main.cpp
    texture.cpp
)

# add lib dependencies
target_link_libraries(${PROJECT_NAME} PUBLIC
    vulkan
    game-activity::game-activity_static
    android
    log
    libbase
    libktx
)
