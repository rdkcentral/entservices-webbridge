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

#include "SharedStorageImplementation.h"
#include "UtilsLogging.h"

namespace WPEFramework {
namespace Plugin {

    SERVICE_REGISTRATION(SharedStorageImplementation, 1, 0);

    SharedStorageImplementation::SharedStorageImplementation() : 
                                _adminLock()
                                , _service(nullptr)
                                , _storeNotification(*this)
                                , _psObject(nullptr)
                                , _psInspector(nullptr)
                                , _psLimit(nullptr)
                                , _psCache(nullptr)
                                , _csObject(nullptr)
    {
        LOGINFO("SharedStorageImplementation constructor - 3");
    }

    SharedStorageImplementation::~SharedStorageImplementation()
    {
        LOGINFO("SharedStorageImplementation destructor");
        if(_psObject)
        {
            _psObject->Release();
        }
        if(_psInspector)
        {
            _psInspector->Release();
        }
        if(_psLimit)
        {
            _psLimit->Release();
        }
        if(_psCache)
        {
            _psCache->Release();
        }
        if(_csObject)
        {
            _csObject->Release();
        }
        if (_service != nullptr)
        {
           _service->Release();
           _service = nullptr;
        }
     }

    SharedStorageImplementation* SharedStorageImplementation::instance(SharedStorageImplementation *SharedStorageImpl)
    {
        LOGINFO("SharedStorageImplementation instance");
        static SharedStorageImplementation *SharedStorageImpl_instance = nullptr;

        if (SharedStorageImpl != nullptr)
        {
            SharedStorageImpl_instance = SharedStorageImpl;
        }
        else
        {
            LOGERR("SharedStorageImpl is null \n");
        }
        return(SharedStorageImpl_instance);
    }

    uint32_t SharedStorageImplementation::Register(ISharedStorage::INotification* notification)
    {
        // Make sure we can't register the same notification callback multiple times
        if (std::find(_sharedStorageNotification.begin(), _sharedStorageNotification.end(), notification) == _sharedStorageNotification.end())
        {
            LOGINFO("Register notification");
            _sharedStorageNotification.push_back(notification);
            notification->AddRef();
        }
        else
        {
            LOGERR("notification is already available in the _sharedStorageNotification");
        }

        return Core::ERROR_NONE;
    }

    uint32_t SharedStorageImplementation::Unregister(ISharedStorage::INotification* notification)
    {
        uint32_t status = Core::ERROR_GENERAL;

        // Make sure we can't unregister the same notification callback multiple times
        auto itr = std::find(_sharedStorageNotification.begin(), _sharedStorageNotification.end(), notification);

        if (itr != _sharedStorageNotification.end())
        {
            (*itr)->Release();
            LOGINFO("Unregister notification");
            _sharedStorageNotification.erase(itr);
            status = Core::ERROR_NONE;
        }
        else
        {
            LOGERR("notification not found");
        }

        return status;
    }

    uint32_t SharedStorageImplementation::Configure(PluginHost::IShell* service)
    {
        uint32_t result = Core::ERROR_GENERAL;

        if (service != nullptr)
        {
            _service = service;
            _service->AddRef();
            result = Core::ERROR_NONE;
    
            // Get interface for IStore2
            _psObject = service->QueryInterfaceByCallsign<WPEFramework::Exchange::IStore2>("org.rdk.PersistentStore");
            if (_psObject != nullptr)
            {
                _psObject->Register(&_storeNotification);
            }
            else
            {
                LOGERR("_psObject is null \n");
            }
            _psInspector = service->QueryInterfaceByCallsign<WPEFramework::Exchange::IStoreInspector>("org.rdk.PersistentStore");
            if(_psInspector == nullptr)
            {
                LOGERR("_psInspector is null \n");
            }
            else
            {
                LOGINFO("_psInspector success ");
            }
            _psLimit = service->QueryInterfaceByCallsign<WPEFramework::Exchange::IStoreLimit>("org.rdk.PersistentStore");
            if(_psLimit == nullptr)
            {
                LOGERR("_psLimit is null \n");
            }
            else
            {
                LOGINFO("_psLimit success ");
            }
            _psCache = service->QueryInterfaceByCallsign<WPEFramework::Exchange::IStoreCache>("org.rdk.PersistentStore");
            if(_psCache == nullptr)
            {
                LOGERR("_psCache is null \n");
            }
            else
            {
                LOGINFO("_psCache success ");
            }

            // Establish communication with CloudStore
            _csObject = service->QueryInterfaceByCallsign<WPEFramework::Exchange::IStore2>("org.rdk.CloudStore");
            if (_csObject != nullptr)
            {
                _csObject->Register(&_storeNotification);
            }
            else
            {
                LOGERR("_csObject is null \n");
	    }
        }
        else
        {
            LOGERR("service is null \n");
        }
    
        return result;
    }

