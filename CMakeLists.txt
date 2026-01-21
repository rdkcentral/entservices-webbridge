###
# If not stated otherwise in this file or this component's LICENSE
# file the following copyright and licenses apply:
#
# Copyright 2023 RDK Management
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

cmake_minimum_required(VERSION 3.3)

find_package(WPEFramework)

# All packages that did not deliver a CMake Find script (and some deprecated scripts that need to be removed)
# are located in the cmake directory. Include it in the search.
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/")

# Product configuration dir
set(PRODUCT_CONFIG_DIR "/etc/entservices")

option(COMCAST_CONFIG "Comcast services configuration" ON)
if(COMCAST_CONFIG)
    include(services.cmake)
endif()

option(PLUGIN_OCICONTAINER "Include OCIContainer plugin" OFF)

# Library installation section
string(TOLOWER ${NAMESPACE} STORAGE_DIRECTORY)

# for writing pc and config files
include(CmakeHelperFunctions)

if(RDK_SERVICE_L2_TEST)
    add_subdirectory(Tests/L2Tests)
endif()

option(DISABLE_SECURITY_TOKEN "Disable security token" OFF)

if(RDK_SERVICES_L1_TEST)
    add_subdirectory(Tests/L1Tests)
endif()

if(DISABLE_SECURITY_TOKEN)
    add_definitions(-DDISABLE_SECURITY_TOKEN)
endif()

if(PLUGIN_USERSETTINGS)
    add_subdirectory(UserSettings)
endif()

if(PLUGIN_USB_MASS_STORAGE)
    add_subdirectory(USBMassStorage)
endif()

if(PLUGIN_OCICONTAINER)
    add_subdirectory(OCIContainer)
endif()

if(PLUGIN_PERSISTENTSTORE)
    add_subdirectory(PersistentStore)
endif()

if(PLUGIN_USBDEVICE)
    add_subdirectory(USBDevice)
endif()

if(PLUGIN_TELEMETRY)
    add_subdirectory(Telemetry)
endif()

if(PLUGIN_MONITOR)
    add_subdirectory(Monitor)
endif()

if(PLUGIN_RDKSHELL)
    add_subdirectory(RDKShell)
endif()

if(PLUGIN_RUSTADAPTER)
    add_subdirectory(RustAdapter)
endif()

if(PLUGIN_MESSAGECONTROL)
    add_subdirectory(MessageControl)
endif()

if (PLUGIN_RESOURCEMANAGER)
    add_subdirectory(ResourceManager)
endif()

if (PLUGIN_ANALYTICS)
    add_subdirectory(Analytics)
endif()

if(PLUGIN_RDK_WINDOW_MANAGER)
    add_subdirectory(RDKWindowManager)
endif()

if(PLUGIN_CLOUDSTORE)
    add_subdirectory(CloudStore)
endif()

if(PLUGIN_SHAREDSTORAGE)
    add_subdirectory(SharedStorage)
endif()

if(PLUGIN_WEBBRIDGE)
    add_subdirectory(WebBridge)
endif()

if(PLUGIN_LIFECYCLE_MANAGER)
    add_subdirectory(LifecycleManager)
endif()

if(PLUGIN_APPMANAGER)
    add_subdirectory(AppManager)
endif()

if(PLUGIN_RUNTIME_MANAGER)
	add_subdirectory(RuntimeManager)
endif()

if(PLUGIN_STORAGE_MANAGER)
	add_subdirectory(StorageManager)
endif()

if(PLUGIN_PACKAGE_MANAGER)
    add_subdirectory(PackageManager)
endif()

if(PLUGIN_DOWNLOADMANAGER)
    add_subdirectory(DownloadManager)
endif()

if(PLUGIN_NATIVEJS)
    add_subdirectory(NativeJS)
endif()

if(PLUGIN_SCENESET)
    add_subdirectory(SceneSet)
endif()

if(PLUGIN_TELEMETRYMETRICS)
    add_subdirectory(TelemetryMetrics)
endif()

if(PLUGIN_PREINSTALL_MANAGER)
    add_subdirectory(PreinstallManager)
endif()

if(PLUGIN_MIGRATION)
    add_subdirectory(Migration)
endif()

if(PLUGIN_APPGATEWAY)
    add_subdirectory(AppGateway)
endif()

if(PLUGIN_APPNOTIFICATIONS)
    add_subdirectory(AppNotifications)
endif()

if(PLUGIN_FBSETTINGS)
	add_subdirectory(FbSettings)
endif()


if(WPEFRAMEWORK_CREATE_IPKG_TARGETS)
    set(CPACK_GENERATOR "DEB")
    set(CPACK_DEB_COMPONENT_INSTALL ON)
    set(CPACK_COMPONENTS_GROUPING IGNORE)

    set(CPACK_DEBIAN_PACKAGE_NAME "${WPEFRAMEWORK_PLUGINS_OPKG_NAME}")
    set(CPACK_DEBIAN_PACKAGE_VERSION "${WPEFRAMEWORK_PLUGINS_OPKG_VERSION}")
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "${WPEFRAMEWORK_PLUGINS_OPKG_ARCHITECTURE}")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "${WPEFRAMEWORK_PLUGINS_OPKG_MAINTAINER}")
    set(CPACK_DEBIAN_PACKAGE_DESCRIPTION "${WPEFRAMEWORK_PLUGINS_OPKG_DESCRIPTION}")
    set(CPACK_PACKAGE_FILE_NAME "${WPEFRAMEWORK_PLUGINS_OPKG_FILE_NAME}")

    include(CPack)
endif()
