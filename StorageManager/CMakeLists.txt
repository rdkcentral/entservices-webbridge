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

set(PLUGIN_NAME StorageManager)
set(MODULE_NAME ${NAMESPACE}${PLUGIN_NAME})
set(PLUGIN_IMPLEMENTATION ${MODULE_NAME}Implementation)

set(PLUGIN_STORAGE_MANAGER_MODE "Off" CACHE STRING "Controls if the plugin should run in its own process, in process or remote")
set(PLUGIN_STORAGE_MANAGER_AUTOSTART false CACHE STRING "Automatically start StorageManager plugin")
set(PLUGIN_STORAGE_MANAGER_EXTRA_LIBRARIES "")
set(PLUGIN_STORAGE_MANAGER_STARTUPORDER "" CACHE STRING "Automatically start StorageManager plugin")

option(AIMANAGERS_TELEMETRY_METRICS_SUPPORT "AIMANAGERS_TELEMETRY_METRICS_SUPPORT" OFF)

add_definitions(-DSTORAGE_MANAGER_API_VERSION_NUMBER_MAJOR=1)
add_definitions(-DSTORAGE_MANAGER_API_VERSION_NUMBER_MINOR=0)
add_definitions(-DSTORAGE_MANAGER_API_VERSION_NUMBER_PATCH=0)

set(STORAGE_MANAGER_IMPLEMENTATION ${PLUGIN_NAME}Implementation)
add_definitions(-DPLUGIN_STORAGE_MANAGER_IMPLEMENTATION_NAME="${STORAGE_MANAGER_IMPLEMENTATION}")
set(PLUGIN_STORAGE_MANAGER_PATH "/opt/persistent/storageManager" CACHE STRING "Path")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

find_package(${NAMESPACE}Plugins REQUIRED)
find_package(${NAMESPACE}Definitions REQUIRED)
find_package(CompileSettingsDebug CONFIG REQUIRED)

add_library(${MODULE_NAME} SHARED
        StorageManager.cpp
        Module.cpp
)

set_target_properties(${MODULE_NAME} PROPERTIES
        CXX_STANDARD 11
        CXX_STANDARD_REQUIRED YES)

set_source_files_properties(StorageManager.cpp PROPERTIES COMPILE_FLAGS "-fexceptions")

target_link_libraries(${MODULE_NAME}
    PRIVATE
        CompileSettingsDebug::CompileSettingsDebug
        ${NAMESPACE}Plugins::${NAMESPACE}Plugins
        ${NAMESPACE}Definitions::${NAMESPACE}Definitions)

install(TARGETS ${MODULE_NAME}
    DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/${STORAGE_DIRECTORY}/plugins)

add_library(${PLUGIN_IMPLEMENTATION} SHARED
    StorageManagerImplementation.cpp
    RequestHandler.cpp
    Module.cpp)

if(AIMANAGERS_TELEMETRY_METRICS_SUPPORT)
    message(STATUS "TELEMETRY_METRICS_SUPPORT is enabled")
    add_definitions(-DENABLE_AIMANAGERS_TELEMETRY_METRICS)
endif(AIMANAGERS_TELEMETRY_METRICS_SUPPORT)

include_directories(../helpers)

set_target_properties(${PLUGIN_IMPLEMENTATION} PROPERTIES
        CXX_STANDARD 11
        CXX_STANDARD_REQUIRED YES)

set(STORAGE_MANAGER_INCLUDES $ENV{STORAGE_MANAGER_INCLUDES})
separate_arguments(STORAGE_MANAGER_INCLUDES)
include_directories(BEFORE ${STORAGE_MANAGER_INCLUDES})

if (RDK_SERVICES_L1_TEST)
	target_link_options(${PLUGIN_IMPLEMENTATION} PRIVATE -Wl,--wrap,open -Wl,--wrap,mkdir -Wl,--wrap,access -Wl,--wrap,nftw -Wl,--wrap,statvfs -Wl,--wrap,rmdir -Wl,--wrap,chown -Wl,--wrap,opendir -Wl,--wrap,readdir -Wl,--wrap,stat -Wl,--wrap,closedir)
endif (RDK_SERVICES_L1_TEST)

if (RDK_SERVICE_L2_TEST)
   find_library(TESTMOCKLIB_LIBRARIES NAMES TestMocklib)
   if (TESTMOCKLIB_LIBRARIES)
       message ("linking mock libraries ${TESTMOCKLIB_LIBRARIES} library")
       target_link_libraries(${PLUGIN_IMPLEMENTATION} PRIVATE ${TESTMOCKLIB_LIBRARIES})
       target_link_options(${PLUGIN_IMPLEMENTATION} PRIVATE -Wl,--wrap,statvfs -Wl,--wrap,chown -Wl,--wrap,access)
   else (TESTMOCKLIB_LIBRARIES)
       message ("Require ${TESTMOCKLIB_LIBRARIES} library")
   endif (TESTMOCKLIB_LIBRARIES)
endif (RDK_SERVICES_L2_TEST)

target_link_libraries(${PLUGIN_IMPLEMENTATION} PRIVATE
    CompileSettingsDebug::CompileSettingsDebug ${NAMESPACE}Plugins::${NAMESPACE}Plugins  ${PLUGIN_STORAGE_MANAGER_EXTRA_LIBRARIES})

install(TARGETS ${PLUGIN_IMPLEMENTATION}
    DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/${STORAGE_DIRECTORY}/plugins)

write_config(${PLUGIN_NAME})
