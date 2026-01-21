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
#include <interfaces/json/JsonData_OCIContainer.h>
#include <interfaces/json/JOCIContainer.h>
#include <interfaces/IOCIContainer.h>
#include "UtilsLogging.h"
#include "tracing/Logging.h"
#include <mutex>

namespace WPEFramework
{
namespace Plugin
{
    class OCIContainer : public PluginHost::IPlugin, public PluginHost::JSONRPC
    {
        private:
            class Notification : public RPC::IRemoteConnection::INotification,
                                    public Exchange::IOCIContainer::INotification
            {
                private:
                    Notification() = delete;
                    Notification(const Notification&) = delete;
                    Notification& operator=(const Notification&) = delete;

                public:
                    explicit Notification(OCIContainer* parent)
                    : _parent(*parent)
                    {
                        ASSERT(parent != nullptr);
                    }

                    virtual ~Notification()
                    {
                    }

                    BEGIN_INTERFACE_MAP(Notification)
                    INTERFACE_ENTRY(Exchange::IOCIContainer::INotification)
                    INTERFACE_ENTRY(RPC::IRemoteConnection::INotification)
                    END_INTERFACE_MAP

                    void Activated(RPC::IRemoteConnection*) override
                    {
                        LOGINFO("OCIContainer Notification Activated");
                    }

                    void Deactivated(RPC::IRemoteConnection *connection) override
                    {
                        LOGINFO("OCIContainer Notification Deactivated");
                        _parent.Deactivated(connection);
                    }

                    void OnContainerStarted(const string& containerId, const string& name) override
                    {
                        LOGINFO("OnContainerStarted");
                        Exchange::JOCIContainer::Event::OnContainerStarted(_parent, containerId, name);
                    }

                    void OnContainerStopped(const string& containerId, const string& name) override
                    {
                        LOGINFO("OnContainerStopped");
                        Exchange::JOCIContainer::Event::OnContainerStopped(_parent, containerId, name);
                    }

                    void OnContainerFailed(const string& containerId, const string& name, uint32_t error) override
                    {
                        LOGINFO("OnContainerFailed");
                        Exchange::JOCIContainer::Event::OnContainerFailed(_parent, containerId, name, error);
                    }

                    void OnContainerStateChanged(const string& containerId, Exchange::IOCIContainer::ContainerState state) override
                    {
                        LOGINFO("OnContainerStateChanged");
                        Exchange::JOCIContainer::Event::OnContainerStateChanged(_parent, containerId, state);
                    }

                private:
                    OCIContainer& _parent;
            };
        public:
            OCIContainer(const OCIContainer&) = delete;
            OCIContainer& operator=(const OCIContainer&) = delete;

            OCIContainer();
            virtual ~OCIContainer();

            BEGIN_INTERFACE_MAP(OCIContainer)
            INTERFACE_ENTRY(PluginHost::IPlugin)
            INTERFACE_ENTRY(PluginHost::IDispatcher)
            INTERFACE_AGGREGATE(Exchange::IOCIContainer, mOCIContainerImplementation)
            END_INTERFACE_MAP

            /* IPlugin methods  */
            const string Initialize(PluginHost::IShell* service) override;
            void Deinitialize(PluginHost::IShell* service) override;
            string Information() const override;

        public /* constants */:
            static const string SERVICE_NAME;

        public /* members */:
            static OCIContainer* sInstance;

        private:
            void Deactivated(RPC::IRemoteConnection* connection);

        private: /* members */
            PluginHost::IShell* _service{};
            uint32_t mConnectionId;
            Exchange::IOCIContainer* mOCIContainerImplementation;
            Core::Sink<Notification> mOCIContainerNotification;
    };
} /* namespace Plugin */
} /* namespace WPEFramework */