    Exchange::IStore2* SharedStorageImplementation::getRemoteStoreObject(const Exchange::ISharedStorageLimit::ScopeType eScope)
    {
        if( (eScope == WPEFramework::Exchange::ISharedStorageLimit::ScopeType::DEVICE) && _psObject)
        {
            return _psObject;
        }
        else if( (eScope == WPEFramework::Exchange::ISharedStorageLimit::ScopeType::ACCOUNT) && _csObject)
        {
            return _csObject;
        }
        else
        {
            TRACE(Trace::Error, (_T("%s: Unknown scope: %d"), __FUNCTION__, static_cast<int>(eScope)));
            return nullptr;
        }
    }

    void SharedStorageImplementation::ValueChanged(const Exchange::ISharedStorage::ScopeType eScope, const string& ns, const string& key, const string& value)
    {
        LOGINFO("ns:%s key:%s value:%s", ns.c_str(), key.c_str(), value.c_str());
        JsonObject params;
        params["namespace"] = ns;
        params["key"] = key;
        params["value"] = value;
        params["scope"] = static_cast<uint8_t>(eScope);
        dispatchEvent(SHAREDSTORAGE_EVT_VALUE_CHANGED, params); 
    }
    
    void SharedStorageImplementation::dispatchEvent(Event event, const JsonObject &params)
    {
        Core::IWorkerPool::Instance().Submit(Job::Create(this, event, params));
    }

    void SharedStorageImplementation::Dispatch(Event event, const JsonObject &params)
    {
        _adminLock.Lock();

        std::list<ISharedStorage::INotification*>::const_iterator index(_sharedStorageNotification.begin());

        while (index != _sharedStorageNotification.end()) 
        {
            string ns = params["namespace"].String();
            string key = params["key"].String();
            string value = params["value"].String();
            uint8_t scopeVal = static_cast<uint8_t>(params["scope"].Number());
            Exchange::ISharedStorage::ScopeType scope = static_cast<Exchange::ISharedStorage::ScopeType>(scopeVal);

            (*index)->OnValueChanged(scope,ns,key,value);
            index++;
        }

        _adminLock.Unlock();
}

    uint32_t SharedStorageImplementation::SetValue(const ISharedStorage::ScopeType eScope, const string& ns, const string& key, const string& value, const uint32_t ttl, Success& success)
    {
        uint32_t status = Core::ERROR_NOT_SUPPORTED;
        success.success = false;
        Exchange::IStore2* _storeObject = getRemoteStoreObject(eScope);
        ASSERT (nullptr != _storeObject);
        if (nullptr != _storeObject)
        {
            status = _storeObject->SetValue(
                        (Exchange::IStore2::ScopeType)eScope,
                        ns,
                        key,
                        value,
                        ttl);
            if (status != Core::ERROR_NONE) {
                TRACE(Trace::Error, (_T("%s: Status: %d"), __FUNCTION__, status));
            }
            else
            {
                success.success = true;
            }
        }
        return status;
    }
    uint32_t SharedStorageImplementation::GetValue(const ISharedStorage::ScopeType eScope, const string& ns, const string& key, string& value, uint32_t& ttl, bool& success)
    {
        uint32_t status = Core::ERROR_NOT_SUPPORTED;
        success = false;
        Exchange::IStore2* _storeObject = getRemoteStoreObject(eScope);
        ASSERT (nullptr != _storeObject);
        if (nullptr != _storeObject)
        {
            status = _storeObject->GetValue(
                        (Exchange::IStore2::ScopeType)eScope,
                        ns,
                        key,
                        value,
                        ttl);
            if (status != Core::ERROR_NONE) {
                TRACE(Trace::Error, (_T("%s: Status: %d"), __FUNCTION__, status));
            }
            else
            {
                success = true;
            }
        }
        return status;
    }
    uint32_t SharedStorageImplementation::DeleteKey(const ISharedStorage::ScopeType eScope, const string& ns, const string& key, Success& success)
    {
        uint32_t status = Core::ERROR_NOT_SUPPORTED;
        success.success = false;
        Exchange::IStore2* _storeObject = getRemoteStoreObject(eScope);
        ASSERT (nullptr != _storeObject);
        if (nullptr != _storeObject)
        {
            status = _storeObject->DeleteKey(
                        (Exchange::IStore2::ScopeType)eScope,
                        ns,
                        key);
            if (status != Core::ERROR_NONE) {
                TRACE(Trace::Error, (_T("%s: Status: %d"), __FUNCTION__, status));
            }
            else
            {
                success.success = true;
            }
        }
        return status;
    }
    uint32_t SharedStorageImplementation::DeleteNamespace(const ISharedStorage::ScopeType eScope, const string& ns, Success& success)
    {
        uint32_t status = Core::ERROR_NOT_SUPPORTED;
        success.success = false;
        Exchange::IStore2* _storeObject = getRemoteStoreObject(eScope);
        ASSERT (nullptr != _storeObject);
        if (nullptr != _storeObject)
        {
            status = _storeObject->DeleteNamespace(
                        (Exchange::IStore2::ScopeType)eScope,
                        ns);
            if (status != Core::ERROR_NONE) {
                TRACE(Trace::Error, (_T("%s: Status: %d"), __FUNCTION__, status));
            }
            else
            {
                success.success = true;
            }
        }
        return status;
    }

