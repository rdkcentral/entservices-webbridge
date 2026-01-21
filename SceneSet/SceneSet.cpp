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

#include "SceneSet.h"
#include <interfaces/IAppManager.h>

#define SCENESET_DEFAULT_APPNAME "rdk-reference-app"

namespace WPEFramework
{
    namespace Plugin
    {
        SceneSet::SceneSet()
            : PluginHost::IPlugin(), mrefAppName(SCENESET_DEFAULT_APPNAME), mService(nullptr), mAppManager(nullptr), mNotification(*this)
        {
        }

        namespace
        {

            static Metadata<SceneSet> metadata(
                // Version
                SCENE_SET_API_VERSION_NUMBER_MAJOR, SCENE_SET_API_VERSION_NUMBER_MINOR, SCENE_SET_API_VERSION_NUMBER_PATCH,
                // Preconditions
                {},
                // Terminations
                {},
                // Controls
                {});
        }

        const string SceneSet::Initialize(PluginHost::IShell *service)
        {
            string message="";
            string configStr;
            ASSERT(service != nullptr);
            ASSERT(mService == nullptr);
            
            LOGINFO();
            
            mService = service;
            mService->AddRef();
            
            configStr = service->ConfigLine().c_str();
            LOGINFO("ConfigLine=%s", service->ConfigLine().c_str());
            SceneSet::Configuration config;
            config.FromString(service->ConfigLine());
            mrefAppName = config.refAppName;
            LOGINFO("refAppName=%s", mrefAppName.c_str());

            if (!mrefAppName.empty()) {
                mAppManager = mService->QueryInterfaceByCallsign<Exchange::IAppManager>("org.rdk.AppManager");
            }

            if (!mAppManager) {
                SYSLOG(Logging::Startup, ("SceneSet: Failed to get AppManager interface"));
            }
            else {
                mAppManager->AddRef();
                mAppManager->Register(&mNotification);
                LOGINFO("AppManager notification registered");
                
                // Start Reference App on plugin startup
                StartReferenceApp();
            }
            
            return message;

        }

        void SceneSet::Deinitialize(PluginHost::IShell *service)
        {

            LOGINFO();
            if (mService != nullptr)
            {
                ASSERT(mService == service);
                if (mAppManager)
                {
                    mAppManager->Unregister(&mNotification);
                    mAppManager->Release();
                    mAppManager = nullptr;
                }
                mService->Release();
                mService = nullptr;
                SYSLOG(Logging::Shutdown, (string(_T("SceneSet de-initialised"))));
            }
        }

        string SceneSet::Information() const
        {
            return (string());
        }

        void SceneSet::OnAppLifecycleStateChanged(const string &appId, const string &appInstanceId, const Exchange::IAppManager::AppLifecycleState newState, const Exchange::IAppManager::AppLifecycleState oldState, const Exchange::IAppManager::AppErrorReason errorReason)
        {

            LOGINFO("OnAppLifecycleStateChanged received appId: %s appInstanceId: %s newState: %u oldState: %u errorReason: %u\n",
                           appId.c_str(),
                           appInstanceId.c_str(),
                           newState,
                           oldState,
                           errorReason);

        }

        void SceneSet::StartReferenceApp()
        {
            // Launch Reference App using AppManager
            LOGINFO();
            if (mAppManager)
            {
                LOGINFO();
                Core::hresult result = mAppManager->LaunchApp(mrefAppName, "", "");
                if (result == Core::ERROR_NONE)
                {
                    SYSLOG(Logging::Startup, ("SceneSet: Reference App launched successfully"));
                }
                else
                {
                    SYSLOG(Logging::Startup, ("SceneSet: Failed to launch Reference App"));
                }
            }
            else
            {
                SYSLOG(Logging::Startup, ("SceneSet: AppManager instance not available"));
            }
        }

        void SceneSet::MonitorReferenceAppCrash()
        {
            LOGINFO();
        }

        void SceneSet::RestartReferenceApp()
        {
            // Listen for lifecycle state changes and restart Reference App if needed
            SYSLOG(Logging::Startup, ("SceneSet: Reference App crashed, restarting..."));
        }

    } // Plugin
} // WPEFramework
