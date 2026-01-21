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

#pragma once

// using jsoncpp instead of Thunders code::json, because of slash escaping.
// In core::json "/path" will be serialized to either "\/path" (normal case) 
// or just path (without quotation, if quotation disabled using path.SetQuoted(false))
#include <json/json.h>
#include "tracing/Logging.h"
#include <string>
#include "ApplicationConfiguration.h"
#include <interfaces/IRuntimeManager.h>
#include "AIConfiguration.h"

namespace WPEFramework
{
namespace Plugin
{

//    struct ApplicationConfiguration;

    class DobbySpecGenerator
    {
        public:
            DobbySpecGenerator();
            virtual ~DobbySpecGenerator();

            bool generate(const ApplicationConfiguration& config, const WPEFramework::Exchange::RuntimeConfig& runtimeConfig, string& outputJsonString);

        private:
            Json::Value createEnvVars(const ApplicationConfiguration& config, const WPEFramework::Exchange::RuntimeConfig& runtimeConfig) const;
            Json::Value createMounts(const ApplicationConfiguration& config, const WPEFramework::Exchange::RuntimeConfig& runtimeConfig) const;
            Json::Value createRdkPlugins(const ApplicationConfiguration& config, const WPEFramework::Exchange::RuntimeConfig& runtimeConfig) const;
            Json::Value createMinidumpPlugin() const;
            Json::Value createAppServiceSDKPlugin(const ApplicationConfiguration& config, const WPEFramework::Exchange::RuntimeConfig& runtimeConfig) const;
            Json::Value createNetworkPlugin(const ApplicationConfiguration& config, const WPEFramework::Exchange::RuntimeConfig& runtimeConfig) const;
            Json::Value createBindMount(const std::string &source,
                                        const std::string &destination,
                                        unsigned long options) const;

            bool shouldEnableGpu(const ApplicationConfiguration& config) const;

            ssize_t getSysMemoryLimit(const ApplicationConfiguration& config, const WPEFramework::Exchange::RuntimeConfig& runtimeConfig) const;
            ssize_t getGPUMemoryLimit(const ApplicationConfiguration& config, const WPEFramework::Exchange::RuntimeConfig& runtimeConfig) const;
            bool getVpuEnabled(const ApplicationConfiguration& config, const WPEFramework::Exchange::RuntimeConfig& runtimeConfig) const;
	    std::string getCpuCores();
            void populateClassicPlugins(const ApplicationConfiguration& config, const WPEFramework::Exchange::RuntimeConfig& runtimeConfig, Json::Value& spec);
            Json::Value createEthanLogPlugin(const ApplicationConfiguration& config, const WPEFramework::Exchange::RuntimeConfig& runtimeConfig) const;
            Json::Value createMulticastSocketPlugin(const ApplicationConfiguration& config, const WPEFramework::Exchange::RuntimeConfig& runtimeConfig) const;
            Json::Value createIonMemoryPlugin() const;
            Json::Value createThunderPlugin(const ApplicationConfiguration& config) const;
            Json::Value createOpenCDMPlugin(const ApplicationConfiguration& config, const WPEFramework::Exchange::RuntimeConfig& runtimeConfig) const;
            Json::Value createPrivateDataMount(const WPEFramework::Exchange::RuntimeConfig& runtimeConfig) const;
            void createFkpsMounts(const ApplicationConfiguration& config, const WPEFramework::Exchange::RuntimeConfig& runtimeConfig, Json::Value& spec) const;
            Json::Value createTmpfsMount(const std::string &mntDestination,
                                 unsigned long mntOptions) const;
            Json::Value createResourceManagerMount(const ApplicationConfiguration& config) const;
            Json::Value getWorkingDir(const ApplicationConfiguration& config, const WPEFramework::Exchange::RuntimeConfig& runtimeConfig) const;
            void initialiseIonHeapsJson();
            std::string encodeURL(std::string url) const;
	    void addHolePunchPortToSpec(Json::Value &spec, in_port_t port) const;

            Json::Value mIonMemoryPluginData;
	    std::string mPackageMountPoint;
	    std::string mRuntimeMountPoint;
            std::string mGstRegistrySourcePath;
            std::string mGstRegistryDestinationPath;
            AIConfiguration* mAIConfiguration;
    };
} /* namespace Plugin */
} /* namespace WPEFramework */
