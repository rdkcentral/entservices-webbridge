/**
* If not stated otherwise in this file or this component's LICENSE
* file the following copyright and licenses apply:
*
* Copyright 2025 RDK Management
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
**/

#include "MigrationImplementation.h"

#include <fstream>
#include <core/core.h>
#include <interfaces/entservices_errorcodes.h>
#include "UtilsLogging.h"
#include "UtilsgetFileContent.h"
#include "rfcapi.h"

#define MIGRATIONSTATUS "/opt/secure/persistent/MigrationStatus"
#define TR181_MIGRATIONSTATUS "Device.DeviceInfo.Migration.MigrationStatus"

namespace WPEFramework
{
    namespace Plugin
    {
        SERVICE_REGISTRATION(MigrationImplementation, 1, 0);
    
        MigrationImplementation::MigrationImplementation()
        {
            LOGINFO("MigrationImplementation Constructor called");
        }

        MigrationImplementation::~MigrationImplementation()
        {
            LOGINFO("MigrationImplementation Destructor called");
        }

        Core::hresult MigrationImplementation::SetMigrationStatus(const MigrationStatus status, MigrationResult& migrationResult)
        {
            // Map enum to string
            static const std::unordered_map<MigrationStatus, std::string> statusToString = {
                { MIGRATION_STATUS_NOT_STARTED,                "NOT_STARTED" },
                { MIGRATION_STATUS_NOT_NEEDED,                 "NOT_NEEDED" },
                { MIGRATION_STATUS_STARTED,                    "STARTED" },
                { MIGRATION_STATUS_PRIORITY_SETTINGS_MIGRATED, "PRIORITY_SETTINGS_MIGRATED" },
                { MIGRATION_STATUS_DEVICE_SETTINGS_MIGRATED,   "DEVICE_SETTINGS_MIGRATED" },
                { MIGRATION_STATUS_CLOUD_SETTINGS_MIGRATED,    "CLOUD_SETTINGS_MIGRATED" },
                { MIGRATION_STATUS_APP_DATA_MIGRATED,          "APP_DATA_MIGRATED" },
                { MIGRATION_STATUS_MIGRATION_COMPLETED,        "MIGRATION_COMPLETED" }
            };

            auto it = statusToString.find(status);
            if (it != statusToString.end()) {
                // if file exists, it will be truncated, otherwise it will be created
                std::ofstream file(MIGRATIONSTATUS, std::ios::trunc);
                if (file.is_open()) {
                // Write the string status to the file
                file << it->second;
                LOGINFO("Current ENTOS Migration Status is %s\n", it->second.c_str());
                } else {
                    LOGERR("Failed to open or create file %s\n", MIGRATIONSTATUS);
                    return (ERROR_FILE_IO);
                }
                file.close();
            } else {
                LOGERR("Invalid Migration Status\n");
                return (WPEFramework::Core::ERROR_INVALID_PARAMETER);
            }
            migrationResult.success = true;
            return WPEFramework::Core::ERROR_NONE;
        }

        Core::hresult MigrationImplementation::GetMigrationStatus(MigrationStatusInfo& migrationStatusInfo)
        {
            bool status = false;
            RFC_ParamData_t param = {0};
            WDMP_STATUS wdmpstatus = getRFCParameter((char*)"thunderapi", TR181_MIGRATIONSTATUS, &param);
            if (WDMP_SUCCESS == wdmpstatus) {
                std::string migrationStatusStr = param.value;

                static const std::unordered_map<std::string, MigrationStatus> stringToStatus = {
                    {"NOT_STARTED",                MIGRATION_STATUS_NOT_STARTED},
                    {"NOT_NEEDED",                 MIGRATION_STATUS_NOT_NEEDED},
                    {"STARTED",                    MIGRATION_STATUS_STARTED},
                    {"PRIORITY_SETTINGS_MIGRATED", MIGRATION_STATUS_PRIORITY_SETTINGS_MIGRATED},
                    {"DEVICE_SETTINGS_MIGRATED",   MIGRATION_STATUS_DEVICE_SETTINGS_MIGRATED},
                    {"CLOUD_SETTINGS_MIGRATED",    MIGRATION_STATUS_CLOUD_SETTINGS_MIGRATED},
                    {"APP_DATA_MIGRATED",          MIGRATION_STATUS_APP_DATA_MIGRATED},
                    {"MIGRATION_COMPLETED",        MIGRATION_STATUS_MIGRATION_COMPLETED}
                };

                auto it = stringToStatus.find(migrationStatusStr);
                if (it != stringToStatus.end()) {
                    migrationStatusInfo.migrationStatus = it->second;
                    LOGINFO("Current ENTOS Migration Status is: %s\n", migrationStatusStr.c_str());
                    status = true;
                }
            } else {
            LOGINFO("Failed to get RFC parameter for Migration Status \n");
            }

            return (status ? static_cast<uint32_t>(WPEFramework::Core::ERROR_NONE) : static_cast<uint32_t>(ERROR_FILE_IO));
        }

        Core::hresult MigrationImplementation::GetBootTypeInfo(BootTypeInfo& bootTypeInfo)
        {
            bool status = false;
            const char* filename = "/tmp/bootType";
            std::string propertyName = "BOOT_TYPE";
            std::string bootTypeStr;

            if (Utils::readPropertyFromFile(filename, propertyName, bootTypeStr)) {
                static const std::unordered_map<std::string, BootType> stringToBootType = {
                    {"BOOT_INIT",         BOOT_TYPE_INIT},
                    {"BOOT_NORMAL",       BOOT_TYPE_NORMAL},
                    {"BOOT_MIGRATION",    BOOT_TYPE_MIGRATION},
                    {"BOOT_UPDATE",       BOOT_TYPE_UPDATE}
                };

                auto it = stringToBootType.find(bootTypeStr);
                if (it != stringToBootType.end()) {
                    bootTypeInfo.bootType = it->second;
                    LOGINFO("Boot type changed to: %s, current OS Class: rdke\n", bootTypeStr.c_str());
                    status = true;
                }
            } else {
                 LOGERR("BootType is not present");
            }
            return (status ? static_cast<uint32_t>(WPEFramework::Core::ERROR_NONE) : static_cast<uint32_t>(ERROR_FILE_IO));
        }
    } // namespace Plugin
} // namespace WPEFramework
