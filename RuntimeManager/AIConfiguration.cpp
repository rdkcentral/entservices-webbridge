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

#include "AIConfiguration.h"
#include "UtilsLogging.h"
#include "tracing/Logging.h"
#include <json/json.h>
#include <fstream>
#include <sstream>
#include <string>

#define AICONFIGURATION_INI_PATH "/opt/demo/config.ini"

extern char **environ;

namespace WPEFramework
{
namespace Plugin
{
    AIConfiguration::AIConfiguration()
    {
    }

    AIConfiguration::~AIConfiguration()
    {
    }

    void AIConfiguration::initialize()
    {
        readFromConfigFile();
    }

    size_t AIConfiguration::getContainerConsoleLogCap()
    {
        return mConsoleLogCap;	
    }

    std::bitset<32> AIConfiguration::getAppsCpuSet() const
    {
        return mAppsCpuSet;
    }

    ssize_t AIConfiguration::getNonHomeAppMemoryLimit()
    {
        return mNonHomeAppMemoryLimit;
    }

    ssize_t AIConfiguration::getNonHomeAppGpuLimit()
    {
        return mNonHomeAppGpuLimit;	
    }

    std::list<std::string> AIConfiguration::getVpuAccessBlacklist() const
    {
        return mVpuAccessBlacklist;	    
    }

    std::list<std::string> AIConfiguration::getAppsRequiringDBus() const
    {
        return mAppsRequiringDBus;
    }

    std::list<int> AIConfiguration::getMapiPorts() const
    {
        return mMapiPorts;
    }

    bool AIConfiguration::getResourceManagerClientEnabled() const
    {
        return mResourceManagerClientEnabled;
    }

    bool AIConfiguration::getGstreamerRegistryEnabled()
    {
        return mGstreamerRegistryEnabled;
    }

    bool AIConfiguration::getSvpEnabled()
    {
        return mSvpEnabled;
    }

    bool AIConfiguration::getEnableUsbMassStorage()
    {
        return mEnableUsbMassStorage;
    }

    bool AIConfiguration::getIPv6Enabled()
    {
        return mIPv6Enabled;
    }

    size_t AIConfiguration::getIonHeapDefaultQuota() const
    {
        return mIonHeapDefaultQuota;
    }

    in_port_t AIConfiguration::getDialServerPort() const
    {
        return mDialServerPort;
    }
    
    std::string AIConfiguration::getDialServerPathPrefix() const
    { 
        return mDialServerPathPrefix;
    }
    
    std::string AIConfiguration::getDialUsn() const
    {
        return mDialUsn;
    }

    std::list<std::string> AIConfiguration::getPreloads() const
    {
        return mPreloads;
    }
    std::list<std::string> AIConfiguration::readGlobalEnv() const
    {
       std::list<std::string> environmentVariables;
       char **envList = environ;

       for (;*envList;envList++)
       {
           environmentVariables.emplace_back(*envList);
       }
       return environmentVariables;
    }

    std::list<std::string> AIConfiguration::getEnvs() const
    {
        return mEnvVariables;
    }

    std::map<std::string, size_t> AIConfiguration::getIonHeapQuotas() const
    {
        return mIonHeapQuotas;
    }

    void AIConfiguration::readFromCustomData()
    {
        mConsoleLogCap = 1 * 1024 * 1024; //.apps.limits.consoleLogBytes
        mAppsCpuSet = 0xff; //.apps.limits.cpuset
        mNonHomeAppMemoryLimit = 200 * 1024 * 1024; //.apps.limits.memory.defaultLimitBytes
        mNonHomeAppGpuLimit = 64 * 1024 * 1024; //.apps.limits.memory.defaultLimitBytes
        /* mVpuAccessBlacklist - apps.vpuAccessBlacklist */
        /* mAppsRequiringDBusList - apps.requireDBus */
        /* mMapiPorts = apps.mapi.ports */
        mResourceManagerClientEnabled = false; // .apps.essosResourceManager.enableClient
        mGstreamerRegistryEnabled = false; // apps.gstreamer.mapCachedRegistry
        mSvpEnabled = false; // apps.svp.enable
        mEnableUsbMassStorage = false; // apps.usbMassStorage.enable
        mIPv6Enabled = false; // apps.enableIPv6
        mIonHeapDefaultQuota = 256 * 1024 * 1024; // apps.limits.ion.defaultLimitBytes
        mDialServerPort = 8009; //.dial.server.port;
        mDialServerPathPrefix = ""; // dial.server.prefix
        mDialUsn = ""; //.dial.usn
        //mPreloads

        //TODO: SUPPORT Dial
	/*
        if (mDialServerPathPrefix.empty())
        {
            auto dialUuid = AICommon::Uuid::createUuid();
            mDialServerPathPrefix = dialUuid.toString();
        }
        if (mDialUsn.empty())
        {
            AICommon::DthMacAddressProvider macAddressProvider;
            mDialUsn = AICommon::getDeviceDialUsn(macAddressProvider.getMac());
        }
	*/
        mEnvVariables.push_back("WESTEROS_SINK_AMLOGIC_USE_DMABUF=1");
        mEnvVariables.push_back("WESTEROS_GL_USE_AMLOGIC_AVSYNC=1");
        mEnvVariables.push_back("WESTEROS_SINK_USE_FREERUN=1");
        mEnvVariables.push_back("WESTEROS_GL_MODE=3840x2160x60");
        mEnvVariables.push_back("WESTEROS_GL_GRAPHICS_MAX_SIZE=1920x1080");
        mEnvVariables.push_back("WESTEROS_GL_USE_REFRESH_LOCK=1");
    }

