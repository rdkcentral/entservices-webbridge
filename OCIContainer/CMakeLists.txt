###
# If not stated otherwise in this file or this component's LICENSE
# file the following copyright and licenses apply:
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
###

set(PLUGIN_NAME OCIContainer)
set(MODULE_NAME ${NAMESPACE}${PLUGIN_NAME})

set(PLUGIN_OCICONTAINER_STARTUPORDER "" CACHE STRING "To configure startup order of OCIContainer plugin")

find_package(PkgConfig)
find_package(${NAMESPACE}Plugins REQUIRED)
find_package(${NAMESPACE}Definitions REQUIRED)
find_package(CompileSettingsDebug CONFIG REQUIRED)
find_package(Dobby CONFIG)

# Temporary fix to get defines in Dobby. Will be removed later.
add_definitions( -DRDK )

pkg_search_module(OMI_CLIENT "omiclientlib")

if(NOT OMI_CLIENT_FOUND)
    if(NOT RDK_SERVICE_L2_TEST)
        set(OMI_CLIENT_INCLUDE_DIRS stubs)
        message("Using stubs for omiclientlib")
    else()
        message("RDK_SERVICE_L2_TEST is enabled, not using stubs for omiclientlib")
    endif()
endif()

add_definitions(-DOCICONTAINER_API_VERSION_NUMBER_MAJOR=1)
add_definitions(-DOCICONTAINER_API_VERSION_NUMBER_MINOR=0)
add_definitions(-DOCICONTAINER_API_VERSION_NUMBER_PATCH=0)

add_library(${MODULE_NAME} SHARED
        OCIContainer.cpp
        OCIContainerImplementation.cpp
        DobbyInterface.cpp
        Module.cpp
)

set_target_properties(${MODULE_NAME} PROPERTIES
        CXX_STANDARD 11
        CXX_STANDARD_REQUIRED YES)

set_source_files_properties(OCIContainer.cpp PROPERTIES COMPILE_FLAGS "-fexceptions")

list(APPEND CMAKE_MODULE_PATH
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/")

# We need libsystemd since we're using their dbus library (sd-bus)
# There's probably a better way of doing this...
find_path(SYSTEMD_INCLUDE_DIRS
    NAMES systemd/sd-bus.h
)

find_library(SYSTEMD_LIBRARIES
    NAMES systemd
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
    LIBSYSTEMD
    SYSTEMD_LIBRARIES SYSTEMD_INCLUDE_DIRS
)

target_include_directories(${MODULE_NAME}
        PRIVATE
        ../helpers
        ${OMI_CLIENT_INCLUDE_DIRS}
)

target_link_libraries(${MODULE_NAME}
        PRIVATE
        CompileSettingsDebug::CompileSettingsDebug
        ${DOBBY_LIBRARIES}
        ${NAMESPACE}Plugins::${NAMESPACE}Plugins
        ${NAMESPACE}Definitions::${NAMESPACE}Definitions
        ${SYSTEMD_LIBRARIES}
        ${OMI_CLIENT_LIBRARIES}
)

if (RDK_SERVICE_L2_TEST)
   find_library(TESTMOCKLIB_LIBRARIES NAMES TestMocklib)
   if (TESTMOCKLIB_LIBRARIES)
       message ("linking mock libraries ${TESTMOCKLIB_LIBRARIES} library")
       target_link_libraries(${MODULE_NAME} PRIVATE ${TESTMOCKLIB_LIBRARIES})
       target_include_directories(${MODULE_NAME} PRIVATE 
       ${CMAKE_SOURCE_DIR}/../../entservices-infra/entservices-testframework/Tests/mocks
       )
   else (TESTMOCKLIB_LIBRARIES)
       message ("Require ${TESTMOCKLIB_LIBRARIES} library")
   endif (TESTMOCKLIB_LIBRARIES)
endif (RDK_SERVICE_L2_TEST)

# ${NAMESPACE}Protocols::${NAMESPACE}Protocols
install(TARGETS ${MODULE_NAME}
        DESTINATION lib/${STORAGE_DIRECTORY}/plugins)

write_config(${PLUGIN_NAME})
