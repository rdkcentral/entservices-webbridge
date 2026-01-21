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

#include "OCIContainer.h"

const string WPEFramework::Plugin::OCIContainer::SERVICE_NAME = "org.rdk.OCIContainer";

namespace WPEFramework
{
    namespace {

        static Plugin::Metadata<Plugin::OCIContainer> metadata(
            /* Version (Major, Minor, Patch) */
            OCICONTAINER_API_VERSION_NUMBER_MAJOR, OCICONTAINER_API_VERSION_NUMBER_MINOR, OCICONTAINER_API_VERSION_NUMBER_PATCH,
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

        SERVICE_REGISTRATION(OCIContainer, OCICONTAINER_API_VERSION_NUMBER_MAJOR, OCICONTAINER_API_VERSION_NUMBER_MINOR, OCICONTAINER_API_VERSION_NUMBER_PATCH);

        OCIContainer* OCIContainer::sInstance = nullptr;

        OCIContainer::OCIContainer(): _service(nullptr), mConnectionId(0), mOCIContainerImplementation(nullptr), mOCIContainerNotification(this)
        {
            SYSLOG(Logging::Startup, (_T("OCIContainer Constructor")));
            OCIContainer::sInstance = this;
        }

        OCIContainer::~OCIContainer()
        {
            SYSLOG(Logging::Startup, (_T("OCIContainer Destructor")));
            OCIContainer::sInstance = nullptr;
            mOCIContainerImplementation = nullptr;
        }

        const string OCIContainer::Initialize(PluginHost::IShell* service )
        {
            //uint32_t result = Core::ERROR_GENERAL;
            string retStatus = "";
            ASSERT(nullptr != service);
            ASSERT(0 == mConnectionId);
            ASSERT(nullptr == _service);
            SYSLOG(Logging::Startup, (_T("OCIContainer::Initialize: PID=%u"), getpid()));
            //{
            //    SYSLOG(Logging::Startup, (_T("OCIContainer::Initialize: service is not valid")));
            //    retStatus = _T("OCIContainer plugin could not be initialised");
            //}
            _service = service;
            _service->AddRef();
            _service->Register(&mOCIContainerNotification);
            mOCIContainerImplementation = _service->Root<Exchange::IOCIContainer>(mConnectionId, 5000, _T("OCIContainerImplementation"));
            if(nullptr != mOCIContainerImplementation)
            {
                // Register for notifications
                mOCIContainerImplementation->Register(&mOCIContainerNotification);
                // Invoking Plugin API register to wpeframework
                Exchange::JOCIContainer::Register(*this, mOCIContainerImplementation);
            }
            else
            {
                SYSLOG(Logging::Startup, (_T("OCIContainer::Initialize: Failed to initialise OCIContainer plugin")));
                retStatus = _T("OCIContainer plugin could not be initialised");
            }

            if (0 != retStatus.length())
            {
               Deinitialize(service);
            }

            return retStatus;
        }

        void OCIContainer::Deinitialize(PluginHost::IShell* service)
        {
            SYSLOG(Logging::Startup, (_T("OCIContainer::Deinitialize: PID=%u"), getpid()));
            ASSERT(_service == service);
            if (nullptr != mOCIContainerImplementation)
            {
                mOCIContainerImplementation->Unregister(&mOCIContainerNotification);
                Exchange::JOCIContainer::Unregister(*this);

                // Stop processing:
                RPC::IRemoteConnection* connection = service->RemoteConnection(mConnectionId);
                VARIABLE_IS_NOT_USED uint32_t result = mOCIContainerImplementation->Release();
                mOCIContainerImplementation = nullptr;
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
            if(nullptr != _service)
            {
                // Make sure the Activated and Deactivated are no longer called before we start cleaning up..
                _service->Unregister(&mOCIContainerNotification);
                _service->Release();
                _service = nullptr;
            }
            SYSLOG(Logging::Shutdown, (string(_T("OCIContainer de-initialised"))));
        }

        string OCIContainer::Information() const
        {
            return(string("{\"service\": \"") + SERVICE_NAME + string("\"}"));
        }

        void OCIContainer::Deactivated(RPC::IRemoteConnection* connection)
        {
            SYSLOG(Logging::Shutdown, (string(_T("OCIContainer Deactivated"))));
            if (connection->Id() == mConnectionId) {
                ASSERT(nullptr != _service);
                Core::IWorkerPool::Instance().Submit(PluginHost::IShell::Job::Create(_service, PluginHost::IShell::DEACTIVATED, PluginHost::IShell::FAILURE));
            }
        }

    } /* namespace Plugin */
} /* namespace WPEFramework */
