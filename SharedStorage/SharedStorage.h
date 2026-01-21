/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2022 RDK Management
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
#include <interfaces/json/JSharedStorage.h>
#include <interfaces/IConfiguration.h>
#include <interfaces/json/JSharedStorageInspector.h>
#include <interfaces/json/JSharedStorageLimit.h>
#include <interfaces/json/JSharedStorageCache.h>

namespace WPEFramework {
namespace Plugin {

    class SharedStorage : public PluginHost::IPlugin, public PluginHost::JSONRPC {
    private:
        class Store2Notification : public Exchange::ISharedStorage::INotification {
        private:
            Store2Notification(const Store2Notification&) = delete;
            Store2Notification& operator=(const Store2Notification&) = delete;

        public:
            explicit Store2Notification(SharedStorage& parent)
                : _parent(parent)
            {
            }
            ~Store2Notification() override = default;

        public:
            void OnValueChanged(const Exchange::ISharedStorage::ScopeType scope, const string& ns, const string& key, const string& value) override
            {
		SYSLOG(Logging::Startup, (_T("SharedStorage OnValueChanged")));
                Exchange::JSharedStorage::Event::OnValueChanged(_parent, scope, ns, key, value);
            }

            BEGIN_INTERFACE_MAP(Store2Notification)
            INTERFACE_ENTRY(Exchange::ISharedStorage::INotification)
            END_INTERFACE_MAP

        private:
            SharedStorage& _parent;
        };

        class RemoteConnectionNotification : public RPC::IRemoteConnection::INotification {
        private:
            RemoteConnectionNotification() = delete;
            RemoteConnectionNotification(const RemoteConnectionNotification&) = delete;
            RemoteConnectionNotification& operator=(const RemoteConnectionNotification&) = delete;

        public:
            explicit RemoteConnectionNotification(SharedStorage& parent)
                : _parent(parent)
            {
            }
            ~RemoteConnectionNotification() override = default;

            BEGIN_INTERFACE_MAP(RemoteConnectionNotification)
            INTERFACE_ENTRY(RPC::IRemoteConnection::INotification)
            END_INTERFACE_MAP

            void Activated(RPC::IRemoteConnection*) override
            {
            }
            void Deactivated(RPC::IRemoteConnection* connection) override
            {
                if (connection->Id() == _parent._connectionId) {
                    ASSERT(_parent._service != nullptr);
                    Core::IWorkerPool::Instance().Schedule(
                        Core::Time::Now(),
                        PluginHost::IShell::Job::Create(
                            _parent._service, PluginHost::IShell::DEACTIVATED, PluginHost::IShell::FAILURE));
                }
            }

        private:
            SharedStorage& _parent;
        };

    private:
    SharedStorage(const SharedStorage&) = delete;
    SharedStorage& operator=(const SharedStorage&) = delete;

    public:
    SharedStorage()
            : PluginHost::JSONRPC()
            , _service(nullptr)
            , _connectionId(0)
            , _store2(nullptr)
            , _store2Sink(*this)
            , _notification(*this)
            , _storeInspector(nullptr)
            , _storeLimit(nullptr)
            , _storeCache(nullptr)
        {
        }
        ~SharedStorage() override = default;

        BEGIN_INTERFACE_MAP(SharedStorage)
        INTERFACE_ENTRY(PluginHost::IPlugin)
        INTERFACE_ENTRY(PluginHost::IDispatcher)
        INTERFACE_AGGREGATE(Exchange::ISharedStorage, _store2)
        INTERFACE_AGGREGATE(Exchange::ISharedStorageInspector, _storeInspector)
        INTERFACE_AGGREGATE(Exchange::ISharedStorageLimit, _storeLimit)
        INTERFACE_AGGREGATE(Exchange::ISharedStorageCache, _storeCache)
        END_INTERFACE_MAP

    public:
        const string Initialize(PluginHost::IShell* service) override;
        void Deinitialize(PluginHost::IShell* service) override;
        string Information() const override;

    private:
        PluginHost::IShell* _service;
        uint32_t _connectionId;
        Exchange::ISharedStorage* _store2;
        Core::Sink<Store2Notification> _store2Sink;
        Core::Sink<RemoteConnectionNotification> _notification;
        Exchange::IConfiguration* configure;
        Exchange::ISharedStorageInspector* _storeInspector;
        Exchange::ISharedStorageLimit* _storeLimit;
        Exchange::ISharedStorageCache* _storeCache;
    };

} // namespace Plugin
} // namespace WPEFramework
