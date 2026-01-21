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
#include <interfaces/IMigration.h>
#include <interfaces/json/JMigration.h>
#include <interfaces/json/JsonData_Migration.h>
#include "UtilsLogging.h"
#include "tracing/Logging.h"

namespace WPEFramework 
{
    namespace Plugin
    {
        class Migration : public PluginHost::IPlugin, public PluginHost::JSONRPC 
        {           
            public:
                Migration(const Migration&) = delete;
                Migration& operator=(const Migration&) = delete;

                Migration();
                virtual ~Migration();

                BEGIN_INTERFACE_MAP(Migration)
                INTERFACE_ENTRY(PluginHost::IPlugin)
                INTERFACE_ENTRY(PluginHost::IDispatcher)
                INTERFACE_AGGREGATE(Exchange::IMigration, _migration)
                END_INTERFACE_MAP

                //  IPlugin methods
                // -------------------------------------------------------------------------------------------------------
                const string Initialize(PluginHost::IShell* service) override;
                void Deinitialize(PluginHost::IShell* service) override;
                string Information() const override;

            private:
                void Deactivated(RPC::IRemoteConnection* connection);

            private:
                PluginHost::IShell* _service{};
                uint32_t _connectionId{};
                Exchange::IMigration* _migration{};
       };
    } // namespace Plugin
} // namespace WPEFramework
