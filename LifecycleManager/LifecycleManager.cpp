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

#include "LifecycleManager.h"
#include <interfaces/IConfiguration.h>

const string WPEFramework::Plugin::LifecycleManager::SERVICE_NAME = "org.rdk.LifecycleManager";

namespace WPEFramework
{
    namespace {

        static Plugin::Metadata<Plugin::LifecycleManager> metadata(
            /* Version (Major, Minor, Patch) */
            LIFECYCLE_MANAGER_API_VERSION_NUMBER_MAJOR, LIFECYCLE_MANAGER_API_VERSION_NUMBER_MINOR, LIFECYCLE_MANAGER_API_VERSION_NUMBER_PATCH,
            /* Preconditions */
            {},
            /* Terminations */
            {},
            /* Controls */
            {}
        );
    }

    namespace Plugin
    {

        SERVICE_REGISTRATION(LifecycleManager, LIFECYCLE_MANAGER_API_VERSION_NUMBER_MAJOR, LIFECYCLE_MANAGER_API_VERSION_NUMBER_MINOR, LIFECYCLE_MANAGER_API_VERSION_NUMBER_PATCH);

        LifecycleManager* LifecycleManager::sInstance = nullptr;

        LifecycleManager::LifecycleManager(): _service(nullptr), mConnectionId(0), mLifecycleManagerImplementation(nullptr), mLifecycleManagerState(nullptr), mLifecycleManagerStateNotification(this)
        {
            SYSLOG(Logging::Startup, (_T("LifecycleManager Constructor")));
            LifecycleManager::sInstance = this;
        }

        LifecycleManager::~LifecycleManager()
        {
            SYSLOG(Logging::Startup, (_T("LifecycleManager Destructor")));
            LifecycleManager::sInstance = nullptr;
	    mLifecycleManagerState = nullptr;
	    mLifecycleManagerImplementation = nullptr;
        }

        const string LifecycleManager::Initialize(PluginHost::IShell* service )
        {
            //uint32_t result = Core::ERROR_GENERAL;
            string retStatus = "";
            ASSERT(nullptr != service);
            ASSERT(0 == mConnectionId);
            ASSERT(nullptr == _service);
            SYSLOG(Logging::Startup, (_T("LifecycleManager::Initialize: PID=%u"), getpid()));
            //{
            //    SYSLOG(Logging::Startup, (_T("LifecycleManager::Initialize: service is not valid")));
            //    retStatus = _T("LifecycleManager plugin could not be initialised");
            //}
	    _service = service;
	    _service->AddRef(); 
	    mLifecycleManagerImplementation = _service->Root<Exchange::ILifecycleManager>(mConnectionId, 5000, _T("LifecycleManagerImplementation"));
            if (nullptr == mLifecycleManagerImplementation)
	    {
                retStatus = "error starting lifecyclemanager";
		return retStatus;
	    }
            auto lifeCycleManagerConfig = mLifecycleManagerImplementation->QueryInterface<Exchange::IConfiguration>();
	    if (lifeCycleManagerConfig != nullptr)
	    {
	        lifeCycleManagerConfig->Configure(service);
	        lifeCycleManagerConfig->Release();
	    }
            mLifecycleManagerState = mLifecycleManagerImplementation->QueryInterface<Exchange::ILifecycleManagerState>();
	    ASSERT(mLifecycleManagerState != nullptr);
            mLifecycleManagerState->Register(&mLifecycleManagerStateNotification);
            Exchange::JLifecycleManagerState::Register(*this, mLifecycleManagerState);

            return retStatus;
        }

        void LifecycleManager::Deinitialize(PluginHost::IShell* service)
        {
            SYSLOG(Logging::Startup, (_T("LifecycleManager::Deinitialize: PID=%u"), getpid()));
            ASSERT(_service == service);
            if (mLifecycleManagerState != nullptr)
	    {
                mLifecycleManagerState->Unregister(&mLifecycleManagerStateNotification);
                Exchange::JLifecycleManagerState::Unregister(*this);
	        mLifecycleManagerState->Release();
	        mLifecycleManagerState = nullptr;
	    }

            if (nullptr != mLifecycleManagerImplementation)
            {
                // Stop processing:
                RPC::IRemoteConnection* connection = service->RemoteConnection(mConnectionId);
                VARIABLE_IS_NOT_USED uint32_t result = mLifecycleManagerImplementation->Release();
                mLifecycleManagerImplementation = nullptr;
                ASSERT(result == Core::ERROR_DESTRUCTION_SUCCEEDED);
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
            _service->Release();
	    _service = nullptr;
            SYSLOG(Logging::Shutdown, (string(_T("LifecycleManager de-initialised"))));
        }

        string LifecycleManager::Information() const
        {
            return(string("{\"service\": \"") + SERVICE_NAME + string("\"}"));
        }
    } /* namespace Plugin */
} /* namespace WPEFramework */
