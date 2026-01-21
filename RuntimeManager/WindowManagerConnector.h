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
#include "UtilsLogging.h"
#include "tracing/Logging.h"
#include <utility>

namespace WPEFramework {
namespace Plugin {
    class WindowManagerConnector
    {
        class WindowManagerNotification : public Exchange::IRDKWindowManager::INotification
        {
            private:
                WindowManagerNotification(const WindowManagerNotification&) = delete;
                WindowManagerNotification& operator=(const WindowManagerNotification&) = delete;

            public:
                explicit WindowManagerNotification(WindowManagerConnector& parent)
                    : _parent(parent)
                {
                }

                ~WindowManagerNotification() override = default;
                BEGIN_INTERFACE_MAP(WindowManagerNotification)
                INTERFACE_ENTRY(Exchange::IRDKWindowManager::INotification)
                END_INTERFACE_MAP

                virtual void OnUserInactivity(const double minutes) override;

            private:
                WindowManagerConnector& _parent;
        };
        public:
            WindowManagerConnector();
            ~WindowManagerConnector();

            // We do not allow this plugin to be copied !!
            WindowManagerConnector(const WindowManagerConnector&) = delete;
            WindowManagerConnector& operator=(const WindowManagerConnector&) = delete;

        public:
            bool initializePlugin(PluginHost::IShell* service); //, IEventHandler* eventHandler
            void releasePlugin();
            bool createDisplay(const string& appInstanceId , const string& displayName, const uint32_t& userId, const uint32_t& groupId);
            bool isPluginInitialized();
            void getDisplayInfo(const string& appInstanceId , string& xdgRuntimeDir , string& waylandDisplayName);\
        private:
            Exchange::IRDKWindowManager* mWindowManager;
            Core::Sink<WindowManagerNotification> mWindowManagerNotification;
            bool mPluginInitialized = false;
    };
} // namespace Plugin
} // namespace WPEFramework
