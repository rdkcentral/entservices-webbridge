/**
* If not stated otherwise in this file or this component's LICENSE
* file the following copyright and licenses apply:
*
* Copyright 2020 RDK Management
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
#include <memory>
#include <iostream>
#include <mutex>
#include <thread>
#include <fstream>
#include <sstream>
#include <unistd.h>
#include <plugins/System.h>
#include "UtilsLogging.h"
#include "tracing/Logging.h"
#include <core/core.h>
#include <com/com.h>
#include <plugins/plugins.h>
#include <interfaces/ILifecycleManager.h>
#include <interfaces/ILifecycleManagerState.h>
#include <interfaces/IAppManager.h>
#include <condition_variable>
#include <map>

namespace WPEFramework
{
    namespace Plugin
    {
        class LifecycleInterfaceConnector
        {
            private:
            class NotificationHandler : public Exchange::ILifecycleManagerState::INotification {

                public:
                    NotificationHandler(LifecycleInterfaceConnector& parent) : mParent(parent){}
                    ~NotificationHandler(){}

                    void OnAppLifecycleStateChanged(const string& appId, const string& appInstanceId, const Exchange::ILifecycleManager::LifecycleState oldState, const Exchange::ILifecycleManager::LifecycleState newState, const string& navigationIntent)
                    {
                        mParent.OnAppLifecycleStateChanged(appId, appInstanceId, oldState, newState, navigationIntent);
                    }

                    BEGIN_INTERFACE_MAP(NotificationHandler)
                    INTERFACE_ENTRY(Exchange::ILifecycleManagerState::INotification)
                    END_INTERFACE_MAP

                private:
                    LifecycleInterfaceConnector& mParent;
            };
            class AppStateChangeNotificationHandler : public Exchange::ILifecycleManager::INotification {

                public:
                    AppStateChangeNotificationHandler(LifecycleInterfaceConnector& parent) : mParent(parent){}
                    ~AppStateChangeNotificationHandler(){}

                    void OnAppStateChanged(const string& appId, Exchange::ILifecycleManager::LifecycleState state, const string& errorReason)
                    {
                        mParent.OnAppStateChanged(appId, state, errorReason);
                    }

                    BEGIN_INTERFACE_MAP(AppStateChangeNotificationHandler)
                    INTERFACE_ENTRY(Exchange::ILifecycleManager::INotification)
                    END_INTERFACE_MAP

                private:
                    LifecycleInterfaceConnector& mParent;
            };

                public/*members*/:
                    static LifecycleInterfaceConnector* _instance;

                public:
                    LifecycleInterfaceConnector(PluginHost::IShell* service);
                    ~LifecycleInterfaceConnector();
                    Core::hresult createLifecycleManagerRemoteObject();
                    void releaseLifecycleManagerRemoteObject();
                    Core::hresult launch(const string& appId, const string& intent, const string& launchArgs, WPEFramework::Exchange::RuntimeConfig& runtimeConfigObject);
                    Core::hresult preLoadApp(const string& appId, const string& launchArgs, WPEFramework::Exchange::RuntimeConfig& runtimeConfigObject, string& error);
                    Core::hresult closeApp(const string& appId);
                    Core::hresult terminateApp(const string& appId);
                    Core::hresult killApp(const string& appId);
                    Core::hresult sendIntent(const string& appId, const string& intent);
                    Core::hresult getLoadedApps(Exchange::IAppManager::ILoadedAppInfoIterator*& apps);
                    void OnAppLifecycleStateChanged(const string& appId, const string& appInstanceId, const Exchange::ILifecycleManager::LifecycleState newState, const Exchange::ILifecycleManager::LifecycleState oldState, const string& navigationIntent);
                    void OnAppStateChanged(const string& appId, Exchange::ILifecycleManager::LifecycleState state, const string& errorReason);
                    Exchange::IAppManager::AppLifecycleState mapAppLifecycleState(Exchange::ILifecycleManager::LifecycleState state);
                    string GetAppInstanceId(const string& appId) const;
                    void removeAppInfoByAppId(const string& appId);
                    Core::hresult isAppLoaded(const string& appId, bool& loaded);
                    bool fileExists(const char* pFileName);
                    Exchange::IAppManager::AppErrorReason mapErrorReason(const string& errorReason);

                private:
                    mutable Core::CriticalSection mAdminLock;
                    Exchange::ILifecycleManager *mLifecycleManagerRemoteObject;
                    Exchange::ILifecycleManagerState *mLifecycleManagerStateRemoteObject;
                    Core::Sink<NotificationHandler> mNotification;
                    Core::Sink<AppStateChangeNotificationHandler> mAppStateChangeNotification;
                    PluginHost::IShell* mCurrentservice;
                    std::condition_variable mStateChangedCV;
                    std::mutex mStateMutex;
                    std::string mAppIdAwaitingPause;
		    std::map<std::string, Exchange::IAppManager::AppLifecycleState> mAppCurrentActionList;
        };
    }
}
