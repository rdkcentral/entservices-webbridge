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


#include "StorageManager.h"

const string WPEFramework::Plugin::StorageManager::SERVICE_NAME = "org.rdk.StorageManager";

namespace WPEFramework
{

    namespace {

        static Plugin::Metadata<Plugin::StorageManager> metadata(
            // Version (Major, Minor, Patch)
            STORAGE_MANAGER_API_VERSION_NUMBER_MAJOR, STORAGE_MANAGER_API_VERSION_NUMBER_MINOR, STORAGE_MANAGER_API_VERSION_NUMBER_PATCH,
            // Preconditions
            {},
            // Terminations
            {},
            // Controls
            {}
        );
    }

    namespace Plugin
    {

    /*
     *Register StorageManager module as wpeframework plugin
     **/
    SERVICE_REGISTRATION(StorageManager, STORAGE_MANAGER_API_VERSION_NUMBER_MAJOR, STORAGE_MANAGER_API_VERSION_NUMBER_MINOR, STORAGE_MANAGER_API_VERSION_NUMBER_PATCH);

    StorageManager::StorageManager() :
        mCurrentService(nullptr),
        mConnectionId(0),
        mStorageManagerImpl(nullptr),
        mConfigure(nullptr)
    {
        SYSLOG(Logging::Startup, (_T("StorageManager Constructor")));
    }

    StorageManager::~StorageManager()
    {
        SYSLOG(Logging::Shutdown, (string(_T("StorageManager Destructor"))));
    }

    const string StorageManager::Initialize(PluginHost::IShell* service)
    {
        string message="";

        ASSERT(nullptr != service);
        ASSERT(nullptr == mCurrentService);
        ASSERT(nullptr == mStorageManagerImpl);
        ASSERT(0 == mConnectionId);

        SYSLOG(Logging::Startup, (_T("StorageManager::Initialize: PID=%u"), getpid()));
        mCurrentService = service;
        if (nullptr != mCurrentService)
        {
            mCurrentService->AddRef();
            if (nullptr == (mStorageManagerImpl = mCurrentService->Root<Exchange::IStorageManager>(mConnectionId, 5000, _T("StorageManagerImplementation"))))
            {
                SYSLOG(Logging::Startup, (_T("StorageManager::Initialize: object creation failed")));
                message = _T("StorageManager plugin could not be initialised");
            }
            else
            {
                mConfigure = mStorageManagerImpl->QueryInterface<Exchange::IConfiguration>();
                if (mConfigure != nullptr)
                {
                    uint32_t result = mConfigure->Configure(mCurrentService);
                    if(result != Core::ERROR_NONE)
                    {
                        message = _T("mStorageManagerImpl could not be configured");
                    }
                }
                else
                {
                    message = _T("mStorageManagerImpl implementation did not provide a configuration interface");
                }
                // Invoking Plugin API register to wpeframework
                Exchange::JStorageManager::Register(*this, mStorageManagerImpl);
            }
        }
        else
        {
            SYSLOG(Logging::Startup, (_T("StorageManager::Initialize: service is not valid")));
            message = _T("StorageManager plugin could not be initialised");
        }

        if (0 != message.length())
        {
            Deinitialize(service);
        }

        return message;
    }

    void StorageManager::Deinitialize(PluginHost::IShell* service)
    {
        ASSERT(mCurrentService == service);

        SYSLOG(Logging::Shutdown, (string(_T("StorageManager::Deinitialize"))));

        if (mConfigure != nullptr)
        {
            mConfigure->Release();
            mConfigure = nullptr;
        }
        if (nullptr != mStorageManagerImpl)
        {
            Exchange::JStorageManager::Unregister(*this);

            // Stop processing:
            RPC::IRemoteConnection* connection = service->RemoteConnection(mConnectionId);
            VARIABLE_IS_NOT_USED uint32_t result = mStorageManagerImpl->Release();

            mStorageManagerImpl = nullptr;

            // It should have been the last reference we are releasing,
            // so it should endup in a DESTRUCTION_SUCCEEDED, if not we
            // are leaking...
            ASSERT(result == Core::ERROR_DESTRUCTION_SUCCEEDED);

            // If this was running in a (container) process...
            if (nullptr != connection)
            {
               // Lets trigger the cleanup sequence for
               // out-of-process code. Which will guard
               // that unwilling processes, get shot if
               // not stopped friendly :-)
               connection->Terminate();
               connection->Release();
            }
        }

        mConnectionId = 0;
        mCurrentService->Release();
        mCurrentService = nullptr;
        SYSLOG(Logging::Shutdown, (string(_T("StorageManager de-initialised"))));
    }

    string StorageManager::Information() const
    {
        // No additional info to report
        return (string());
    }
} /* namespace Plugin */
} /* namespace WPEFramework */
