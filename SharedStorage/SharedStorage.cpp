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

#include "SharedStorage.h"

#define API_VERSION_NUMBER_MAJOR 1
#define API_VERSION_NUMBER_MINOR 0
#define API_VERSION_NUMBER_PATCH 1

namespace WPEFramework {

namespace Plugin {

    namespace {

        static Metadata<SharedStorage> metadata(
            // Version (Major, Minor, Patch)
            API_VERSION_NUMBER_MAJOR, API_VERSION_NUMBER_MINOR, API_VERSION_NUMBER_PATCH,
            // Preconditions
            {},
            // Terminations
            {},
            // Controls
            {});
    }

    SERVICE_REGISTRATION(SharedStorage, API_VERSION_NUMBER_MAJOR, API_VERSION_NUMBER_MINOR, API_VERSION_NUMBER_PATCH);

    const string SharedStorage::Initialize(PluginHost::IShell* service)
    {
        string message="";

        ASSERT(service != nullptr);
        ASSERT(_store2 == nullptr);
        ASSERT(_storeInspector == nullptr);
        ASSERT(_storeLimit == nullptr);
        ASSERT(_storeCache == nullptr);
        ASSERT(_service == nullptr);
        ASSERT(_connectionId == 0);

        SYSLOG(Logging::Startup, (_T("SharedStorage::Initialize: PID=%u"), getpid()));

        _service = service;
        _service->AddRef();

        _service->Register(&_notification);

        _store2 = _service->Root<Exchange::ISharedStorage>(_connectionId, RPC::CommunicationTimeOut, _T("SharedStorageImplementation"));
        if (_store2 != nullptr) {
            configure = _store2->QueryInterface<Exchange::IConfiguration>();
            if (configure != nullptr)
            {
                uint32_t result = configure->Configure(_service);
                if(result != Core::ERROR_NONE)
                {
                    message = _T("SharedStorage could not be configured");
                }
            }
            else
            {
                message = _T("SharedStorage implementation did not provide a configuration interface");
            }

            Exchange::JSharedStorage::Register(*this, _store2);
            _store2->Register(&_store2Sink);
        } 
        else {
            message = _T("Couldn't create _store2 implementation instance");
        }
        if(message != "")
        {
            return message;
        }

        _storeInspector = _store2->QueryInterface<Exchange::ISharedStorageInspector>();
        if (_storeInspector != nullptr) {
            Exchange::JSharedStorageInspector::Register(*this, _storeInspector);
        } 
        else {
            message = _T("Couldn't create _storeInspector implementation instance - 2");
            return message;
        }

        _storeLimit = _store2->QueryInterface<Exchange::ISharedStorageLimit>();
        if (_storeLimit != nullptr) {
            Exchange::JSharedStorageLimit::Register(*this, _storeLimit);
        } 
        else {
            message = _T("Couldn't create _storeLimit implementation instance");
            return message;
        }
        _storeCache = _store2->QueryInterface<Exchange::ISharedStorageCache>();
        if (_storeCache != nullptr) {
            Exchange::JSharedStorageCache::Register(*this, _storeCache);
        } 
        else {
            message = _T("Couldn't create _storeCache implementation instance");
            return message;
        }

        return message;
    }

    void SharedStorage::Deinitialize(PluginHost::IShell* service)
    {
        ASSERT(_service == service);

        SYSLOG(Logging::Shutdown, (string(_T("SharedStorage::Deinitialize"))));

        _service->Unregister(&_notification);

        if (_store2 != nullptr) {
            _store2->Unregister(&_store2Sink);
            Exchange::JSharedStorage::Unregister(*this);
            configure->Release();
            configure = nullptr;

            auto connection = _service->RemoteConnection(_connectionId);
            VARIABLE_IS_NOT_USED auto result = _store2->Release();
            _store2 = nullptr;
            ASSERT(result == Core::ERROR_DESTRUCTION_SUCCEEDED);
            if (connection != nullptr) {
                connection->Terminate();
                connection->Release();
            }
        }
        if (_storeInspector != nullptr) {
            Exchange::JSharedStorageInspector::Unregister(*this);
        }
        if (_storeLimit != nullptr) {
            Exchange::JSharedStorageLimit::Unregister(*this);
        }
        if (_storeCache != nullptr) {
            Exchange::JSharedStorageCache::Unregister(*this);
        }

        _connectionId = 0;
        _service->Release();
        _service = nullptr;
        SYSLOG(Logging::Shutdown, (string(_T("SharedStorage de-initialised"))));
    }

    string SharedStorage::Information() const
    {
        return (string());
    }

} // namespace Plugin
} // namespace WPEFramework
