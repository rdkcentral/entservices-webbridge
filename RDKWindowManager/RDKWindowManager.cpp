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

#include "RDKWindowManager.h"

const string WPEFramework::Plugin::RDKWindowManager::SERVICE_NAME = "org.rdk.RDKWindowManager";

namespace WPEFramework {

    namespace {

        static Plugin::Metadata<Plugin::RDKWindowManager> metadata(
            /* Version (Major, Minor, Patch) */
            RDK_WINDOW_MANAGER_API_VERSION_NUMBER_MAJOR, RDK_WINDOW_MANAGER_API_VERSION_NUMBER_MINOR, RDK_WINDOW_MANAGER_API_VERSION_NUMBER_PATCH,
            /* Preconditions */
            {},
            /* Terminations */
            {},
            /* Controls */
            {}
        );
    }

    namespace Plugin {

        SERVICE_REGISTRATION(RDKWindowManager, RDK_WINDOW_MANAGER_API_VERSION_NUMBER_MAJOR, RDK_WINDOW_MANAGER_API_VERSION_NUMBER_MINOR, RDK_WINDOW_MANAGER_API_VERSION_NUMBER_PATCH);

        RDKWindowManager* RDKWindowManager::_instance = nullptr;

        RDKWindowManager::RDKWindowManager()
                : PluginHost::JSONRPC()
                , mCurrentService(nullptr)
                , mConnectionId(0)
                , mRDKWindowManagerImpl(nullptr)
                , mRDKWindowManagerNotification(this)
        {
            SYSLOG(Logging::Startup, (_T("RDKWindowManager Constructor")));
            RDKWindowManager::_instance = this;
        }

        RDKWindowManager::~RDKWindowManager()
        {
            SYSLOG(Logging::Startup, (_T("RDKWindowManager Destructor")));
            RDKWindowManager::_instance = nullptr;
        }

        const string RDKWindowManager::Initialize(PluginHost::IShell* service )
        {
            uint32_t result = Core::ERROR_GENERAL;
            string retStatus = "";

            ASSERT(nullptr != service);
            ASSERT(nullptr == mCurrentService);
            ASSERT(nullptr == mRDKWindowManagerImpl);
            ASSERT(0 == mConnectionId);

            SYSLOG(Logging::Startup, (_T("RDKWindowManager::Initialize: PID=%u"), getpid()));

            mCurrentService = service;

            if (nullptr != mCurrentService)
            {
                mCurrentService->AddRef();
                mCurrentService->Register(&mRDKWindowManagerNotification);

                mRDKWindowManagerImpl = mCurrentService->Root<Exchange::IRDKWindowManager>(mConnectionId, 5000, _T(PLUGIN_RDK_WINDOW_MANAGER_IMPLEMENTATION_NAME));

                if (nullptr != mRDKWindowManagerImpl)
                {
                    if (Core::ERROR_NONE != (result = mRDKWindowManagerImpl->Initialize(mCurrentService)))
                    {
                        SYSLOG(Logging::Startup, (_T("RDKWindowManager::Initialize: Failed to initialise %s"), PLUGIN_RDK_WINDOW_MANAGER_IMPLEMENTATION_NAME));
                        retStatus = _T("RDKWindowManager plugin could not be initialised");
                    }
                    else
                    {
                        /* Register for notifications */
                        mRDKWindowManagerImpl->Register(&mRDKWindowManagerNotification);
                        /* Invoking Plugin API register to wpeframework */
                        Exchange::JRDKWindowManager::Register(*this, mRDKWindowManagerImpl);
                    }
                }
                else
                {
                    SYSLOG(Logging::Startup, (_T("RDKWindowManager::Initialize: RDKWindowManagerImpl[%s] object creation failed"), PLUGIN_RDK_WINDOW_MANAGER_IMPLEMENTATION_NAME));
                    retStatus = _T("RDKWindowManager plugin could not be initialised");
                }
            }
            else
            {
                SYSLOG(Logging::Startup, (_T("RDKWindowManager::Initialize: service is not valid")));
                retStatus = _T("RDKWindowManager plugin could not be initialised");
            }

            if (0 != retStatus.length())
            {
               Deinitialize(service);
            }

            return retStatus;
        }

        void RDKWindowManager::Deinitialize(PluginHost::IShell* service)
        {
            SYSLOG(Logging::Startup, (_T("RDKWindowManager::Deinitialize: PID=%u"), getpid()));

            ASSERT(mCurrentService == service);
            ASSERT(0 == mConnectionId);

            if (nullptr != mRDKWindowManagerImpl)
            {
                mRDKWindowManagerImpl->Unregister(&mRDKWindowManagerNotification);
                Exchange::JRDKWindowManager::Unregister(*this);

                if (nullptr != mCurrentService)
                {
                    mRDKWindowManagerImpl->Deinitialize(mCurrentService);
                }

                /* Stop processing: */
                RPC::IRemoteConnection* connection = service->RemoteConnection(mConnectionId);
                VARIABLE_IS_NOT_USED uint32_t result = mRDKWindowManagerImpl->Release();
    
                mRDKWindowManagerImpl = nullptr;
    
                /* It should have been the last reference we are releasing,
                 * so it should endup in a DESTRUCTION_SUCCEEDED, if not we
                 * are leaking... */
                ASSERT(result == Core::ERROR_DESTRUCTION_SUCCEEDED);
    
                /* If this was running in a (container) process... */
                if (nullptr != connection)
                {
                   /* Lets trigger the cleanup sequence for
                    * out-of-process code. Which will guard
                    * that unwilling processes, get shot if
                    * not stopped friendly :-) */
                   connection->Terminate();
                   connection->Release();
                }
            }
    
            if (nullptr != mCurrentService)
            {
                /* Make sure the Activated and Deactivated are no longer called before we start cleaning up.. */
                mCurrentService->Unregister(&mRDKWindowManagerNotification);
                mCurrentService->Release();
                mCurrentService = nullptr;
            }

            mConnectionId = 0;
            SYSLOG(Logging::Shutdown, (string(_T("RDKWindowManager de-initialised"))));
        }

        string RDKWindowManager::Information() const
        {
            return(string("{\"service\": \"") + SERVICE_NAME + string("\"}"));
        }

        void RDKWindowManager::Deactivated(RPC::IRemoteConnection* connection)
        {
            if (connection->Id() == mConnectionId) {
                ASSERT(nullptr != mCurrentService);
                LOGINFO("RDKWindowManager::Deactivated");
                Core::IWorkerPool::Instance().Submit(PluginHost::IShell::Job::Create(mCurrentService, PluginHost::IShell::DEACTIVATED, PluginHost::IShell::FAILURE));
            }
        }
    } /* namespace Plugin */
} /* namespace WPEFramework */
