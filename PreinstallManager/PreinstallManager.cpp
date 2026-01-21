/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2025 RDK Management
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

#include "PreinstallManager.h"

const string WPEFramework::Plugin::PreinstallManager::SERVICE_NAME = "org.rdk.PreinstallManager";

namespace WPEFramework
{

    namespace
    {

        static Plugin::Metadata<Plugin::PreinstallManager> metadata(
            // Version (Major, Minor, Patch)
            PREINSTALL_MANAGER_API_VERSION_NUMBER_MAJOR, PREINSTALL_MANAGER_API_VERSION_NUMBER_MINOR, PREINSTALL_MANAGER_API_VERSION_NUMBER_PATCH,
            // Preconditions
            {},
            // Terminations
            {},
            // Controls
            {});
    }

    namespace Plugin
    {

        /*
         *Register PreinstallManager module as wpeframework plugin
         **/
        SERVICE_REGISTRATION(PreinstallManager, PREINSTALL_MANAGER_API_VERSION_NUMBER_MAJOR, PREINSTALL_MANAGER_API_VERSION_NUMBER_MINOR, PREINSTALL_MANAGER_API_VERSION_NUMBER_PATCH);

        PreinstallManager *PreinstallManager::_instance = nullptr;

        PreinstallManager::PreinstallManager() : mCurrentService(nullptr),
                                                 mConnectionId(0),
                                                 mPreinstallManagerImpl(nullptr),
                                                 mPreinstallManagerNotification(this),
                                                 mPreinstallManagerConfigure(nullptr)
        {
            SYSLOG(Logging::Startup, (_T("PreinstallManager Constructor")));
            if (nullptr == PreinstallManager::_instance)
            {
                PreinstallManager::_instance = this;
            }
        }

        PreinstallManager::~PreinstallManager()
        {
            SYSLOG(Logging::Shutdown, (string(_T("PreinstallManager Destructor"))));
            PreinstallManager::_instance = nullptr;
        }

        const string PreinstallManager::Initialize(PluginHost::IShell *service)
        {
            string message = "";

            ASSERT(nullptr != service);
            ASSERT(nullptr == mCurrentService);
            ASSERT(nullptr == mPreinstallManagerImpl);
            ASSERT(0 == mConnectionId);

            SYSLOG(Logging::Startup, (_T("PreinstallManager::Initialize: PID=%u"), getpid()));

            mCurrentService = service;
            if (nullptr != mCurrentService)
            {
                mCurrentService->AddRef();
                mCurrentService->Register(&mPreinstallManagerNotification);

                if (nullptr == (mPreinstallManagerImpl = mCurrentService->Root<Exchange::IPreinstallManager>(mConnectionId, 5000, _T("PreinstallManagerImplementation"))))
                {
                    SYSLOG(Logging::Startup, (_T("PreinstallManager::Initialize: object creation failed")));
                    message = _T("PreinstallManager plugin could not be initialised");
                }
                else if (nullptr == (mPreinstallManagerConfigure = mPreinstallManagerImpl->QueryInterface<Exchange::IConfiguration>()))
                {
                    SYSLOG(Logging::Startup, (_T("PreinstallManager::Initialize: did not provide a configuration interface")));
                    message = _T("PreinstallManager implementation did not provide a configuration interface");
                }
                else
                {
                    // Register for notifications
                    mPreinstallManagerImpl->Register(&mPreinstallManagerNotification);
                    // Invoking Plugin API register to wpeframework
                    Exchange::JPreinstallManager::Register(*this, mPreinstallManagerImpl);

                    if (Core::ERROR_NONE != mPreinstallManagerConfigure->Configure(mCurrentService))
                    {
                        SYSLOG(Logging::Startup, (_T("PreinstallManager::Initialize: could not be configured")));
                        message = _T("PreinstallManager could not be configured");
                    }
                }
            }
            else
            {
                SYSLOG(Logging::Startup, (_T("PreinstallManager::Initialize: service is not valid")));
                message = _T("PreinstallManager plugin could not be initialised");
            }

            if (0 != message.length())
            {
                Deinitialize(service);
            }

            return message;
        }

        void PreinstallManager::Deinitialize(PluginHost::IShell *service)
        {
            ASSERT(mCurrentService == service);

            SYSLOG(Logging::Shutdown, (string(_T("PreinstallManager::Deinitialize"))));

            // Make sure the Activated and Deactivated are no longer called before we start cleaning up..
            mCurrentService->Unregister(&mPreinstallManagerNotification);

            if (nullptr != mPreinstallManagerImpl)
            {
                mPreinstallManagerImpl->Unregister(&mPreinstallManagerNotification);
                Exchange::JPreinstallManager::Unregister(*this);

                if (nullptr != mPreinstallManagerConfigure)
                {
                    mPreinstallManagerConfigure->Release();
                    mPreinstallManagerConfigure = nullptr;
                }

                // Stop processing:
                RPC::IRemoteConnection *connection = service->RemoteConnection(mConnectionId);
                VARIABLE_IS_NOT_USED uint32_t result = mPreinstallManagerImpl->Release();

                mPreinstallManagerImpl = nullptr;

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
            SYSLOG(Logging::Shutdown, (string(_T("PreinstallManager de-initialised"))));
        }

        string PreinstallManager::Information() const
        {
            // No additional info to report
            return (string());
        }

        void PreinstallManager::Deactivated(RPC::IRemoteConnection *connection)
        {
            if (connection->Id() == mConnectionId)
            {
                ASSERT(nullptr != mCurrentService);
                Core::IWorkerPool::Instance().Submit(PluginHost::IShell::Job::Create(mCurrentService, PluginHost::IShell::DEACTIVATED, PluginHost::IShell::FAILURE));
            }
        }
    } /* namespace Plugin */
} /* namespace WPEFramework */
