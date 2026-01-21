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

project(PackageManager)
set(PLUGIN_NAME ${PROJECT_NAME}RDKEMS)
# When existing/old PackageManager plugin is removed, remove "RDKEMS" from the PLUGIN_NAME
set(MODULE_NAME ${NAMESPACE}${PLUGIN_NAME})
message("Setup ${MODULE_NAME}")

# XXX: ../cmake/FindCurl.cmake is bad
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/;${CMAKE_MODULE_PATH}")
message("CMAKE_MODULE_PATH=${CMAKE_MODULE_PATH}")

if (LIB_PACKAGE)
    message("LIB_PACKAGE=${LIB_PACKAGE}")
    add_compile_options ( -DUSE_LIBPACKAGE )
    include_directories(${SYSROOT_PATH}/${CMAKE_INSTALL_PREFIX}/include/Package)
    set(LIB_PACKAGE_LIBRARY "${SYSROOT_PATH}/${CMAKE_INSTALL_PREFIX}/lib/libPackage.so")
endif()

option(AIMANAGERS_TELEMETRY_METRICS_SUPPORT "AIMANAGERS_TELEMETRY_METRICS_SUPPORT" OFF)

find_package(${NAMESPACE}Plugins REQUIRED)
find_package(${NAMESPACE}Definitions REQUIRED)
find_package( Curl REQUIRED)
if (CURL_FOUND)
    message("Yay, found curl inc=${CURL_INCLUDE_DIRS} lib=${CURL_LIBRARY}")
endif()

add_library(${MODULE_NAME} SHARED
    PackageManager.cpp
    PackageManagerImplementation.cpp
    Module.cpp
    HttpClient.cpp)

if(BUILD_REFERENCE)
    add_definitions(-DBUILD_REFERENCE=${BUILD_REFERENCE})
endif()

if(AIMANAGERS_TELEMETRY_METRICS_SUPPORT)
    message(STATUS "TELEMETRY_METRICS_SUPPORT is enabled")
    add_definitions(-DENABLE_AIMANAGERS_TELEMETRY_METRICS)
endif(AIMANAGERS_TELEMETRY_METRICS_SUPPORT)

target_include_directories(${MODULE_NAME} PRIVATE
    ../helpers
    ${CURL_INCLUDE_DIR}
    ${CMAKE_SYSROOT}${CMAKE_INSTALL_PREFIX}/include/${NAMESPACE}/interfaces
    )

target_link_libraries(${MODULE_NAME} PRIVATE
    ${NAMESPACE}Definitions::${NAMESPACE}Definitions
    ${NAMESPACE}Plugins::${NAMESPACE}Plugins
    ${CURL_LIBRARY}
    ${LIB_PACKAGE_LIBRARY}
    )

if(RDK_SERVICES_L1_TEST)
    find_package( JsonCpp REQUIRED)
    if (JSONCPP_FOUND)
    message("Yay, found jsoncpp inc=${JSONCPP_INCLUDE_DIRS} lib=${JSONCPP_LIBRARY}")
    endif()
    target_include_directories(${MODULE_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/../entservices-testframework/Tests/mocks)
    target_include_directories(${MODULE_NAME} PUBLIC ${JSONCPP_INCLUDE_DIR})
    target_link_libraries(${MODULE_NAME} PUBLIC ${JSONCPP_LIBRARY})
endif()

option(ENABLE_UNIT_TESTS "Enable unit tests" OFF)
if(ENABLE_UNIT_TESTS)
  target_compile_definitions(${MODULE_NAME} PRIVATE UNIT_TEST)
endif()

install(TARGETS ${MODULE_NAME}
    DESTINATION lib/${STORAGE_DIRECTORY}/plugins)

set(PLUGIN_PACKAGEMANAGER_MODE "Local" CACHE STRING "Controls if the plugin should run in its own process, in process or remote")
set(PLUGIN_PACKAGEMANAGER_AUTOSTART "false" CACHE STRING "Deactivated PackageManager plugin")
#write_config()
write_config(INSTALL_NAME ${PLUGIN_NAME}.json)
