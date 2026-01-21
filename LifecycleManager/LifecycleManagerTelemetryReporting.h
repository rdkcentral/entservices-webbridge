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

#include "ApplicationContext.h"
#include <interfaces/ITelemetryMetrics.h>

#define TELEMETRY_MARKER_LAUNCH_TIME                         "OverallLaunchTime_split"
#define TELEMETRY_MARKER_CLOSE_TIME                          "AppCloseTime_split"
#define TELEMETRY_MARKER_SUSPEND_TIME                        "SuspendTime_split"
#define TELEMETRY_MARKER_RESUME_TIME                         "ResumeTime_split"
#define TELEMETRY_MARKER_HIBERNATE_TIME                      "HibernateTime_split"
#define TELEMETRY_MARKER_WAKE_TIME                           "WakeTime_split"

namespace WPEFramework
{
namespace Plugin
{

class LifecycleManagerTelemetryReporting
{
    public /*methods*/:
        LifecycleManagerTelemetryReporting(const LifecycleManagerTelemetryReporting&) = delete;
        LifecycleManagerTelemetryReporting& operator=(const LifecycleManagerTelemetryReporting&) = delete;
        static LifecycleManagerTelemetryReporting& getInstance();
        void reportTelemetryDataOnStateChange(ApplicationContext* context, const JsonObject &data);
        time_t getCurrentTimestamp();
        void initialize(PluginHost::IShell* service);

    private /*methods*/:
        LifecycleManagerTelemetryReporting();
        ~LifecycleManagerTelemetryReporting();
        Core::hresult createTelemetryMetricsPluginObject();

    private /*members*/:
        mutable Core::CriticalSection mAdminLock;
        Exchange::ITelemetryMetrics* mTelemetryMetricsObject;
        PluginHost::IShell* mCurrentservice;
};

} /* namespace Plugin */
} /* namespace WPEFramework */

