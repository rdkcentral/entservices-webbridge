/**
* If not stated otherwise in this file or this component's LICENSE
* file the following copyright and licenses apply:
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
**/

#include "DownloadManager.h"

namespace WPEFramework {

namespace Plugin
{

    namespace {
        static Metadata<DownloadManager> metadata(
            // Version
            1, 0, 0,
            // Preconditions
            {},
            // Terminations
            {},
            // Controls
            {}
        );
    }

    DownloadManager::DownloadManager()
        : mConnectionId(0)
        , mService(nullptr)
        , mDownloadManagerImpl(nullptr)
        , mNotificationSink(*this)
    {
    }

    const string DownloadManager::Initialize(PluginHost::IShell * service)
    {
        string message;

        ASSERT(service != nullptr);
        ASSERT(mService == nullptr);
        ASSERT(mConnectionId == 0);
        ASSERT(mDownloadManagerImpl == nullptr);
        mService = service;
        mService->AddRef();

        LOGINFO();
        // Register the Process::Notification stuff. The Remote process might die before we get a
        // change to "register" the sink for these events !!! So do it ahead of instantiation.
        mService->Register(&mNotificationSink);

        mDownloadManagerImpl = service->Root<Exchange::IDownloadManager>(mConnectionId, RPC::CommunicationTimeOut, _T("DownloadManagerImplementation"));
        if (mDownloadManagerImpl != nullptr)
        {
            mDownloadManagerImpl->Initialize(service);

            mDownloadManagerImpl->Register(&mNotificationSink);
            Exchange::JDownloadManager::Register(*this, mDownloadManagerImpl);
        }
        else
        {
            message = _T("DownloadManager could not be instantiated.");
        }

        // On success return empty, to indicate there is no error text.
        return (message);
    }

    void DownloadManager::Deinitialize(PluginHost::IShell* service)
    {
        LOGINFO();
        if (mService != nullptr)
        {
            ASSERT(mService == service);
            mService->Unregister(&mNotificationSink);
            if (mDownloadManagerImpl != nullptr)
            {
                mDownloadManagerImpl->Unregister(&mNotificationSink);
                Exchange::JDownloadManager::Unregister(*this);

                if (nullptr != mService)
                {
                    mDownloadManagerImpl->Deinitialize(mService);
                }

                RPC::IRemoteConnection* connection(mService->RemoteConnection(mConnectionId));
                if (mDownloadManagerImpl->Release() != Core::ERROR_DESTRUCTION_SUCCEEDED)
                {
                    LOGERR("DownloadManager Plugin is not properly destructed. %d", mConnectionId);
                }
                mDownloadManagerImpl = nullptr;

                // The connection can disappear in the meantime...
                if (connection != nullptr)
                {
                    // But if it did not dissapear in the meantime, forcefully terminate it. Shoot to kill :-)
                    connection->Terminate();
                    connection->Release();
                }
            }

            mService->Release();
            mService = nullptr;
            mConnectionId = 0;
            SYSLOG(Logging::Shutdown, (string(_T("DownloadManager de-initialised"))));
        }
    }

    string DownloadManager::Information() const
    {
        // No additional info to report.
        return (string());
    }


    void DownloadManager::Deactivated(RPC::IRemoteConnection* connection)
    {
        LOGINFO();
        // This can potentially be called on a socket thread, so the deactivation (wich in turn kills this object) must be done
        // on a seperate thread. Also make sure this call-stack can be unwound before we are totally destructed.
        if (mConnectionId == connection->Id())
        {
            ASSERT(mService != nullptr);
            Core::IWorkerPool::Instance().Submit(PluginHost::IShell::Job::Create(mService, PluginHost::IShell::DEACTIVATED, PluginHost::IShell::FAILURE));
        }
    }

} // namespace Plugin
} // namespace WPEFramework

