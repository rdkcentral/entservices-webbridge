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

#pragma once

#include "Module.h"
#include <interfaces/json/JsonData_AppManager.h>
#include <interfaces/json/JAppManager.h>
#include <interfaces/IAppManager.h>
#include <interfaces/IConfiguration.h>
#include "UtilsLogging.h"
#include "tracing/Logging.h"
#include <mutex>

namespace WPEFramework {
namespace Plugin {

    class AppManager: public PluginHost::IPlugin, public PluginHost::JSONRPC
    {
     private:
        class Notification : public RPC::IRemoteConnection::INotification,
                             public Exchange::IAppManager::INotification
        {
            private:
                Notification() = delete;
                Notification(const Notification&) = delete;
                Notification& operator=(const Notification&) = delete;

            public:
            explicit Notification(AppManager* parent)
                : _parent(*parent)
                {
                    ASSERT(parent != nullptr);
                }

                virtual ~Notification()
                {
                }

                BEGIN_INTERFACE_MAP(Notification)
                INTERFACE_ENTRY(Exchange::IAppManager::INotification)
                INTERFACE_ENTRY(RPC::IRemoteConnection::INotification)
                END_INTERFACE_MAP

                void Activated(RPC::IRemoteConnection*) override
                {
                    LOGINFO("AppManager Notification Activated");
                }

                void Deactivated(RPC::IRemoteConnection *connection) override
                {
                    LOGINFO("AppManager Notification Deactivated");
                    _parent.Deactivated(connection);
                }

                void OnAppInstalled(const string& appId, const string& version) override
                {
                    LOGINFO("AppManager on OnAppInstalled" );
                    Exchange::JAppManager::Event::OnAppInstalled(_parent, appId,version);
                }

                void OnAppUninstalled(const string& appId)override
                {
                    LOGINFO("AppManager on OnAppUninstalled" );
                    Exchange::JAppManager::Event::OnAppUninstalled(_parent, appId);
                }

                void OnAppLifecycleStateChanged(const string& appId, const string& appInstanceId,const Exchange::IAppManager::AppLifecycleState newState, const Exchange::IAppManager::AppLifecycleState oldState, const Exchange::IAppManager::AppErrorReason errorReason) override
                {
                    LOGINFO("AppManager on onAppLifecycleStateChanged");
                    Exchange::JAppManager::Event::OnAppLifecycleStateChanged(_parent, appId,appInstanceId,newState,oldState,errorReason);
                }

                void OnAppLaunchRequest(const string& appId, const string& intent, const string& source) override
                {
                    LOGINFO("AppManager on OnAppLaunchRequest" );
                    Exchange::JAppManager::Event::OnAppLaunchRequest(_parent, appId,intent,source);
                }

                void OnAppUnloaded(const string& appId, const string& appInstanceId) override
                {
                    LOGINFO("AppManager on OnAppUnloaded" );
                    Exchange::JAppManager::Event::OnAppUnloaded(_parent, appId ,appInstanceId);
                }

            private:
                AppManager& _parent;
        };

        public:
            AppManager(const AppManager&) = delete;
            AppManager& operator=(const AppManager&) = delete;

            AppManager();
            virtual ~AppManager();

            BEGIN_INTERFACE_MAP(AppManager)
            INTERFACE_ENTRY(PluginHost::IPlugin)
            INTERFACE_ENTRY(PluginHost::IDispatcher)
            INTERFACE_AGGREGATE(Exchange::IAppManager, mAppManagerImpl)
            END_INTERFACE_MAP

            //  IPlugin methods
            // -------------------------------------------------------------------------------------------------------
            const string Initialize(PluginHost::IShell* service) override;
            void Deinitialize(PluginHost::IShell* service) override;
            string Information() const override;

        private:
            void Deactivated(RPC::IRemoteConnection* connection);

        private:
            PluginHost::IShell* mCurrentService{};
            uint32_t mConnectionId{};
            Exchange::IAppManager* mAppManagerImpl{};
            Core::Sink<Notification> mAppManagerNotification;
            Exchange::IConfiguration* mAppManagerConfigure;

            public /* constants */:
            static const string SERVICE_NAME;

        public /* members */:
            static AppManager* _instance;
    };

} /* namespace Plugin */
} /* namespace WPEFramework */