    uint32_t SharedStorageImplementation::GetKeys(const ISharedStorageInspector::ScopeType eScope, const string& ns, RPC::IStringIterator*& keys, bool& success)
    {
        uint32_t status = Core::ERROR_NOT_SUPPORTED;
        success = false;
        LOGINFO("Scope-2:%d ns:%s", static_cast<int>(eScope), ns.c_str());
        if(eScope != ISharedStorageInspector::ScopeType::DEVICE)
        {
            LOGINFO("Scope is not DEVICE");
            return status;
        }
        status = Core::ERROR_GENERAL;
        ASSERT (nullptr != _psInspector);
        if (nullptr != _psInspector)
        {
            LOGINFO("_psInspector is not null");
            status = _psInspector->GetKeys((Exchange::IStore2::ScopeType)eScope, ns, keys);
            if (status != Core::ERROR_NONE) {
                TRACE(Trace::Error, (_T("%s: Status: %d"), __FUNCTION__, status));
            }
            else
            {
                success = true;
            }
        }
        else
        {
            LOGINFO("_psInspector is null");
        }
        return status;
    }

    uint32_t SharedStorageImplementation::GetNamespaces(const ISharedStorageInspector::ScopeType eScope, RPC::IStringIterator*& namespaces, bool& success)
    {
        uint32_t status = Core::ERROR_NOT_SUPPORTED;
        success = false;
        if(eScope != ISharedStorageInspector::ScopeType::DEVICE)
        {
            return status;
        }
        status = Core::ERROR_GENERAL;
        ASSERT (nullptr != _psInspector);
        if (nullptr != _psInspector)
        {
            status = _psInspector->GetNamespaces((Exchange::IStore2::ScopeType)eScope, namespaces);
            if (status != Core::ERROR_NONE) {
                TRACE(Trace::Error, (_T("%s: Status: %d"), __FUNCTION__, status));
            }
            else
            {
                success = true;
            }
        }
        return status;
    }

    Core::hresult SharedStorageImplementation::GetStorageSizes(const ISharedStorageInspector::ScopeType eScope, INamespaceSizeIterator*& storageList, bool& success)
    {
        Core::hresult status = Core::ERROR_NOT_SUPPORTED;
        success = false;
        if(eScope != ISharedStorageInspector::ScopeType::DEVICE)
        {
            return status;
        }
        status = Core::ERROR_GENERAL;
        ASSERT (nullptr != _psInspector);
        if (nullptr != _psInspector)
        {
            status = _psInspector->GetStorageSizes((Exchange::IStore2::ScopeType)eScope, (WPEFramework::Exchange::IStoreInspector::INamespaceSizeIterator*&)storageList);
            if (status != Core::ERROR_NONE) {
                TRACE(Trace::Error, (_T("%s: Status: %d"), __FUNCTION__, status));
            }
            else
            {
                success = true;
            }
        }
        return status;
    }

    Core::hresult SharedStorageImplementation::SetNamespaceStorageLimit(const ISharedStorageLimit::ScopeType eScope, const string& ns, const uint32_t storageLimit, bool& success)
    {
        Core::hresult status = Core::ERROR_NOT_SUPPORTED;
        if(eScope != ISharedStorageLimit::ScopeType::DEVICE)
        {
            return status;
        }
        status = Core::ERROR_GENERAL;
        ASSERT(nullptr != _psLimit);
        if(_psLimit != nullptr)
        {
            status = _psLimit->SetNamespaceStorageLimit((Exchange::IStore2::ScopeType)eScope, ns, storageLimit);
        }
        success = (status == Core::ERROR_NONE);
        if (status != Core::ERROR_NONE) {
            TRACE(Trace::Error, (_T("%s: Status: %d"), __FUNCTION__, status));
        }
        return status;
    }

    Core::hresult SharedStorageImplementation::GetNamespaceStorageLimit(const ISharedStorageLimit::ScopeType eScope, const string& ns, StorageLimit& storageLimit)
    {
        Core::hresult status = Core::ERROR_NOT_SUPPORTED;
        if(eScope != ISharedStorageLimit::ScopeType::DEVICE)
        {
            return status;
        }
        status = Core::ERROR_GENERAL;
        ASSERT(nullptr != _psLimit);
        if(_psLimit != nullptr)
        {
            status = _psLimit->GetNamespaceStorageLimit((Exchange::IStore2::ScopeType)eScope, ns, storageLimit.storageLimit);
        }

        if (status != Core::ERROR_NONE) {
            TRACE(Trace::Error, (_T("%s: Status: %d"), __FUNCTION__, status));
        }
        return status;
    }

    Core::hresult SharedStorageImplementation::FlushCache()
    {
        Core::hresult status = Core::ERROR_GENERAL;
        ASSERT(nullptr != _psCache);
        if(_psCache != nullptr)
        {
            status = _psCache->FlushCache();
            if (status != Core::ERROR_NONE) {
                TRACE(Trace::Error, (_T("%s: Status: %d"), __FUNCTION__, status));
            }
        }
        return status;
    }

} // namespace Plugin
} // namespace WPEFramework
