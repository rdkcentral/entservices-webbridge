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
#include <interfaces/IRDKWindowManager.h>
#include <map>
#include <plugins/plugins.h>
#include "IEventHandler.h"
#include "UtilsLogging.h"
#include "tracing/Logging.h"
#include <utility>

namespace WPEFramework {
namespace Plugin {
    class WindowManagerHandler
    {
        class WindowManagerNotification : public Exchange::IRDKWindowManager::INotification
        {
            private:
                WindowManagerNotification(const WindowManagerNotification&) = delete;
                WindowManagerNotification& operator=(const WindowManagerNotification&) = delete;

            public:
                explicit WindowManagerNotification(WindowManagerHandler& parent)
                    : _parent(parent)
                {
                }

                ~WindowManagerNotification() override = default;
                BEGIN_INTERFACE_MAP(WindowManagerNotification)
                INTERFACE_ENTRY(Exchange::IRDKWindowManager::INotification)
                END_INTERFACE_MAP

                virtual void OnUserInactivity(const double minutes) override;
                virtual void OnDisconnected(const std::string& client) override;
                virtual void OnReady(const std::string &client) override;

            private:
                WindowManagerHandler& _parent;
        };
        public:
            WindowManagerHandler();
            ~WindowManagerHandler();

            // We do not allow this plugin to be copied !!
            WindowManagerHandler(const WindowManagerHandler&) = delete;
            WindowManagerHandler& operator=(const WindowManagerHandler&) = delete;

        public:
            bool initialize(PluginHost::IShell* service, IEventHandler* eventHandler);
            void terminate();
            void onEvent(JsonObject& data);
            Core::hresult renderReady(std::string appInstanceId, bool& isReady);
            Core::hresult enableDisplayRender(std::string appInstanceId, bool render);
        private:
            Exchange::IRDKWindowManager* mWindowManager;
            Core::Sink<WindowManagerNotification> mWindowManagerNotification;
            IEventHandler* mEventHandler;
    };
} // namespace Plugin
} // namespace WPEFramework
