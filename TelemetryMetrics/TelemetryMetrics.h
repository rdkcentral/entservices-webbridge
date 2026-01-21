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
#include <interfaces/ITelemetryMetrics.h>
#include <interfaces/IConfiguration.h>

namespace WPEFramework {
namespace Plugin {

    class TelemetryMetrics: public PluginHost::IPlugin
    {
        public:
            TelemetryMetrics(const TelemetryMetrics&) = delete;
            TelemetryMetrics& operator=(const TelemetryMetrics&) = delete;

            TelemetryMetrics();
            virtual ~TelemetryMetrics();

            BEGIN_INTERFACE_MAP(TelemetryMetrics)
            INTERFACE_ENTRY(PluginHost::IPlugin)
            INTERFACE_AGGREGATE(Exchange::ITelemetryMetrics, mTelemetryMetricsImpl)
            END_INTERFACE_MAP

            //  IPlugin methods
            // -------------------------------------------------------------------------------------------------------
            const string Initialize(PluginHost::IShell* service) override;
            void Deinitialize(PluginHost::IShell* service) override;
            string Information() const override;

        private:
            PluginHost::IShell* mCurrentService{};
            uint32_t mConnectionId{};
            Exchange::ITelemetryMetrics* mTelemetryMetricsImpl{};

        public /* constants */:
            static const string SERVICE_NAME;

    };

} /* namespace Plugin */
} /* namespace WPEFramework */