    // helper functions for json parsing
    std::list<std::string> parseStringArray(std::string key, std::string value)
    {
        std::list<std::string> result;

        Json::Value root;
        Json::Reader reader;

        if (!reader.parse(value, root))
        {
            LOGERR("Failed to parse JSON for - %s", key.c_str());
            return {};
        }

        for (const auto &val : root)
        {
            result.push_back(val.asString());
        }

        return result;
    }

    std::list<int> parseIntArray(std::string value)
    {
        std::list<int> result;

        Json::Value root;
        Json::Reader reader;

        if (!reader.parse(value, root))
        {
            LOGERR("Failed to parse JSON for int array");
            return {};
        }

        for (const auto &val : root)
        {
            result.push_back(val.asInt());
        }

        return result;
    }

    std::map<std::string, size_t> parseIonLimits(std::string value)
    {
        std::map<std::string, size_t> result;

        Json::Value root;
        Json::Reader reader;

        if (!reader.parse(value, root))
        {
            LOGERR("Failed to parse JSON for ionLimits");
            return {};
        }

        for (const auto &key : root.getMemberNames())
        {
            result[key] = root[key].asUInt();
        }

        return result;
    }

    void AIConfiguration::printAIConfiguration()
    {
         // Output parsed values for verification using member variables

        std::string vpuBlacklistStr = "";
        for (const auto &item : mVpuAccessBlacklist)
        {
            vpuBlacklistStr += item + " ";
        }

        std::string dbusAppsStr = "";
        for (const auto &item : mAppsRequiringDBus)
        {
            dbusAppsStr += item + " ";
        }

        std::string mapiPortsStr = "";
        for (const auto &port : mMapiPorts)
        {
            mapiPortsStr += std::to_string(port) + " ";
        }

        std::string preloadsStr = "";
        for (const auto &lib : mPreloads)
        {
            preloadsStr += lib + " ";
        }

        std::string envsStr = "";
        for (const auto &env : mEnvVariables)
        {
            envsStr += env + " ";
        }

        LOGINFO("consoleLogCap: %zu", mConsoleLogCap);
        LOGINFO("appsCpuSet: 0x%08lx", mAppsCpuSet.to_ulong());
        LOGINFO("nonHomeAppMemoryLimit: %zd", mNonHomeAppMemoryLimit);
        LOGINFO("nonHomeAppGpuLimit: %zd", mNonHomeAppGpuLimit);
        LOGINFO("vpuAccessBlacklist: %s", vpuBlacklistStr.c_str());
        LOGINFO("appsRequiringDBus: %s", dbusAppsStr.c_str());
        LOGINFO("mapiPorts: %s", mapiPortsStr.c_str());
        LOGINFO("resourceManagerClientEnabled: %s", mResourceManagerClientEnabled ? "true" : "false");
        LOGINFO("gstreamerRegistryEnabled: %s", mGstreamerRegistryEnabled ? "true" : "false");
        LOGINFO("svpEnabled: %s", mSvpEnabled ? "true" : "false");
        LOGINFO("enableUsbMassStorage: %s", mEnableUsbMassStorage ? "true" : "false");
        LOGINFO("ipv6Enabled: %s", mIPv6Enabled ? "true" : "false");
        LOGINFO("ionHeapDefaultQuota: %zu", mIonHeapDefaultQuota);
        LOGINFO("dialServerPort: %u", static_cast<unsigned int>(mDialServerPort));
        LOGINFO("dialServerPathPrefix: %s", mDialServerPathPrefix.c_str());
        LOGINFO("dialUsn: %s", mDialUsn.c_str());
        LOGINFO("ionHeapQuotas:");
        for (const auto &pair : mIonHeapQuotas)
        {
            LOGINFO("  Name: %s, Limit: %zu", pair.first.c_str(), pair.second);
        }
        LOGINFO("preloads: %s", preloadsStr.c_str());
        LOGINFO("envVariables: %s", envsStr.c_str());
    }

