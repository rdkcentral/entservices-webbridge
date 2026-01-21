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

#include "PackageManager.h"
#include <interfaces/IConfiguration.h>

namespace WPEFramework {

namespace Plugin
{

    namespace {
        static Metadata<PackageManager> metadata(
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

    PackageManager::PackageManager()
        : mConnectionId(0)
        , mService(nullptr)
        , mPackageDownloader(nullptr)
        , mPackageInstaller(nullptr)
        , mPackageHandler(nullptr)
        , mNotificationSink(*this)
    {
    }

    const string PackageManager::Initialize(PluginHost::IShell * service)
    {
        string message;

        ASSERT(service != nullptr);
        ASSERT(mService == nullptr);
        ASSERT(mConnectionId == 0);
        ASSERT(mPackageDownloader == nullptr);
        ASSERT(mPackageInstaller == nullptr);
        ASSERT(mPackageHandler == nullptr);
        mService = service;
        mService->AddRef();

        LOGINFO();
        // Register the Process::Notification stuff. The Remote process might die before we get a
        // change to "register" the sink for these events !!! So do it ahead of instantiation.
        mService->Register(&mNotificationSink);

        mPackageDownloader = service->Root<Exchange::IPackageDownloader>(mConnectionId, RPC::CommunicationTimeOut, _T("PackageManagerImplementation"));
        if (mPackageDownloader != nullptr) {
            mPackageDownloader->Initialize(service);

            mPackageDownloader->Register(&mNotificationSink);
            Exchange::JPackageDownloader::Register(*this, mPackageDownloader);

             mPackageInstaller = mPackageDownloader->QueryInterface<Exchange::IPackageInstaller>();
            if (mPackageInstaller != nullptr) {
                mPackageInstaller->Register(&mNotificationSink);
                Exchange::JPackageInstaller::Register(*this, mPackageInstaller);
            } else {
                LOGERR("Failed to get instance of IPackageInstaller");
            }

            mPackageHandler = mPackageDownloader->QueryInterface<Exchange::IPackageHandler>();
            if (mPackageHandler != nullptr) {
                Exchange::JPackageHandler::Register(*this, mPackageHandler);
            } else {
                LOGERR("Failed to get instance of IPackageHandler");
            }
        }
        else {
            message = _T("PackageManager could not be instantiated.");
        }

        // On success return empty, to indicate there is no error text.
        return (message);
    }

    void PackageManager::Deinitialize(PluginHost::IShell* service VARIABLE_IS_NOT_USED)
    {
        LOGINFO();
        if (mService != nullptr) {
            ASSERT(mService == service);
            mService->Unregister(&mNotificationSink);

            if (mPackageHandler != nullptr) {
                Exchange::JPackageHandler::Unregister(*this);
            }

            if (mPackageInstaller != nullptr) {
                mPackageInstaller->Unregister(&mNotificationSink);
                Exchange::JPackageInstaller::Unregister(*this);
            }

            if (mPackageDownloader != nullptr) {
                mPackageDownloader->Unregister(&mNotificationSink);
                Exchange::JPackageDownloader::Unregister(*this);

                RPC::IRemoteConnection* connection(mService->RemoteConnection(mConnectionId));
                if (mPackageDownloader->Release() != Core::ERROR_DESTRUCTION_SUCCEEDED) {
                    LOGERR("PackageManager Plugin is not properly destructed. %d", mConnectionId);
                }
                mPackageDownloader = nullptr;

                // The connection can disappear in the meantime...
                if (connection != nullptr) {
                    // But if it did not dissapear in the meantime, forcefully terminate it. Shoot to kill :-)
                    connection->Terminate();
                    connection->Release();
                }
            }

            mService->Release();
            mService = nullptr;
            mConnectionId = 0;
            SYSLOG(Logging::Shutdown, (string(_T("PackageManager de-initialised"))));
        }
    }

    string PackageManager::Information() const
    {
        // No additional info to report.
        return (string());
    }


    void PackageManager::Deactivated(RPC::IRemoteConnection* connection)
    {
        // This can potentially be called on a socket thread, so the deactivation (wich in turn kills this object) must be done
        // on a seperate thread. Also make sure this call-stack can be unwound before we are totally destructed.
        if (mConnectionId == connection->Id()) {
            ASSERT(mService != nullptr);
            Core::IWorkerPool::Instance().Submit(PluginHost::IShell::Job::Create(mService, PluginHost::IShell::DEACTIVATED, PluginHost::IShell::FAILURE));
        }
    }

    void PackageManager::NotificationHandler::OnAppDownloadStatus(Exchange::IPackageDownloader::IPackageInfoIterator* const packageInfos)
    {
        JsonArray packageInfoArray;
        if (packageInfos != nullptr)
        {
            Exchange::IPackageDownloader::PackageInfo resultItem{};

            while (packageInfos->Next(resultItem) == true)
            {
                JsonObject packageInfoJson;
                packageInfoJson["downloadId"] = resultItem.downloadId;
                packageInfoJson["fileLocator"] = resultItem.fileLocator;
                packageInfoJson["failReason"] = (resultItem.reason == Exchange::IPackageDownloader::Reason::NONE) ? "NONE" :
                                            (resultItem.reason == Exchange::IPackageDownloader::Reason::DOWNLOAD_FAILURE) ? "DOWNLOAD_FAILURE" :
                                            (resultItem.reason == Exchange::IPackageDownloader::Reason::DISK_PERSISTENCE_FAILURE) ? "DISK_PERSISTENCE_FAILURE" : "UNKNOWN";
                packageInfoArray.Add(packageInfoJson);
            }

            Core::JSON::Container eventPayload;
            eventPayload.Add(_T("jsonresponse"), &packageInfoArray);
            //unable to remove escaped characters from fileLocator as ToString called in Notify() always adds escape characters.

            mParent.Notify(_T("onAppDownloadStatus"), eventPayload);
        }
    }
} // namespace Plugin
} // namespace WPEFramework
