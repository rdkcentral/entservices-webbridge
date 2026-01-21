# If not stated otherwise in this file or this component's license file the
# following copyright and licenses apply:
#
# Copyright 2020 RDK Management
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

set(PLUGIN_NAME FbSettings)
set(MODULE_NAME ${NAMESPACE}${PLUGIN_NAME})

set(VERSION_MAJOR 1)
set(VERSION_MINOR 0)
set(VERSION_PATCH 0)

add_compile_definitions(FBSETTINGS_MAJOR_VERSION=${VERSION_MAJOR})
add_compile_definitions(FBSETTINGS_MINOR_VERSION=${VERSION_MINOR})
add_compile_definitions(FBSETTINGS_PATCH_VERSION=${VERSION_PATCH})

set(MODULE_VERSION ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH})

set(PLUGIN_FBSETTINGS_STARTUPORDER "" CACHE STRING "To configure startup order of FbSettings plugin")
set(PLUGIN_FBSETTINGS_AUTOSTART "false" CACHE STRING "Automatically start FbSettings plugin")

message("Setup ${MODULE_NAME} v${MODULE_VERSION}")

find_package(${NAMESPACE}Plugins REQUIRED)
find_package(${NAMESPACE}Definitions REQUIRED)
find_package(CompileSettingsDebug CONFIG REQUIRED)
find_library(NMPROXY_LIB NAMES ${NAMESPACE}NetworkManagerProxy PATHS ${CMAKE_SYSROOT}/usr/lib/wpeframework/proxystubs)

add_library(${MODULE_NAME} SHARED
        FbSettings.cpp
        Module.cpp)


target_include_directories(${MODULE_NAME} PRIVATE ../helpers)

set_target_properties(${MODULE_NAME} PROPERTIES
        CXX_STANDARD 11
        CXX_STANDARD_REQUIRED YES)

target_compile_definitions(${MODULE_NAME} PRIVATE MODULE_NAME=Plugin_${PLUGIN_NAME})

target_link_libraries(${MODULE_NAME}
        PRIVATE
        CompileSettingsDebug::CompileSettingsDebug
        ${NAMESPACE}Plugins::${NAMESPACE}Plugins
        ${NAMESPACE}Definitions::${NAMESPACE}Definitions)

# To error any missing links
target_link_options(${MODULE_NAME} PRIVATE -Wl,-z,defs)
# To error any warnings
add_compile_options(-Wall -Werror)

install(TARGETS ${MODULE_NAME}
        DESTINATION lib/${STORAGE_DIRECTORY}/plugins)

write_config(${PLUGIN_NAME})
