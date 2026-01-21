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

#pragma once

#include "Module.h"
#include <interfaces/IAppManager.h>
#include "UtilsJsonRpc.h"
#include <string>
#include <iostream>

namespace WPEFramework
{
    namespace Plugin
    {

        class SceneSet : public PluginHost::IPlugin
        {
        public:
            SceneSet(const SceneSet &) = delete;
            SceneSet &operator=(const SceneSet &) = delete;
            SceneSet(SceneSet &&) = delete;
            SceneSet &operator=(SceneSet &&) = delete;

            SceneSet();

            ~SceneSet() override = default;

        private:
            class Configuration : public Core::JSON::Container
            {
            public:
                Configuration()
                    : Core::JSON::Container(), refAppName()
                {
                    Add(_T("refAppName"), &refAppName);
                }
                ~Configuration() = default;

                Configuration(Configuration &&) = delete;
                Configuration(const Configuration &) = delete;
                Configuration &operator=(Configuration &&) = delete;
                Configuration &operator=(const Configuration &) = delete;

            public:
                Core::JSON::String refAppName;
            };

            class NotificationHandler : public Exchange::IAppManager::INotification
            {

            public:
                NotificationHandler(SceneSet &parent) : mParent(parent) {}
                ~NotificationHandler() {}

                void OnAppLifecycleStateChanged(const string &appId, const string &appInstanceId, const Exchange::IAppManager::AppLifecycleState newState, const Exchange::IAppManager::AppLifecycleState oldState, const Exchange::IAppManager::AppErrorReason errorReason)
                {
                    mParent.OnAppLifecycleStateChanged(appId, appInstanceId, newState, oldState, errorReason);
                }

                BEGIN_INTERFACE_MAP(NotificationHandler)
                INTERFACE_ENTRY(Exchange::IAppManager::INotification)
                END_INTERFACE_MAP

            private:
                SceneSet &mParent;
            };

        public:
            // IPlugin Methods
            const string Initialize(PluginHost::IShell *service) override;
            void Deinitialize(PluginHost::IShell *service) override;
            string Information() const override;

            // Handle App Manager lifecycle state changes
            void OnAppLifecycleStateChanged(const string &appId, const string &appInstanceId, const Exchange::IAppManager::AppLifecycleState newState, const Exchange::IAppManager::AppLifecycleState oldState, const Exchange::IAppManager::AppErrorReason errorReason);

            BEGIN_INTERFACE_MAP(SceneSet)
            INTERFACE_ENTRY(PluginHost::IPlugin)
            END_INTERFACE_MAP

        private:
            std::string mrefAppName;
            PluginHost::IShell *mService;

            // App Manager interface pointer
            Exchange::IAppManager *mAppManager;
            Core::Sink<NotificationHandler> mNotification;

            void StartReferenceApp();
            void MonitorReferenceAppCrash();
            void RestartReferenceApp();
        };
    } // Plugin
} // WPEFramework
