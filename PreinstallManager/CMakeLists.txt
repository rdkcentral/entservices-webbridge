# If not stated otherwise in this file or this component's license file the
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

set(PLUGIN_NAME PreinstallManager)
set(MODULE_NAME ${NAMESPACE}${PLUGIN_NAME})
set(PLUGIN_IMPLEMENTATION ${MODULE_NAME}Implementation)

message("Setup ${MODULE_NAME} v${PROJECT_VERSION}")

set(PLUGIN_PREINSTALL_MANAGER_MODE "Off" CACHE STRING "Controls if the plugin should run in its own process, in process or remote")
set(PLUGIN_PREINSTALL_MANAGER_AUTOSTART false CACHE STRING "Automatically start start PreinstallManager plugin")
set(PLUGIN_PREINSTALL_MANAGER_EXTRA_LIBRARIES "")

add_definitions(-DPREINSTALL_MANAGER_API_VERSION_NUMBER_MAJOR=1)
add_definitions(-DPREINSTALL_MANAGER_API_VERSION_NUMBER_MINOR=0)
add_definitions(-DPREINSTALL_MANAGER_API_VERSION_NUMBER_PATCH=0)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

find_package(${NAMESPACE}Plugins REQUIRED)
find_package(${NAMESPACE}Definitions REQUIRED)
find_package(CompileSettingsDebug CONFIG REQUIRED)

add_library(${MODULE_NAME} SHARED
    PreinstallManager.cpp
    Module.cpp
)

set_target_properties(${MODULE_NAME} PROPERTIES
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED YES)


set_source_files_properties(PreinstallManager.cpp PROPERTIES COMPILE_FLAGS "-fexceptions")

target_link_libraries(${MODULE_NAME}
    PRIVATE
    CompileSettingsDebug::CompileSettingsDebug
    ${NAMESPACE}Plugins::${NAMESPACE}Plugins
    ${NAMESPACE}Definitions::${NAMESPACE}Definitions)

install(TARGETS ${MODULE_NAME}
    DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/${STORAGE_DIRECTORY}/plugins)

add_library(${PLUGIN_IMPLEMENTATION} SHARED
    PreinstallManagerImplementation.cpp
    Module.cpp)

include_directories(../helpers)

set_target_properties(${PLUGIN_IMPLEMENTATION} PROPERTIES
        CXX_STANDARD 11
        CXX_STANDARD_REQUIRED YES)

set(PREINSTALL_MANAGER_INCLUDES $ENV{PREINSTALL_MANAGER_INCLUDES})
separate_arguments(PREINSTALL_MANAGER_INCLUDES)
include_directories(BEFORE ${PREINSTALL_MANAGER_INCLUDES})
if (RDK_SERVICES_L1_TEST)
	target_link_options(${PLUGIN_IMPLEMENTATION} PRIVATE -Wl,--wrap,mkdir -Wl,--wrap,access -Wl,--wrap,opendir -Wl,--wrap,readdir -Wl,--wrap,stat -Wl,--wrap,closedir)
	target_link_options(${PLUGIN_IMPLEMENTATION} PRIVATE -Wl,--wrap,stat)
endif (RDK_SERVICES_L1_TEST)
target_link_libraries(${PLUGIN_IMPLEMENTATION} PRIVATE
    CompileSettingsDebug::CompileSettingsDebug ${NAMESPACE}Plugins::${NAMESPACE}Plugins  ${PLUGIN_PREINSTALL_MANAGER_EXTRA_LIBRARIES})

install(TARGETS ${PLUGIN_IMPLEMENTATION}
    DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/${STORAGE_DIRECTORY}/plugins)

write_config(${PLUGIN_NAME})
