# If not stated otherwise in this file or this component's LICENSE file the
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

cmake_minimum_required(VERSION 3.8)
set(PLUGIN_NAME L1TestsIN)
set(MODULE_NAME ${NAMESPACE}${PLUGIN_NAME})

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(${NAMESPACE}Plugins REQUIRED)

set (TEST_SRC
    tests/test_UtilsFile.cpp
)

set (TEST_LIB
    ${NAMESPACE}Plugins::${NAMESPACE}Plugins
)

set (TEST_INC ../../helpers)

#########################################################################################
# add_plugin_test_ex: Macro to add plugin tests, it will append to TEST_SRC, TEST_INC,
#                     and TEST_LIB. Args are positional.
#                     arg1: test file list as **string**
#                     arg2: include dir list as **string**
#                     arg3: plugin libs to link list as **string**
#
# sample invocation : 
#    add_plugin_test_ex(PLUGIN_NAME
#                       "test/file1.cpp;test/file2.cpp"
#                       "../../inc_dir1;../../inc_dir2"
#                       "${NAMESPACE}PluginName;${NAMESPACE}PluginNameImplementation")
# ----------------------------------------- OR ------------------------------------------
# list(APPEND PLUGIN_NAME_SRC test/file1.cpp)
# list(APPEND PLUGIN_NAME_SRC test/file2.cpp)
#
# list(APPEND PLUGIN_NAME_INC ../../inc_dir1)
# list(APPEND PLUGIN_NAME_INC ../../inc_dir2)
#
# list(APPEND PLUGIN_NAME_LIB ${NAMESPACE}PluginName)
# list(APPEND PLUGIN_NAME_LIB ${NAMESPACE}PluginNameImplementation)
#
#    add_plugin_test_ex(PLUGIN_NAME
#                       "${PLUGIN_NAME_SRC}"
#                       "${PLUGIN_NAME_INC}"
#                       "${PLUGIN_NAME_LIB}")
#
# NOTE: Alternatively test can choose to update `TEST_SRC`, `TEST_INC` & `TEST_LIB`
#       directly (like in the case of Miracast)
#########################################################################################
macro(add_plugin_test_ex plugin_opt plugin_test_sources_str plugin_includes_str plugin_libs_str)
    # Check if the plugin option is enabled
    if(${plugin_opt})
        message(STATUS "${plugin_opt}=ON")

        string(REPLACE ";" ";" srclist "${plugin_test_sources_str}")
        string(REPLACE ";" ";" inclist "${plugin_includes_str}")
        string(REPLACE ";" ";" liblist "${plugin_libs_str}")

        foreach(item IN LISTS srclist)
            # Add each test source file
            list(APPEND TEST_SRC ${item})
        endforeach()

        foreach(item IN LISTS inclist)
            # Add each include directory
            list(APPEND TEST_INC ${item})
        endforeach()

        foreach(item IN LISTS liblist)
            # Add each libraries to link
            list(APPEND TEST_LIB ${item})
        endforeach()
    else()
        message(STATUS "${plugin_opt}=OFF")
    endif()
endmacro()

# helper to add plugin test
macro(add_plugin_test plugin_name test_files)
    # Convert plugin name to uppercase for the option variable
    string(TOUPPER "${plugin_name}" plugin_option)
    set(plugin_opt "PLUGIN_${plugin_option}")

    add_plugin_test_ex(${plugin_opt} "${test_files}" "../../${plugin_name}" "${NAMESPACE}${plugin_name}")
endmacro()

# PLUGIN_USBDEVICE
set (USBDEVICE_INC ${CMAKE_SOURCE_DIR}/../entservices-infra/USBDevice ${CMAKE_SOURCE_DIR}/../entservices-infra/helpers)
set (USBDEVICE_LIBS ${NAMESPACE}USBDevice ${NAMESPACE}USBDeviceImplementation)
add_plugin_test_ex(PLUGIN_USBDEVICE tests/test_USBDevice.cpp "${USBDEVICE_INC}" "${USBDEVICE_LIBS}")

# PLUGIN_USB_MASS_STORAGE
set (USB_MASS_STORAGE_INC ${CMAKE_SOURCE_DIR}/../entservices-infra/USBMassStorage ${CMAKE_SOURCE_DIR}/../entservices-infra/helpers)
set (USB_MASS_STORAGE_LIBS ${NAMESPACE}USBMassStorage ${NAMESPACE}USBMassStorageImplementation)
add_plugin_test_ex(PLUGIN_USB_MASS_STORAGE tests/test_USBMassStorage.cpp "${USB_MASS_STORAGE_INC}" "${USB_MASS_STORAGE_LIBS}")