    void AIConfiguration::readFromConfigFile()
    {
        //TODO SUPPORT DEVICE_FRIENDLYNAME
        //TODO SUPPORT DEVICE_MODEL_NUM
        //TODO SUPPORT PARTNER_ID
        //TODO SUPPORT REGION=USA,
        //TODO SUPPORT LANG=en_US
        //TODO SUPPORT STB_ENTITLEMENTS

        std::list<std::string> gEnv = readGlobalEnv();
        mEnvVariables.insert(mEnvVariables.end(),gEnv.begin(),gEnv.end());
        LOGINFO("AIConfiguration reading from config file at %s", AICONFIGURATION_INI_PATH);
        std::ifstream iniFile(AICONFIGURATION_INI_PATH);
        if (!iniFile.is_open())
        {
            LOGERR("Failed to open the ini file at %s", AICONFIGURATION_INI_PATH);
            LOGINFO("Populating custom values for AIConfiguration from readFromCustomData()");
            readFromCustomData();
            return;
        }

        std::string line;
        while (std::getline(iniFile, line))
        {
            // Skip empty lines
            if (line.empty())
                continue;

            // Remove leading and trailing whitespace
            line.erase(0, line.find_first_not_of(" \t"));
            line.erase(line.find_last_not_of(" \t") + 1);

            // Skip comments
            if (line[0] == '#')
                continue;

            std::istringstream iss(line);
            std::string key, value;
            if (std::getline(iss, key, '=') && std::getline(iss, value))
            {
                key.erase(key.find_last_not_of(" \t") + 1);     // Trim trailing spaces
                value.erase(0, value.find_first_not_of(" \t")); // Trim leading spaces

                if (key == "consoleLogCap")
                {
                    mConsoleLogCap = static_cast<size_t>(std::stoull(value));
                }
                else if (key == "cores")
                {
                    mAppsCpuSet = std::bitset<32>(std::stoul(value));
                }
                else if (key == "ramLimit")
                {
                    mNonHomeAppMemoryLimit = static_cast<ssize_t>(std::stoll(value));
                }
                else if (key == "gpuMemoryLimit")
                {
                    mNonHomeAppGpuLimit = static_cast<ssize_t>(std::stoll(value));
                }
                else if (key == "vpuAccessBlacklist")
                {
                    mVpuAccessBlacklist = parseStringArray(key, value);
                }
                else if (key == "appsRequiringDBus")
                {
                    mAppsRequiringDBus = parseStringArray(key, value);
                }
                else if (key == "mapiPorts")
                {
                    mMapiPorts = parseIntArray(value);
                }
                else if (key == "resourceManagerClientEnabled")
                {
                    mResourceManagerClientEnabled = (value == "1" || value == "true");
                }
                else if (key == "gstreamerRegistryEnabled")
                {
                    mGstreamerRegistryEnabled = (value == "1" || value == "true");
                }
                else if (key == "svpEnabled")
                {
                    mSvpEnabled = (value == "1" || value == "true");
                }
                else if (key == "enableUsbMassStorage")
                {
                    mEnableUsbMassStorage = (value == "1" || value == "true");
                }
                else if (key == "ipv6Enabled")
                {
                    mIPv6Enabled = (value == "1" || value == "true");
                }
                else if (key == "ionDefaultLimit")
                {
                    mIonHeapDefaultQuota = static_cast<size_t>(std::stoull(value));
                }
                else if (key == "dialServerPort")
                {
                    mDialServerPort = static_cast<in_port_t>(std::stoul(value));
                }
                else if (key == "dialServerPathPrefix")
                {
                    // Remove surrounding quotes if present
                    if (!value.empty() && value.front() == '"' && value.back() == '"')
                    {
                        value = value.substr(1, value.size() - 2);
                    }
                    mDialServerPathPrefix = value;
                }
                else if (key == "dialUsn")
                {
                    // Remove surrounding quotes if present
                    if (!value.empty() && value.front() == '"' && value.back() == '"')
                    {
                        value = value.substr(1, value.size() - 2);
                    }
                    mDialUsn = value;
                }
                else if (key == "ionLimits")
                {
                    mIonHeapQuotas = parseIonLimits(value);
                }
                else if (key == "preloads")
                {
                    mPreloads = parseStringArray(key, value);
                }
                else if (key == "envVariables")
                {
                    mEnvVariables = parseStringArray(key, value);
                }
            }
        }

        iniFile.close();

        printAIConfiguration();
    }
} /* namespace Plugin */
} /* namespace WPEFramework */
