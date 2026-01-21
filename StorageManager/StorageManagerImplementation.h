/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2024 RDK Management
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
#include <interfaces/IStorageManager.h>
#include <interfaces/IConfiguration.h>
#include <interfaces/IStore2.h>
#ifdef ENABLE_AIMANAGERS_TELEMETRY_METRICS
#include <interfaces/ITelemetryMetrics.h>
#endif
#include <ftw.h>
#include <mutex>

namespace WPEFramework {
namespace Plugin {

    class StorageManagerImplementation : public Exchange::IStorageManager ,public Exchange::IConfiguration{

        private:
        class Config : public Core::JSON::Container {
        private:
            Config(const Config&) = delete;
            Config& operator=(const Config&) = delete;

        public:
            Config()
                : Core::JSON::Container()
                {
                    Add(_T("path"), &Path);
                }

        public:
            Core::JSON::String Path;
        };

    public:

        StorageManagerImplementation();
        ~StorageManagerImplementation() override;

        StorageManagerImplementation(const StorageManagerImplementation&) = delete;
        StorageManagerImplementation& operator=(const StorageManagerImplementation&) = delete;

        BEGIN_INTERFACE_MAP(StorageManagerImplementation)
        INTERFACE_ENTRY(Exchange::IStorageManager)
        INTERFACE_ENTRY(Exchange::IConfiguration)
        END_INTERFACE_MAP

        Core::hresult CreateStorage(const string& appId, const uint32_t& size, string& path, string& errorReason) override;
        Core::hresult GetStorage(const string& appId, const int32_t& userId, const int32_t& groupId, string& path, uint32_t& size, uint32_t& used) override;
        Core::hresult DeleteStorage(const string& appId, string& errorReason) override;
        Core::hresult Clear(const string& appId, string& errorReason) override;
        Core::hresult ClearAll(const string& exemptionAppIds, string& errorReason) override;

        // IConfiguration methods
        uint32_t Configure(PluginHost::IShell* service) override;

    private:

        Config _config;
        PluginHost::IShell* mCurrentservice;
#ifdef ENABLE_AIMANAGERS_TELEMETRY_METRICS
        Exchange::ITelemetryMetrics* mTelemetryMetricsObject;
#endif
        std::string mBaseStoragePath;                           /* Base path for the app storage */
    };
} /* namespace Plugin */
} /* namespace WPEFramework */
