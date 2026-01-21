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

#include <string>
#include <unordered_map>
#include <unordered_set>

#define TELEMETRY_MARKER_DOWNLOAD_TIME                  "DownloadTime_split"
#define TELEMETRY_MARKER_DOWNLOAD_ERROR               "DownloadError_split"
#define TELEMETRY_MARKER_INSTALL_TIME                       "InstallTime_split"
#define TELEMETRY_MARKER_INSTALL_ERROR                    "InstallError_split"
#define TELEMETRY_MARKER_UNINSTALL_TIME                  "UninstallTime_split"
#define TELEMETRY_MARKER_UNINSTALL_ERROR               "UninstallError_split"
#define TELEMETRY_MARKER_LAUNCH_TIME                       "OverallLaunchTime_split"
#define TELEMETRY_MARKER_LAUNCH_ERROR                    "AppLaunchError_split"
#define TELEMETRY_MARKER_CLOSE_TIME                         "AppCloseTime_split"
#define TELEMETRY_MARKER_CLOSE_ERROR                      "AppCloseError_split"
#define TELEMETRY_MARKER_SUSPEND_TIME                     "SuspendTime_split"
#define TELEMETRY_MARKER_RESUME_TIME                      "ResumeTime_split"
#define TELEMETRY_MARKER_HIBERNATE_TIME                 "HibernateTime_split"
#define TELEMETRY_MARKER_WAKE_TIME                          "WakeTime_split"
#define TELEMETRY_MARKER_APP_CRASHED                      "AppCrashed_split"


static const std::unordered_map<std::string, std::unordered_set<std::string>> markerFilters = {
    {TELEMETRY_MARKER_DOWNLOAD_TIME, {"downloadTime", "markerName"}},
    {TELEMETRY_MARKER_DOWNLOAD_ERROR, {"errorCode", "markerName"}},
    {TELEMETRY_MARKER_INSTALL_TIME, {"installTime", "markerName"}},
    {TELEMETRY_MARKER_INSTALL_ERROR, {"errorCode", "markerName"}},
    {TELEMETRY_MARKER_UNINSTALL_TIME, {"uninstallTime", "markerName"}},
    {TELEMETRY_MARKER_UNINSTALL_ERROR, {"errorCode", "markerName"}},
    {TELEMETRY_MARKER_LAUNCH_TIME, {"totalLaunchTime", "appManagerLaunchTime", "packageManagerLockTime", "lifecycleManagerSpawnTime", "windowManagerCreateDisplayTime", "runtimeManagerRunTime", "storageManagerLaunchTime", "fireboltGatewayLaunchTime", "appId", "appInstanceId", "appVersion", "runtimeId", "runtimeVersion", "launchType", "markerName"}},
    {TELEMETRY_MARKER_LAUNCH_ERROR, {"errorCode", "markerName"}},
    {TELEMETRY_MARKER_CLOSE_TIME, {"totalCloseTime", "appManagerCloseTime", "packageManagerUnlockTime", "lifecycleManagerSetTargetStateTime", "windowManagerDestroyTime", "runtimeManagerTerminateTime", "storageManagerTime", "fireboltGatewayTerminateTime", "appId", "appInstanceId", "appVersion", "closeType", "markerName"}},
    {TELEMETRY_MARKER_CLOSE_ERROR, {"errorCode", "markerName"}},
    {TELEMETRY_MARKER_SUSPEND_TIME, {"lifecycleManagerSetTargetStateTime", "runtimeManagerSuspendTime", "appId", "appInstanceId", "markerName"}},
    {TELEMETRY_MARKER_RESUME_TIME, {"lifecycleManagerSetTargetStateTime", "runtimeManagerResumeTime", "appId", "appInstanceId", "markerName"}},
    {TELEMETRY_MARKER_HIBERNATE_TIME, {"lifecycleManagerSetTargetStateTime", "runtimeManagerHibernateTime", "appId", "appInstanceId", "markerName"}},
    {TELEMETRY_MARKER_WAKE_TIME, {"lifecycleManagerSetTargetStateTime", "runtimeManagerWakeTime", "appId", "appInstanceId", "markerName"}},
    {TELEMETRY_MARKER_APP_CRASHED, {"crashReason", "appId", "appInstanceId", "markerName"}},
};