# PLUGIN_TELEMETRY
set (TELEMETRY_INC ${CMAKE_SOURCE_DIR}/../entservices-infra/Telemetry ${CMAKE_SOURCE_DIR}/../entservices-infra/helpers)
set (TELEMETRY_LIBS ${NAMESPACE}Telemetry ${NAMESPACE}TelemetryImplementation)
add_plugin_test_ex(PLUGIN_TELEMETRY tests/test_Telemetry.cpp "${TELEMETRY_INC}" "${TELEMETRY_LIBS}")

# PLUGIN_APPMANAGER
set (APPMANAGER_INC ${CMAKE_SOURCE_DIR}/../entservices-infra/AppManager ${CMAKE_SOURCE_DIR}/../entservices-infra/helpers)
set (APPMANAGER_LIBS ${NAMESPACE}AppManager ${NAMESPACE}AppManagerImplementation)
add_plugin_test_ex(PLUGIN_APPMANAGER tests/test_AppManager.cpp "${APPMANAGER_INC}" "${APPMANAGER_LIBS}")

# PLUGIN_OCICONTAINER
set (OCICONTAINER_INC ${CMAKE_SOURCE_DIR}/../entservices-infra/OCIContainer ${CMAKE_SOURCE_DIR}/../entservices-infra/OCIContainer/stubs)
set (OCICONTAINER_LIBS ${NAMESPACE}OCIContainer)
add_plugin_test_ex(PLUGIN_OCICONTAINER tests/test_OCIContainer.cpp "${OCICONTAINER_INC}" "${OCICONTAINER_LIBS}")

# PLUGIN_RUNTIME_MANAGER
set (RUNTIMEMANAGER_INC ${CMAKE_SOURCE_DIR}/../entservices-infra/RuntimeManager ${CMAKE_SOURCE_DIR}/../entservices-infra/helpers)
set (RUNTIMEMANAGER_LIBS ${NAMESPACE}RuntimeManager ${NAMESPACE}RuntimeManagerImplementation)
add_plugin_test_ex(PLUGIN_RUNTIME_MANAGER tests/test_RunTimeManager.cpp "${RUNTIMEMANAGER_INC}" "${RUNTIMEMANAGER_LIBS}")


# PLUGIN_USERSETTINGS
set (USERSETTINGS_INC ${CMAKE_SOURCE_DIR}/../entservices-infra/UserSettings ${CMAKE_SOURCE_DIR}/../entservices-infra/helpers)
set (USERSETTINGS_LIBS ${NAMESPACE}UserSettings ${NAMESPACE}UserSettingsImplementation)
add_plugin_test_ex(PLUGIN_USERSETTINGS tests/test_UserSettings.cpp "${USERSETTINGS_INC}" "${USERSETTINGS_LIBS}")

# PLUGIN_RESOURCEMANAGER
set (RESOURCEMANAGER_INC ${CMAKE_SOURCE_DIR}/../entservices-infra/ResourceManager ${CMAKE_SOURCE_DIR}/../entservices-infra/helpers)
add_plugin_test_ex(PLUGIN_RESOURCEMANAGER tests/test_ResourceManager.cpp "${RESOURCEMANAGER_INC}" "${NAMESPACE}ResourceManager")

#PLUGIN_STORAGE_MANAGER
set (STORAGE_MANAGER_INC ${CMAKE_SOURCE_DIR}/../entservices-infra/StorageManager ${CMAKE_SOURCE_DIR}/../entservices-infra/helpers)
set (STORAGE_MANAGER_LIBS ${NAMESPACE}StorageManager ${NAMESPACE}StorageManagerImplementation)
add_plugin_test_ex(PLUGIN_STORAGE_MANAGER tests/test_StorageManager.cpp "${STORAGE_MANAGER_INC}" "${STORAGE_MANAGER_LIBS}")

# PLUGIN_LIFECYCLE_MANAGER
set (LIFECYCLEMANAGER_INC ${CMAKE_SOURCE_DIR}/../entservices-infra/LifecycleManager ${CMAKE_SOURCE_DIR}/../entservices-infra/helpers)
set (LIFECYCLEMANAGER_LIBS ${NAMESPACE}LifecycleManager)
add_plugin_test_ex(PLUGIN_LIFECYCLE_MANAGER tests/test_LifecycleManager.cpp "${LIFECYCLEMANAGER_INC}" "${LIFECYCLEMANAGER_LIBS}")

