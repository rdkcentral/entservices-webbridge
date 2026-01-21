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

#include <interfaces/IOCIContainer.h>
#include <plugins/plugins.h>
#include "IEventHandler.h"

namespace WPEFramework {
    namespace Plugin {
        class DobbyEventListener
        {
            class OCIContainerNotification : public Exchange::IOCIContainer::INotification
        {
            private:
                OCIContainerNotification(const OCIContainerNotification&) = delete;
                OCIContainerNotification& operator=(const OCIContainerNotification&) = delete;

            public:
                explicit OCIContainerNotification(DobbyEventListener* parent)
                    : _parent(*parent)
                {
                }

                virtual void OnContainerStarted(const string& containerId, const string& name) override;
                virtual void OnContainerStopped(const string& containerId, const string& name) override;
                virtual void OnContainerFailed(const string& containerId, const string& name, uint32_t error) override;
                virtual void OnContainerStateChanged(const string& containerId, Exchange::IOCIContainer::ContainerState state) override;

                ~OCIContainerNotification() override = default;
                BEGIN_INTERFACE_MAP(OCIContainerNotification)
                INTERFACE_ENTRY(Exchange::IOCIContainer::INotification)
                END_INTERFACE_MAP


            private:
                DobbyEventListener& _parent;
            };

        public:
            DobbyEventListener();
            ~DobbyEventListener();

        public:
            bool initialize(PluginHost::IShell* service, IEventHandler* eventHandler, Exchange::IOCIContainer* ociContainerObject);
            bool deinitialize();

        private:
            Exchange::IOCIContainer* mOCIContainerObject;
            Core::Sink<OCIContainerNotification> mOCIContainerNotification;
            IEventHandler* mEventHandler;

        };
} // namespace Plugin
} // namespace WPEFramework