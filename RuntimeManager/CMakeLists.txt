# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2024 RDK Management
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

set(PLUGIN_NAME RuntimeManager)
set(MODULE_NAME ${NAMESPACE}${PLUGIN_NAME})
set(PLUGIN_IMPLEMENTATION ${MODULE_NAME}Implementation)

set(PLUGIN_RUNTIME_MANAGER_MODE "Off" CACHE STRING "Controls if the plugin should run in its own process, in process or remote")
set(PLUGIN_RUNTIME_MANAGER_AUTOSTART false CACHE STRING "Automatically start RuntimeManager plugin")
set(PLUGIN_RUNTIME_MANAGER_EXTRA_LIBRARIES "")
set(PLUGIN_RUNTIME_MANAGER_STARTUPORDER "" CACHE STRING "Automatically start RuntimeManager plugin")
set(PLUGIN_RUNTIME_APP_PORTAL "" CACHE STRING "Runtime application portal identifier")

option(AIMANAGERS_TELEMETRY_METRICS_SUPPORT "AIMANAGERS_TELEMETRY_METRICS_SUPPORT" OFF)

add_definitions(-DRUNTIME_MANAGER_API_VERSION_NUMBER_MAJOR=1)
add_definitions(-DRUNTIME_MANAGER_API_VERSION_NUMBER_MINOR=0)
add_definitions(-DRUNTIME_MANAGER_API_VERSION_NUMBER_PATCH=0)

set(RUNTIME_MANAGER_IMPLEMENTATION ${PLUGIN_NAME}Implementation)
add_definitions(-DPLUGIN_RUNTIME_MANAGER_IMPLEMENTATION_NAME="${RUNTIME_MANAGER_IMPLEMENTATION}")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

find_package(${NAMESPACE}Plugins REQUIRED)
find_package(${NAMESPACE}Definitions REQUIRED)
find_package(CompileSettingsDebug CONFIG REQUIRED)

add_library(${MODULE_NAME} SHARED
        RuntimeManager.cpp
        Module.cpp
)

set_target_properties(${MODULE_NAME} PROPERTIES
        CXX_STANDARD 11
        CXX_STANDARD_REQUIRED YES)

set_source_files_properties(RuntimeManager.cpp PROPERTIES COMPILE_FLAGS "-fexceptions")

target_link_libraries(${MODULE_NAME}
    PRIVATE
        CompileSettingsDebug::CompileSettingsDebug
        ${NAMESPACE}Plugins::${NAMESPACE}Plugins
        ${NAMESPACE}Definitions::${NAMESPACE}Definitions)

install(TARGETS ${MODULE_NAME}
    DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/${STORAGE_DIRECTORY}/plugins)

set (RUNTIMEMANAGER_SOURCES)
list(APPEND RUNTIMEMANAGER_SOURCES  RuntimeManagerImplementation.cpp)
list(APPEND RUNTIMEMANAGER_SOURCES  DobbySpecGenerator.cpp)
list(APPEND RUNTIMEMANAGER_SOURCES  WindowManagerConnector.cpp)
list(APPEND RUNTIMEMANAGER_SOURCES  DobbyEventListener.cpp)
list(APPEND RUNTIMEMANAGER_SOURCES  UserIdManager.cpp)
list(APPEND RUNTIMEMANAGER_SOURCES  AIConfiguration.cpp)
list(APPEND RUNTIMEMANAGER_SOURCES  Module.cpp)

if (RIALTO_IN_DAC_FEATURE)
  add_definitions("-DRIALTO_IN_DAC_FEATURE_ENABLED")
  list(APPEND RUNTIMEMANAGER_SOURCES  RialtoConnector.cpp)
endif(RIALTO_IN_DAC_FEATURE)


add_library(${PLUGIN_IMPLEMENTATION} SHARED ${RUNTIMEMANAGER_SOURCES})

if (RIALTO_IN_DAC_FEATURE)
target_link_libraries(${PLUGIN_IMPLEMENTATION} PRIVATE ${NAMESPACE}Plugins::${NAMESPACE}Plugins RialtoServerManager)
endif(RIALTO_IN_DAC_FEATURE)

if(AIMANAGERS_TELEMETRY_METRICS_SUPPORT)
    message(STATUS "AIMANAGERS_TELEMETRY_METRICS_SUPPORT is enabled")
    add_definitions(-DENABLE_AIMANAGERS_TELEMETRY_METRICS)
endif(AIMANAGERS_TELEMETRY_METRICS_SUPPORT)

if(RDK_APPMANAGERS_DEBUG)
    message(STATUS "Enable APP Manager debug support")
    add_definitions("-DRDK_APPMANAGERS_DEBUG")
endif()

include_directories(../helpers)

set_target_properties(${PLUGIN_IMPLEMENTATION} PROPERTIES
        CXX_STANDARD 11
        CXX_STANDARD_REQUIRED YES)

set( AI_BUILD_TYPE "DEBUG" )
add_definitions( -DAI_DEBUG=1 -DAI_RELEASE=2 )
if( AI_BUILD_TYPE STREQUAL "RELEASE" )
    add_definitions( -DAI_BUILD_TYPE=AI_RELEASE )
elseif( AI_BUILD_TYPE STREQUAL "DEBUG" )
    add_definitions( -DAI_BUILD_TYPE=AI_DEBUG )
else()
    message( FATAL_ERROR "AI_BUILD_TYPE '${AI_BUILD_TYPE}' not supported" )
endif()

set(RUNTIME_MANAGER_INCLUDES $ENV{RUNTIME_MANAGER_INCLUDES})
separate_arguments(RUNTIME_MANAGER_INCLUDES)
include_directories(BEFORE ${RUNTIME_MANAGER_INCLUDES})

target_link_libraries(${PLUGIN_IMPLEMENTATION} PRIVATE
    CompileSettingsDebug::CompileSettingsDebug ${NAMESPACE}Plugins::${NAMESPACE}Plugins  
    ${PLUGIN_RUNTIME_MANAGER_EXTRA_LIBRARIES})

if(RDK_SERVICES_L1_TEST)
    set(JSONCPP_INCLUDE_PATH "/usr/include/jsoncpp" CACHE PATH "Path to jsoncpp includes")
    include_directories(BEFORE ${JSONCPP_INCLUDE_PATH})
    target_link_libraries(${PLUGIN_IMPLEMENTATION} PRIVATE jsoncpp curl)
endif()

install(TARGETS ${PLUGIN_IMPLEMENTATION}
    DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/${STORAGE_DIRECTORY}/plugins)

write_config(${PLUGIN_NAME})