# PLUGIN_MESSAGECONTROL
set (MESSAGECONTROL_INC ${CMAKE_SOURCE_DIR}/../entservices-infra/MessageControl ${CMAKE_SOURCE_DIR}/../entservices-infra/helpers)
set (MESSAGECONTROL_LIBS ${NAMESPACE}MessageControl)
add_plugin_test_ex(PLUGIN_MESSAGECONTROL tests/test_MessageControl.cpp "${MESSAGECONTROL_INC}" "${MESSAGECONTROL_LIBS}")


# PLUGIN_MIGRATION
set (MIGRATION_INC ${CMAKE_SOURCE_DIR}/../entservices-infra/Migration ${CMAKE_SOURCE_DIR}/../entservices-infra/helpers)
set (MIGRATION_LIBS ${NAMESPACE}Migration ${NAMESPACE}MigrationImplementation)
add_plugin_test_ex(PLUGIN_MIGRATION tests/test_Migration.cpp "${MIGRATION_INC}" "${MIGRATION_LIBS}")

# PLUGIN_PREINSTALL_MANAGER
set (PREINSTALL_MANAGER_INC ${CMAKE_SOURCE_DIR}/../entservices-infra/PreinstallManager ${CMAKE_SOURCE_DIR}/../entservices-infra/helpers)
set (PREINSTALL_MANAGER_LIBS ${NAMESPACE}PreinstallManager ${NAMESPACE}PreinstallManagerImplementation)
add_plugin_test_ex(PLUGIN_PREINSTALL_MANAGER tests/test_PreinstallManager.cpp "${PREINSTALL_MANAGER_INC}" "${PREINSTALL_MANAGER_LIBS}")

# PLUGIN_PACKAGE_MANAGER
set (PACKAGEMANAGER_INC ${CMAKE_SOURCE_DIR}/../entservices-infra/PackageManager ${CMAKE_SOURCE_DIR}/../entservices-infra/helpers)
set (PACKAGEMANAGER_LIBS ${NAMESPACE}PackageManagerRDKEMS)
add_plugin_test_ex(PLUGIN_PACKAGE_MANAGER tests/test_PackageManager.cpp "${PACKAGEMANAGER_INC}" "${PACKAGEMANAGER_LIBS}")

#PLUGIN_DOWNLOAD_MANAGER
set (DOWNLOADMANAGER_INC ${CMAKE_SOURCE_DIR}/../entservices-infra/DownloadManager ${CMAKE_SOURCE_DIR}/../entservices-infra/helpers)
set (DOWNLOADMANAGER_LIBS ${NAMESPACE}DownloadManager ${NAMESPACE}DownloadManagerImplementation)
add_plugin_test_ex(PLUGIN_DOWNLOADMANAGER tests/test_DownloadManager.cpp "${DOWNLOADMANAGER_INC}" "${DOWNLOADMANAGER_LIBS}")

add_library(${MODULE_NAME} SHARED ${TEST_SRC})

if (RDK_SERVICES_L1_TEST)
    target_link_options(${MODULE_NAME} PRIVATE "-Wl,-wrap,opendir")
endif (RDK_SERVICES_L1_TEST)

include_directories(${TEST_INC})

target_link_directories(${MODULE_NAME} PUBLIC ${CMAKE_INSTALL_PREFIX}/lib ${CMAKE_INSTALL_PREFIX}/lib/wpeframework/plugins)

target_link_libraries(${MODULE_NAME} ${TEST_LIB})

target_include_directories(${MODULE_NAME}
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
        ${CMAKE_SOURCE_DIR}/../entservices-testframework/Tests/mocks
        ${CMAKE_SOURCE_DIR}/../entservices-testframework/Tests/mocks/devicesettings
        ${CMAKE_SOURCE_DIR}/../entservices-testframework/Tests/mocks/thunder
        ${CMAKE_SOURCE_DIR}/../Thunder/Source/plugins
        )

target_compile_definitions(${MODULE_NAME} PRIVATE UNIT_TEST)

#install(TARGETS ${PROJECT_NAME} DESTINATION bin)

install(TARGETS ${MODULE_NAME} DESTINATION lib)
write_config(${PLUGIN_NAME})
