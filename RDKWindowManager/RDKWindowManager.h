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
#include <interfaces/json/JsonData_RDKWindowManager.h>
#include <interfaces/json/JRDKWindowManager.h>
#include <interfaces/IRDKWindowManager.h>
#include "UtilsLogging.h"
#include "tracing/Logging.h"
#include <mutex>

namespace WPEFramework {

    namespace Plugin {

        class RDKWindowManager : public PluginHost::IPlugin, public PluginHost::JSONRPC {
        private:
            class Notification : public RPC::IRemoteConnection::INotification,
                                 public Exchange::IRDKWindowManager::INotification
            {
                private:
                    Notification() = delete;
                    Notification(const Notification&) = delete;
                    Notification& operator=(const Notification&) = delete;

                public:
                explicit Notification(RDKWindowManager* parent)
                    : _parent(*parent)
                    {
                        ASSERT(parent != nullptr);
                    }

                    virtual ~Notification()
                    {
                    }

                    BEGIN_INTERFACE_MAP(Notification)
                    INTERFACE_ENTRY(Exchange::IRDKWindowManager::INotification)
                    INTERFACE_ENTRY(RPC::IRemoteConnection::INotification)
                    END_INTERFACE_MAP

                    void Activated(RPC::IRemoteConnection*) override
                    {
                    }

                    void Deactivated(RPC::IRemoteConnection *connection) override
                    {
                       _parent.Deactivated(connection);
                    }

                    void OnUserInactivity(const double minutes) override
                    {
                        LOGINFO("OnUserInactivity");
                        Exchange::JRDKWindowManager::Event::OnUserInactivity(_parent, minutes);
                    }

                private:
                    RDKWindowManager& _parent;
            };

        public:
            RDKWindowManager(const RDKWindowManager&) = delete;
            RDKWindowManager& operator=(const RDKWindowManager&) = delete;

            RDKWindowManager();
            virtual ~RDKWindowManager();

            BEGIN_INTERFACE_MAP(RDKWindowManager)
            INTERFACE_ENTRY(PluginHost::IPlugin)
            INTERFACE_ENTRY(PluginHost::IDispatcher)
            INTERFACE_AGGREGATE(Exchange::IRDKWindowManager, mRDKWindowManagerImpl)
            END_INTERFACE_MAP

            /* IPlugin methods  */
            const string Initialize(PluginHost::IShell* service) override;
            void Deinitialize(PluginHost::IShell* service) override;
            string Information() const override;

        private:
            void Deactivated(RPC::IRemoteConnection* connection);

        private: /* members */
            PluginHost::IShell* mCurrentService{};
            uint32_t mConnectionId{};
            Exchange::IRDKWindowManager* mRDKWindowManagerImpl{};
            Core::Sink<Notification> mRDKWindowManagerNotification;

        public /* constants */:
            static const string SERVICE_NAME;

        public /* members */:
            static RDKWindowManager* _instance;
        };

    } /* namespace Plugin */
} /* namespace WPEFramework */
