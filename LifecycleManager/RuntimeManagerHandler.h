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

#include <map>
#include <interfaces/IRuntimeManager.h>
#include <plugins/plugins.h>
#include "IEventHandler.h"
#include "ApplicationContext.h"

namespace WPEFramework {
namespace Plugin {
    class RuntimeManagerHandler
    {
        class RuntimeManagerNotification : public Exchange::IRuntimeManager::INotification
	{
            private:
                RuntimeManagerNotification(const RuntimeManagerNotification&) = delete;
                RuntimeManagerNotification& operator=(const RuntimeManagerNotification&) = delete;

            public:
                explicit RuntimeManagerNotification(RuntimeManagerHandler& parent)
                    : _parent(parent)
                {
                }

                virtual void OnStarted(const string& appInstanceId) override;
                virtual void OnTerminated(const string& appInstanceId) override;
                virtual void OnFailure(const string& appInstanceId, const string& error) override;
                virtual void OnStateChanged(const string& appInstanceId, Exchange::IRuntimeManager::RuntimeState state) override;

                ~RuntimeManagerNotification() override = default;
                BEGIN_INTERFACE_MAP(RuntimeManagerNotification)
                INTERFACE_ENTRY(Exchange::IRuntimeManager::INotification)
                END_INTERFACE_MAP

            private:
                RuntimeManagerHandler& _parent;
        };

        public:
            RuntimeManagerHandler();
            ~RuntimeManagerHandler();

            // We do not allow this plugin to be copied !!
            RuntimeManagerHandler(const RuntimeManagerHandler&) = delete;
            RuntimeManagerHandler& operator=(const RuntimeManagerHandler&) = delete;

        public:
            bool initialize(PluginHost::IShell* service, IEventHandler* eventHandler);
	    void deinitialize();
            bool run(const string& appId, const string& appInstanceId, const string& launchArgs, Exchange::ILifecycleManager::LifecycleState targetState, WPEFramework::Exchange::RuntimeConfig& runtimeConfigObject, string& errorReason);
            bool kill(const string& appInstanceId, string& errorReason);
            bool terminate(const string& appInstanceId, string& errorReason);
            bool suspend(const string& appInstanceId, string& errorReason);
            bool resume(const string& appInstanceId, string& errorReason);
            bool hibernate(const string& appInstanceId, string& errorReason);
            bool wake(const string& appInstanceId, Exchange::ILifecycleManager::LifecycleState state, string& errorReason);
            bool getRuntimeStats(const string& appInstanceId, string& info);
            void onEvent(JsonObject& data);

        private:
            Exchange::IRuntimeManager* mRuntimeManager;
            Core::Sink<RuntimeManagerNotification> mRuntimeManagerNotification;
            uint32_t mFireboltAccessPort;
            IEventHandler* mEventHandler;
    };
} // namespace Plugin
} // namespace WPEFramework
