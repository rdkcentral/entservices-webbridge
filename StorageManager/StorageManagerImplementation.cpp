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


#include "StorageManagerImplementation.h"
#include "UtilsLogging.h"
#include "RequestHandler.h"
#include <sys/statvfs.h>

#define DEFAULT_APP_STORAGE_PATH        "/opt/persistent/storageManager"

#ifdef ENABLE_AIMANAGERS_TELEMETRY_METRICS
#define TELEMETRY_MARKER_LAUNCH_TIME    "OverallLaunchTime_split"
#endif

namespace WPEFramework {
namespace Plugin {

    SERVICE_REGISTRATION(StorageManagerImplementation, 1, 0);

    StorageManagerImplementation::StorageManagerImplementation()
    : mCurrentservice(nullptr)
#ifdef ENABLE_AIMANAGERS_TELEMETRY_METRICS
    , mTelemetryMetricsObject(nullptr)
#endif
    {
        LOGINFO("Create StorageManagerImplementation Instance");
    }

    StorageManagerImplementation::~StorageManagerImplementation()
    {
        LOGINFO("Delete StorageManagerImplementation Instance");
        RequestHandler& handler = RequestHandler::getInstance();
        handler.releasePersistentStoreRemoteStoreObject();

        if (nullptr != mCurrentservice)
        {
            mCurrentservice->Release();
            mCurrentservice = nullptr;
        }
    }

    uint32_t StorageManagerImplementation::Configure(PluginHost::IShell* service)
    {
        uint32_t result = Core::ERROR_NONE;
        Core::hresult status = Core::ERROR_GENERAL;

        if (service != nullptr)
        {
            mCurrentservice = service;
            if (nullptr != mCurrentservice)
            {
                mCurrentservice->AddRef();

                auto configLine = mCurrentservice->ConfigLine();

                _config.FromString(configLine);
                mBaseStoragePath = _config.Path.Value();
                RequestHandler& handler = RequestHandler::getInstance();
                if (mBaseStoragePath.empty())
                {
                    LOGWARN("Base storage path is empty. Setting default path: %s", DEFAULT_APP_STORAGE_PATH);
                    mBaseStoragePath = DEFAULT_APP_STORAGE_PATH;  // Fallback path
                }
                
                handler.setCurrentService(mCurrentservice);
                if (Core::ERROR_NONE != handler.createPersistentStoreRemoteStoreObject())
                {
                    LOGERR("Failed to create createPersistentStoreRemoteStoreObject");
                }
                else
                {
                    LOGINFO("created createPersistentStoreRemoteStoreObject");
                }

#ifdef ENABLE_AIMANAGERS_TELEMETRY_METRICS
                if (nullptr == (mTelemetryMetricsObject = mCurrentservice->QueryInterfaceByCallsign<WPEFramework::Exchange::ITelemetryMetrics>("org.rdk.TelemetryMetrics")))
                {
                    LOGERR("mTelemetryMetricsObject is null \n");
                }
                else
                {
                    LOGINFO("created TelemetryMetrics Object");
                }
#endif

                Core::SystemInfo::SetEnvironment(PATH_ENV, mBaseStoragePath.c_str());
                LOGINFO("Base Storage Path Set: %s", mBaseStoragePath.c_str());
                handler.SetBaseStoragePath(mBaseStoragePath);
                status = handler.populateAppInfoCacheFromStoragePath();
                if (Core::ERROR_NONE != status)
                {
                    LOGERR("populateAppInfoCacheFromStoragePath Failed!!!");
                }
                else
                {
                    LOGINFO("populateAppInfoCacheFromStoragePath Success!!");
                }
            }
        }
        else
        {
            LOGERR("service is null \n");
            result = Core::ERROR_GENERAL;
        }

        return result;
    }

#ifdef ENABLE_AIMANAGERS_TELEMETRY_METRICS
    static time_t getCurrentTimestamp()
    {
        timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return (((time_t)ts.tv_sec * 1000) + ((time_t)ts.tv_nsec/1000000));
    }
#endif

    /**
     * @brief : Creates storage for a given app id and returns the storage path
     */
    Core::hresult StorageManagerImplementation::CreateStorage(const std::string& appId, const uint32_t& size, std::string& path, std::string& errorReason)
    {
        LOGINFO("Entered CreateStorage Implementation appId: %s", appId.c_str());
        Core::hresult status = Core::ERROR_GENERAL;
        RequestHandler& handler = RequestHandler::getInstance();
        if (appId.empty())
        {
            LOGERR("Invalid App ID");
            errorReason = "appId cannot be empty";
        }
        else if (Core::ERROR_NONE != (status = handler.CreateStorage(appId, size, path, errorReason)))
        {
            LOGERR("Failed to create storage for appId: %s, status %u, Error: %s", appId.c_str(), status, errorReason.c_str());
        }
        else
        {
            LOGINFO("Storage created successfully for appId: %s", appId.c_str());
        }
        return status;
    }

