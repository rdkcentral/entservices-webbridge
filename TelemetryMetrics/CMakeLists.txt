# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2025 RDK Management
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

set(PLUGIN_NAME TelemetryMetrics)
set(MODULE_NAME ${NAMESPACE}${PLUGIN_NAME})
set(PLUGIN_IMPLEMENTATION ${MODULE_NAME}Implementation)

set(PLUGIN_TELEMETRY_METRICS_MODE "Off" CACHE STRING "Controls if the plugin should run in its own process, in process or remote")
set(PLUGIN_TELEMETRY_METRICS_AUTOSTART false CACHE STRING "Automatically start TelemetryMetrics plugin")
set(PLUGIN_TELEMETRY_METRICS_EXTRA_LIBRARIES "")
set(PLUGIN_TELEMETRY_METRICS_STARTUPORDER "" CACHE STRING "Automatically start TelemetryMetrics plugin")

add_definitions(-DTELEMETRY_METRICS_API_VERSION_NUMBER_MAJOR=1)
add_definitions(-DTELEMETRY_METRICS_API_VERSION_NUMBER_MINOR=0)
add_definitions(-DTELEMETRY_METRICS_API_VERSION_NUMBER_PATCH=0)

set(TELEMETRY_METRICS_IMPLEMENTATION ${PLUGIN_NAME}Implementation)
add_definitions(-DPLUGIN_TELEMETRY_METRICS_IMPLEMENTATION_NAME="${TELEMETRY_METRICS_IMPLEMENTATION}")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

find_package(${NAMESPACE}Plugins REQUIRED)
find_package(${NAMESPACE}Definitions REQUIRED)
find_package(CompileSettingsDebug CONFIG REQUIRED)

add_library(${MODULE_NAME} SHARED
        TelemetryMetrics.cpp
        Module.cpp
)

set_target_properties(${MODULE_NAME} PROPERTIES
        CXX_STANDARD 11
        CXX_STANDARD_REQUIRED YES)

set_source_files_properties(TelemetryMetrics.cpp PROPERTIES COMPILE_FLAGS "-fexceptions")

target_link_libraries(${MODULE_NAME}
    PRIVATE
        CompileSettingsDebug::CompileSettingsDebug
        ${NAMESPACE}Plugins::${NAMESPACE}Plugins
        ${NAMESPACE}Definitions::${NAMESPACE}Definitions)

install(TARGETS ${MODULE_NAME}
    DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/${STORAGE_DIRECTORY}/plugins)

add_library(${PLUGIN_IMPLEMENTATION} SHARED
    TelemetryMetricsImplementation.cpp
    Module.cpp)

include_directories(../helpers)

set_target_properties(${PLUGIN_IMPLEMENTATION} PROPERTIES
        CXX_STANDARD 11
        CXX_STANDARD_REQUIRED YES)

set(TELEMETRY_METRICS_INCLUDES $ENV{TELEMETRY_METRICS_INCLUDES})
separate_arguments(TELEMETRY_METRICS_INCLUDES)
include_directories(BEFORE ${TELEMETRY_METRICS_INCLUDES})

target_link_libraries(${PLUGIN_IMPLEMENTATION} PRIVATE
    CompileSettingsDebug::CompileSettingsDebug ${NAMESPACE}Plugins::${NAMESPACE}Plugins  ${PLUGIN_TELEMETRY_METRICS_EXTRA_LIBRARIES})

install(TARGETS ${PLUGIN_IMPLEMENTATION}
    DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/${STORAGE_DIRECTORY}/plugins)

write_config(${PLUGIN_NAME})
