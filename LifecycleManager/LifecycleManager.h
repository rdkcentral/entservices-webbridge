/**
* If not stated otherwise in this file or this component's LICENSE
* file the following copyright and licenses apply:
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
**/

#pragma once

#include "Module.h"
#include <interfaces/ILifecycleManager.h>
#include <interfaces/json/JsonData_LifecycleManagerState.h>
#include <interfaces/json/JLifecycleManagerState.h>
#include <interfaces/ILifecycleManagerState.h>
#include "UtilsLogging.h"
#include "tracing/Logging.h"
#include <mutex>

namespace WPEFramework
{
    namespace Plugin
    {
        class LifecycleManager : public PluginHost::IPlugin, public PluginHost::JSONRPC
	{
            class Notification : public RPC::IRemoteConnection::INotification,
                                 public Exchange::ILifecycleManagerState::INotification
            {
                private:
                    Notification() = delete;
                    Notification(const Notification&) = delete;
                    Notification& operator=(const Notification&) = delete;

                public:
                explicit Notification(LifecycleManager* parent)
                    : _parent(*parent)
                    {
                        ASSERT(parent != nullptr);
                    }

                    virtual ~Notification()
                    {
                    }

                    BEGIN_INTERFACE_MAP(Notification)
                    INTERFACE_ENTRY(Exchange::ILifecycleManagerState::INotification)
                    INTERFACE_ENTRY(RPC::IRemoteConnection::INotification)
                    END_INTERFACE_MAP

                    void Activated(RPC::IRemoteConnection*) override
                    {
                        LOGINFO("LifecycleManagerState Notification Activated");
                    }

                    void Deactivated(RPC::IRemoteConnection *connection) override
                    {
                        LOGINFO("LifecycleManagerState Notification Deactivated");
                    }

                    void OnAppLifecycleStateChanged(const string& appId,
                                                const string& appInstanceId,
                                                const Exchange::ILifecycleManager::LifecycleState oldState,
                                                const Exchange::ILifecycleManager::LifecycleState newState,
                                                const string& navigationIntent) override
                    {
                        LOGINFO("LifecycleManagerState onAppLifecycleStateChanged");
                        Exchange::JLifecycleManagerState::Event::OnAppLifecycleStateChanged(_parent, appId,appInstanceId,oldState, newState, navigationIntent);
                    }

                private:
                    LifecycleManager& _parent;
            };

            public:
                LifecycleManager(const LifecycleManager&) = delete;
                LifecycleManager& operator=(const LifecycleManager&) = delete;

                LifecycleManager();
                virtual ~LifecycleManager();

                BEGIN_INTERFACE_MAP(LifecycleManager)
                INTERFACE_ENTRY(PluginHost::IPlugin)
                INTERFACE_ENTRY(PluginHost::IDispatcher)
                INTERFACE_AGGREGATE(Exchange::ILifecycleManager, mLifecycleManagerImplementation)
                INTERFACE_AGGREGATE(Exchange::ILifecycleManagerState, mLifecycleManagerState)
                END_INTERFACE_MAP

                /* IPlugin methods  */
                const string Initialize(PluginHost::IShell* service) override;
                void Deinitialize(PluginHost::IShell* service) override;
                string Information() const override;

            public /* constants */:
                static const string SERVICE_NAME;

            public /* members */:
                static LifecycleManager* sInstance;

	    private: /* members */
                PluginHost::IShell* _service{};
                uint32_t mConnectionId;
                Exchange::ILifecycleManager* mLifecycleManagerImplementation;
                Exchange::ILifecycleManagerState* mLifecycleManagerState;
                Core::Sink<Notification> mLifecycleManagerStateNotification;
        };
    } /* namespace Plugin */
} /* namespace WPEFramework */
