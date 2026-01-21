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
#include <mutex>
#include <json/json.h>


namespace WPEFramework {
namespace Plugin {

    class TelemetryMetricsImplementation : public Exchange::ITelemetryMetrics {

    public:

        TelemetryMetricsImplementation();
        ~TelemetryMetricsImplementation() override;

        TelemetryMetricsImplementation(const TelemetryMetricsImplementation&) = delete;
        TelemetryMetricsImplementation& operator=(const TelemetryMetricsImplementation&) = delete;

        BEGIN_INTERFACE_MAP(TelemetryMetricsImplementation)
        INTERFACE_ENTRY(Exchange::ITelemetryMetrics)
        END_INTERFACE_MAP

        Core::hresult Record(const string& id, const string& metrics, const string& name) override;
        Core::hresult Publish(const string& id, const string& name) override;

    private:
        std::unordered_map<std::string, Json::Value> mMetricsRecord;
        std::mutex mMetricsMutex;
    };
} /* namespace Plugin */
} /* namespace WPEFramework */
