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

#include "LifecycleManagerTelemetryReporting.h"
#include "UtilsLogging.h"
#include "tracing/Logging.h"
#include <time.h>

namespace WPEFramework
{
namespace Plugin
{
    LifecycleManagerTelemetryReporting::LifecycleManagerTelemetryReporting(): mTelemetryMetricsObject(nullptr), mCurrentservice(nullptr)
    {
    }

    LifecycleManagerTelemetryReporting::~LifecycleManagerTelemetryReporting()
    {
        if(mTelemetryMetricsObject )
        {
            mTelemetryMetricsObject ->Release();
            mTelemetryMetricsObject = nullptr;
            mCurrentservice = nullptr;
        }
    }

    LifecycleManagerTelemetryReporting& LifecycleManagerTelemetryReporting::getInstance()
    {
        LOGINFO("Get LifecycleManagerTelemetryReporting Instance");
        static LifecycleManagerTelemetryReporting instance;
        return instance;
    }

    void LifecycleManagerTelemetryReporting::initialize(PluginHost::IShell* service)
    {
        ASSERT(nullptr != service);
        mAdminLock.Lock();
        mCurrentservice = service;
        mAdminLock.Unlock();
        if(Core::ERROR_NONE != createTelemetryMetricsPluginObject())
        {
            LOGERR("Failed to create TelemetryMetricsObject\n");
        }
    }

    time_t LifecycleManagerTelemetryReporting::getCurrentTimestamp()
    {
        timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return ((time_t)(ts.tv_sec * 1000) + ((time_t)ts.tv_nsec/1000000));
    }

/*
* Creates TelemetryMetrics plugin object to access interface methods
*/
    Core::hresult LifecycleManagerTelemetryReporting::createTelemetryMetricsPluginObject()
    {
        Core::hresult status = Core::ERROR_GENERAL;

        mAdminLock.Lock();
        if (nullptr == mCurrentservice)
        {
                LOGERR("mCurrentservice is null \n");
        }
        else if (nullptr == (mTelemetryMetricsObject = mCurrentservice->QueryInterfaceByCallsign<WPEFramework::Exchange::ITelemetryMetrics>("org.rdk.TelemetryMetrics")))
        {
                LOGERR("Failed to create TelemetryMetricsObject\n");
        }
        else
        {
            status = Core::ERROR_NONE;
            LOGINFO("created TelemetryMetrics Object");
        }
        mAdminLock.Unlock();
        return status;
    }

    void LifecycleManagerTelemetryReporting::reportTelemetryDataOnStateChange(ApplicationContext* context, const JsonObject &data)
    {
        string appId = "";
        RequestType requestType = REQUEST_TYPE_NONE;
        time_t requestTime = 0;
        time_t currentTime = 0;
        Exchange::ILifecycleManager::LifecycleState targetLifecycleState;
        Exchange::ILifecycleManager::LifecycleState newLifecycleState;
        JsonObject jsonParam;
        std::string telemetryMetrics = "";
        std::string markerName = "";
        bool shouldPublish = false;

        if(nullptr == mTelemetryMetricsObject) /*mTelemetryMetricsObject is null retry to create*/
        {
            if(Core::ERROR_NONE != createTelemetryMetricsPluginObject())
            {
                LOGERR("Failed to create TelemetryMetricsObject\n");
            }
        }

        if (nullptr == context)
        {
            LOGERR("context is nullptr");
        }
        else if (nullptr == mTelemetryMetricsObject)
        {
            LOGERR("mTelemetryMetricsObject is not valid");
        }
        else if (!data.HasLabel("appId") || (appId = data["appId"].String()).empty())
        {
            LOGERR("appId not present or empty");
        }
        else
        {
            requestType = context->getRequestType();
            requestTime = context->getRequestTime();
            currentTime = getCurrentTimestamp();
            targetLifecycleState = context->getTargetLifecycleState();
            newLifecycleState = static_cast<Exchange::ILifecycleManager::LifecycleState>(data["newLifecycleState"].Number());
            LOGINFO("Received state change for appId %s newLifecycleState %d requestType %d", appId.c_str(), newLifecycleState, requestType);

            switch(requestType)
            {
                case REQUEST_TYPE_LAUNCH:
                    if (((Exchange::ILifecycleManager::LifecycleState::ACTIVE == newLifecycleState) && (Exchange::ILifecycleManager::LifecycleState::ACTIVE == targetLifecycleState)) ||
                        ((Exchange::ILifecycleManager::LifecycleState::PAUSED == newLifecycleState) && (Exchange::ILifecycleManager::LifecycleState::PAUSED == targetLifecycleState)))
                    {
                        /*Telemetry reporting - launch case*/
                        jsonParam["lifecycleManagerSpawnTime"] = (int)(currentTime - requestTime);
                        jsonParam.ToString(telemetryMetrics);
                        markerName = TELEMETRY_MARKER_LAUNCH_TIME;
                        mTelemetryMetricsObject->Record(appId, telemetryMetrics, markerName);
                    }
                break;
                case REQUEST_TYPE_TERMINATE:
                    if(Exchange::ILifecycleManager::LifecycleState::UNLOADED == newLifecycleState)
                    {
                        /*Telemetry reporting - close case*/
                        jsonParam["lifecycleManagerSetTargetStateTime"] = (int)(currentTime - requestTime);
                        jsonParam.ToString(telemetryMetrics);
                        markerName = TELEMETRY_MARKER_CLOSE_TIME;
                        mTelemetryMetricsObject->Record(appId, telemetryMetrics, markerName);
                    }
                    else if(Exchange::ILifecycleManager::LifecycleState::SUSPENDED == newLifecycleState)
                    {
                        /*Telemetry reporting - wake case, wake is called during app terminate*/
                        markerName = TELEMETRY_MARKER_WAKE_TIME;
                        shouldPublish = true;
                    }
                break;
                case REQUEST_TYPE_SUSPEND:
                    /*Telemetry reporting - suspend case*/
                    if(Exchange::ILifecycleManager::LifecycleState::SUSPENDED == newLifecycleState)
                    {
                        markerName = TELEMETRY_MARKER_SUSPEND_TIME;
                        shouldPublish = true;
                    }
                break;
                case REQUEST_TYPE_RESUME:
                    /*Telemetry reporting - resume case*/
                    if(Exchange::ILifecycleManager::LifecycleState::ACTIVE == newLifecycleState)
                    {
                        markerName = TELEMETRY_MARKER_RESUME_TIME;
                        shouldPublish = true;
                    }
                break;
                case REQUEST_TYPE_HIBERNATE:
                    /*Telemetry reporting - hibernate case*/
                    if(Exchange::ILifecycleManager::LifecycleState::HIBERNATED == newLifecycleState)
                    {
                        markerName = TELEMETRY_MARKER_HIBERNATE_TIME;
                        shouldPublish = true;
                    }
                break;
                default:
                    LOGERR("requestType is invalid");
                break;
            }

            if (!markerName.empty() && shouldPublish)
            {
                jsonParam["appId"] = appId;
                jsonParam["appInstanceId"] = context->getAppInstanceId();
                jsonParam["lifecycleManagerSetTargetStateTime"] = (int)(currentTime - requestTime);
                jsonParam.ToString(telemetryMetrics);
                if(!telemetryMetrics.empty())
                {
                    mTelemetryMetricsObject->Record(appId, telemetryMetrics, markerName);
                    mTelemetryMetricsObject->Publish(appId, markerName);
                }
            }
        }
    }

} /* namespace Plugin */
} /* namespace WPEFramework */

