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

#include "AppManager.h"

const string WPEFramework::Plugin::AppManager::SERVICE_NAME = "org.rdk.AppManager";

namespace WPEFramework
{

    namespace {

        static Plugin::Metadata<Plugin::AppManager> metadata(
            // Version (Major, Minor, Patch)
            APP_MANAGER_API_VERSION_NUMBER_MAJOR, APP_MANAGER_API_VERSION_NUMBER_MINOR, APP_MANAGER_API_VERSION_NUMBER_PATCH,
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
     *Register AppManager module as wpeframework plugin
     **/
    SERVICE_REGISTRATION(AppManager, APP_MANAGER_API_VERSION_NUMBER_MAJOR, APP_MANAGER_API_VERSION_NUMBER_MINOR, APP_MANAGER_API_VERSION_NUMBER_PATCH);

    AppManager* AppManager::_instance = nullptr;


    AppManager::AppManager() :
        mCurrentService(nullptr),
        mConnectionId(0),
        mAppManagerImpl(nullptr),
        mAppManagerNotification(this),
        mAppManagerConfigure(nullptr)
    {
        SYSLOG(Logging::Startup, (_T("AppManager Constructor")));
        if (nullptr == AppManager::_instance)
        {
            AppManager::_instance = this;
        }
    }

    AppManager::~AppManager()
    {
        SYSLOG(Logging::Shutdown, (string(_T("AppManager Destructor"))));
        AppManager::_instance = nullptr;
    }

    const string AppManager::Initialize(PluginHost::IShell* service)
    {
        string message="";

        ASSERT(nullptr != service);
        ASSERT(nullptr == mCurrentService);
        ASSERT(nullptr == mAppManagerImpl);
        ASSERT(0 == mConnectionId);

        SYSLOG(Logging::Startup, (_T("AppManager::Initialize: PID=%u"), getpid()));

        mCurrentService = service;
        if (nullptr != mCurrentService)
        {
            mCurrentService->AddRef();
            mCurrentService->Register(&mAppManagerNotification);

            if (nullptr == (mAppManagerImpl = mCurrentService->Root<Exchange::IAppManager>(mConnectionId, 5000, _T("AppManagerImplementation"))))
            {
                SYSLOG(Logging::Startup, (_T("AppManager::Initialize: object creation failed")));
                message = _T("AppManager plugin could not be initialised");
            }
            else if (nullptr == (mAppManagerConfigure = mAppManagerImpl->QueryInterface<Exchange::IConfiguration>()))
            {
                SYSLOG(Logging::Startup, (_T("AppManager::Initialize: did not provide a configuration interface")));
                message = _T("AppManager implementation did not provide a configuration interface");
            }
            else
            {
                // Register for notifications
                mAppManagerImpl->Register(&mAppManagerNotification);
                // Invoking Plugin API register to wpeframework
                Exchange::JAppManager::Register(*this, mAppManagerImpl);

                if (Core::ERROR_NONE != mAppManagerConfigure->Configure(mCurrentService))
                {
                    SYSLOG(Logging::Startup, (_T("AppManager::Initialize: could not be configured")));
                    message = _T("AppManager could not be configured");
                }
            }
        }
        else
        {
            SYSLOG(Logging::Startup, (_T("AppManager::Initialize: service is not valid")));
            message = _T("AppManager plugin could not be initialised");
        }

        if (0 != message.length())
        {
            Deinitialize(service);
        }

        return message;
    }

    void AppManager::Deinitialize(PluginHost::IShell* service)
    {
        ASSERT(mCurrentService == service);

        SYSLOG(Logging::Shutdown, (string(_T("AppManager::Deinitialize"))));

        // Make sure the Activated and Deactivated are no longer called before we start cleaning up..
        mCurrentService->Unregister(&mAppManagerNotification);

        if (nullptr != mAppManagerImpl)
        {
            mAppManagerImpl->Unregister(&mAppManagerNotification);
            Exchange::JAppManager::Unregister(*this);

            if (nullptr != mAppManagerConfigure)
            {
                mAppManagerConfigure->Release();
                mAppManagerConfigure = nullptr;
            }

            // Stop processing:
            RPC::IRemoteConnection* connection = service->RemoteConnection(mConnectionId);
            VARIABLE_IS_NOT_USED uint32_t result = mAppManagerImpl->Release();

            mAppManagerImpl = nullptr;

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
        SYSLOG(Logging::Shutdown, (string(_T("AppManager de-initialised"))));
    }

    string AppManager::Information() const
    {
        // No additional info to report
        return (string());
    }

    void AppManager::Deactivated(RPC::IRemoteConnection* connection)
    {
        if (connection->Id() == mConnectionId)
        {
            ASSERT(nullptr != mCurrentService);
            Core::IWorkerPool::Instance().Submit(PluginHost::IShell::Job::Create(mCurrentService, PluginHost::IShell::DEACTIVATED, PluginHost::IShell::FAILURE));
        }
    }
} /* namespace Plugin */
} /* namespace WPEFramework */
