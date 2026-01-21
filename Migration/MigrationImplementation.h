/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
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
*/

#pragma once

#include "Module.h"
#include <interfaces/Ids.h>
#include <interfaces/IMigration.h>

#include <com/com.h>
#include <core/core.h>

namespace WPEFramework
{
    namespace Plugin
    {
        class MigrationImplementation : public Exchange::IMigration
        {
            public:
                // We do not allow this plugin to be copied !!
                MigrationImplementation();
                ~MigrationImplementation() override;

                // We do not allow this plugin to be copied !!
                MigrationImplementation(const MigrationImplementation&) = delete;
                MigrationImplementation& operator=(const MigrationImplementation&) = delete;

                BEGIN_INTERFACE_MAP(MigrationImplementation)
                INTERFACE_ENTRY(Exchange::IMigration)
                END_INTERFACE_MAP

            public:
                Core::hresult GetBootTypeInfo(BootTypeInfo& bootTypeInfo) override;
                Core::hresult SetMigrationStatus(const MigrationStatus status, MigrationResult& migrationResult) override;
                Core::hresult GetMigrationStatus(MigrationStatusInfo& migrationStatusInfo) override;
        };
    } // namespace Plugin
} // namespace WPEFramework
