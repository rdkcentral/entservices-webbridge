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

#include "RuntimeManager.h"

const string WPEFramework::Plugin::RuntimeManager::SERVICE_NAME = "org.rdk.RuntimeManager";

namespace WPEFramework
{
    namespace {

        static Plugin::Metadata<Plugin::RuntimeManager> metadata(
            /* Version (Major, Minor, Patch) */
            RUNTIME_MANAGER_API_VERSION_NUMBER_MAJOR, RUNTIME_MANAGER_API_VERSION_NUMBER_MINOR, RUNTIME_MANAGER_API_VERSION_NUMBER_PATCH,
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

        SERVICE_REGISTRATION(RuntimeManager, RUNTIME_MANAGER_API_VERSION_NUMBER_MAJOR, RUNTIME_MANAGER_API_VERSION_NUMBER_MINOR, RUNTIME_MANAGER_API_VERSION_NUMBER_PATCH);

        RuntimeManager* RuntimeManager::sInstance = nullptr;

        RuntimeManager::RuntimeManager():
            mCurrentService(nullptr),
            mConnectionId(0),
            mRuntimeManagerImpl(nullptr),
            mRuntimeManagerConfigure(nullptr)
        {
            SYSLOG(Logging::Startup, (_T("RuntimeManager Constructor")));
            RuntimeManager::sInstance = this;
        }

        RuntimeManager::~RuntimeManager()
        {
            SYSLOG(Logging::Startup, (_T("RuntimeManager Destructor")));
            RuntimeManager::sInstance = nullptr;
        }

        const string RuntimeManager::Initialize(PluginHost::IShell* service )
        {
            string retMessage = "";

            ASSERT(nullptr != service);
            ASSERT(0 == mConnectionId);
            ASSERT(nullptr == mCurrentService);
            ASSERT(nullptr == mRuntimeManagerImpl);

            SYSLOG(Logging::Startup, (_T("RuntimeManager::Initialize: PID=%u"), getpid()));
            mCurrentService = service;

            if (nullptr != mCurrentService)
            {
                mCurrentService->AddRef();

                if (nullptr == (mRuntimeManagerImpl = mCurrentService->Root<Exchange::IRuntimeManager>(mConnectionId, 5000, _T("RuntimeManagerImplementation"))))
                {
                    SYSLOG(Logging::Startup, (_T("RuntimeManager::Initialize: object creation failed")));
                    retMessage = _T("RuntimeManager plugin could not be initialised");
                }
                else if (nullptr == (mRuntimeManagerConfigure = mRuntimeManagerImpl->QueryInterface<Exchange::IConfiguration>()))
                {
                    SYSLOG(Logging::Startup, (_T("RuntimeManager::Initialize: did not provide a configuration interface")));
                    retMessage = _T("RuntimeManager implementation did not provide a configuration interface");
                }
                else
                {
                    if (Core::ERROR_NONE != mRuntimeManagerConfigure->Configure(mCurrentService))
                    {
                        SYSLOG(Logging::Startup, (_T("RuntimeManager::Initialize: could not be configured")));
                        retMessage = _T("RuntimeManager could not be configured");
                    }
                }
            }
            else
            {
                SYSLOG(Logging::Startup, (_T("RuntimeManager::Initialize: service is not valid")));
                retMessage = _T("RuntimeManager plugin could not be initialised");
            }

            if (0 != retMessage.length())
            {
                Deinitialize(service);
            }

            return retMessage;
        }

        void RuntimeManager::Deinitialize(PluginHost::IShell* service)
        {
            ASSERT(mCurrentService == service);

            SYSLOG(Logging::Shutdown, (string(_T("RuntimeManager::Deinitialize"))));

            if (nullptr != mRuntimeManagerImpl)
            {
                if (nullptr != mRuntimeManagerConfigure)
                {
                    mRuntimeManagerConfigure->Release();
                    mRuntimeManagerConfigure = nullptr;
                }

                // Stop processing:
                RPC::IRemoteConnection* connection = service->RemoteConnection(mConnectionId);
                VARIABLE_IS_NOT_USED uint32_t result = mRuntimeManagerImpl->Release();

                mRuntimeManagerImpl = nullptr;

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
            SYSLOG(Logging::Shutdown, (string(_T("RuntimeManager de-initialised"))));
        }

        string RuntimeManager::Information() const
        {
            return(string("{\"service\": \"") + SERVICE_NAME + string("\"}"));
        }
    } /* namespace Plugin */
} /* namespace WPEFramework */
