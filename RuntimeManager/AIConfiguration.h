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

#include <vector>
#include <string>
#include <list>
#include <map>
#include <bitset>
#include <netinet/in.h>

namespace WPEFramework
{
namespace Plugin
{
    class AIConfiguration
    {
        public:
            AIConfiguration();
            virtual ~AIConfiguration();
            void initialize();
            size_t getContainerConsoleLogCap();
            std::bitset<32> getAppsCpuSet() const;
            ssize_t getNonHomeAppMemoryLimit();
            ssize_t getNonHomeAppGpuLimit();
            std::list<std::string> getVpuAccessBlacklist() const;
            std::list<std::string> getAppsRequiringDBus() const;
            std::list<int> getMapiPorts() const;
            bool getResourceManagerClientEnabled() const;
            bool getGstreamerRegistryEnabled();
            bool getSvpEnabled();
            bool getEnableUsbMassStorage();
            bool getIPv6Enabled();
            size_t getIonHeapDefaultQuota() const;
            in_port_t getDialServerPort() const;
            std::string getDialServerPathPrefix() const;
            std::string getDialUsn() const;
            std::map<std::string, size_t> getIonHeapQuotas() const;
            void printAIConfiguration();

            // system configuration
            std::list<std::string> getPreloads() const;
            std::list<std::string> getEnvs() const;

        private:
            void readFromCustomData();
            void readFromConfigFile();
            std::list<std::string>readGlobalEnv() const;

            size_t mConsoleLogCap;
            std::bitset<32> mAppsCpuSet;        // cores
            ssize_t mNonHomeAppMemoryLimit;     // ramLimit
            ssize_t mNonHomeAppGpuLimit;        // gpuMemoryLimit
            std::list<std::string> mVpuAccessBlacklist;
            std::list<std::string> mAppsRequiringDBus;
            std::list<int> mMapiPorts;
            bool mResourceManagerClientEnabled;
            bool mGstreamerRegistryEnabled;
            bool mSvpEnabled;
            bool mEnableUsbMassStorage;
            bool mIPv6Enabled;
            size_t mIonHeapDefaultQuota;        // ionDefaultLimit
            in_port_t mDialServerPort;
            std::string mDialServerPathPrefix;
            std::string mDialUsn;
            std::map<std::string, size_t> mIonHeapQuotas; // ionLimits

            // system configuration
            std::list<std::string> mPreloads;
            std::list<std::string> mEnvVariables;
    };
} /* namespace Plugin */
} /* namespace WPEFramework */