    /**
     * @brief : Returns the storage information and location for a given app id
     */
    Core::hresult StorageManagerImplementation::GetStorage(const string& appId, const int32_t& userId, const int32_t& groupId, string& path, uint32_t& size, uint32_t& used)
    {
        LOGINFO("Entered GetStorage Implementation");
        Core::hresult status = Core::ERROR_GENERAL;
        RequestHandler& handler = RequestHandler::getInstance();

        #ifdef ENABLE_AIMANAGERS_TELEMETRY_METRICS
            JsonObject jsonParam;
            std::string telemetryMetrics = "";
	    /* Get current timestamp at the start of getStorage for telemetry */
            time_t requestTime = getCurrentTimestamp();
        #endif

        if (appId.empty())
        {
            LOGERR("Invalid App ID");
        }
        else if (Core::ERROR_NONE != (status = handler.GetStorage(appId, userId, groupId, path, size, used)))
        {
            LOGERR("Failed to get storage information for appId: %s status %u", appId.c_str(), status);
        }
        else
        {
            LOGINFO("Storage retreived successfully for appId: %s", appId.c_str());
        }

        #ifdef ENABLE_AIMANAGERS_TELEMETRY_METRICS
            /* Get current timestamp at the end of getStorage for Telemetry */
            time_t currentTime = getCurrentTimestamp();
            int duration = static_cast<int>(currentTime - requestTime);
            jsonParam["storageManagerLaunchTime"] = duration;
            jsonParam["appId"] = appId;
            jsonParam.ToString(telemetryMetrics);
            if(nullptr != mTelemetryMetricsObject)
            {
		LOGINFO("Record appId %s storageManagerLaunchTime %d",appId.c_str(),duration);
                mTelemetryMetricsObject->Record(appId, telemetryMetrics, TELEMETRY_MARKER_LAUNCH_TIME);
            }
        #endif
        return status;
    }

    /**
     * @brief : Deletes storage for a given app id
     */
    Core::hresult StorageManagerImplementation::DeleteStorage(const string& appId, string& errorReason)
    {
        LOGINFO("Entered DeleteStorage Implementation");
        Core::hresult status = Core::ERROR_GENERAL;
        RequestHandler& handler = RequestHandler::getInstance();
        if (appId.empty())
        {
            errorReason = "AppId is empty";
            LOGERR("AppId is empty");
        }
        else if (Core::ERROR_NONE != (status = handler.DeleteStorage(appId, errorReason)))
        {
            LOGERR("Failed to delete storage for appId: %s, status %u,Error: %s", appId.c_str(), status, errorReason.c_str());
        }
        else
        {
            LOGINFO("Storage deleted successfully for appId: %s", appId.c_str());
        }
        return status;
    }

    /**
     * @brief : Clears storage for a given app id
     */
    Core::hresult StorageManagerImplementation::Clear(const string& appId, string& errorReason)
    {
        LOGINFO("Entered Clear Implementation");
        Core::hresult status = Core::ERROR_GENERAL;
        RequestHandler& handler = RequestHandler::getInstance();

        if(appId.empty())
        {
            errorReason = "Clear called with no appId";
            LOGERR("Clear called with no appId");
        }
        else if(Core::ERROR_NONE != (status = handler.Clear(appId, errorReason)))
        {
            LOGERR("Failed to clear storage for appId: %s, status %u, Error: %s", appId.c_str(), status, errorReason.c_str());
        }
        else
        {
            LOGINFO("Cleared storage successfully for appId: %s", appId.c_str());
        }
        return status;
    }

    /**
     * @brief : Clears all app data except for the exempt app ids
     */
    Core::hresult StorageManagerImplementation::ClearAll(const string& exemptionAppIds, string& errorReason)
    {
        LOGINFO("Entered ClearAll Implementation");
        Core::hresult status = Core::ERROR_GENERAL;
        JsonObject parameters;
        RequestHandler& handler = RequestHandler::getInstance();

        if (Core::ERROR_NONE != (status = handler.ClearAll(exemptionAppIds, errorReason)))
        {
            LOGERR("Failed to clear all storage status %u, Error: %s", status, errorReason.c_str());
        }
        else
        {
            LOGINFO("Cleared all storage successfully, except for exempted app ids %s", exemptionAppIds.c_str());
        }
        return status;
    }
} /* namespace Plugin */
} /* namespace WPEFramework */
