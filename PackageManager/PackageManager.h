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

#pragma once

#include "Module.h"

#include <interfaces/json/JPackageDownloader.h>
#include <interfaces/json/JPackageInstaller.h>
#include <interfaces/json/JPackageHandler.h>
#include <interfaces/json/JsonData_PackageDownloader.h>
#include <interfaces/json/JsonData_PackageInstaller.h>

#include "UtilsLogging.h"

namespace WPEFramework {
namespace Plugin {

    class PackageManager : public PluginHost::IPlugin,
                           public PluginHost::JSONRPC {
    private:
        class NotificationHandler
            : public RPC::IRemoteConnection::INotification
            , public Exchange::IPackageDownloader::INotification
            , public Exchange::IPackageInstaller::INotification
            {
        public:
            NotificationHandler() = delete;
            NotificationHandler(const NotificationHandler&) = delete;
            NotificationHandler(NotificationHandler&&) = delete;
            NotificationHandler& operator=(const NotificationHandler&) = delete;
            NotificationHandler& operator=(NotificationHandler&&) = delete;

            NotificationHandler(PackageManager& parent)
                : mParent(parent)
            {
            }
            ~NotificationHandler() override = default;

        public:
            void Activated(RPC::IRemoteConnection*  /* connection */) override
            {
            }
            void Deactivated(RPC::IRemoteConnection* connection) override
            {
                mParent.Deactivated(connection);
            }
            void Terminated (RPC::IRemoteConnection* /* connection */) override
            {
            }

            void OnAppDownloadStatus(Exchange::IPackageDownloader::IPackageInfoIterator* const packageInfos) override;
            void OnAppInstallationStatus(const string& jsonresponse) override
            {
                LOGTRACE();
                Exchange::JPackageInstaller::Event::OnAppInstallationStatus(mParent, jsonresponse);
            }

            BEGIN_INTERFACE_MAP(NotificationHandler)
                INTERFACE_ENTRY(RPC::IRemoteConnection::INotification)
                INTERFACE_ENTRY(Exchange::IPackageDownloader::INotification)
            END_INTERFACE_MAP

        private:
            PackageManager& mParent;
        };
    public:
        PackageManager(const PackageManager&) = delete;
        PackageManager(PackageManager&&) = delete;
        PackageManager& operator=(const PackageManager&) = delete;
        PackageManager& operator=(PackageManager&) = delete;

        PackageManager();
        ~PackageManager() override = default;

        // Build QueryInterface implementation, specifying all possible interfaces to be returned.
        BEGIN_INTERFACE_MAP(PackageManager)
            INTERFACE_ENTRY(PluginHost::IPlugin)
            INTERFACE_ENTRY(PluginHost::IDispatcher)
            INTERFACE_AGGREGATE(Exchange::IPackageDownloader, mPackageDownloader)
            INTERFACE_AGGREGATE(Exchange::IPackageInstaller, mPackageInstaller)
            INTERFACE_AGGREGATE(Exchange::IPackageHandler, mPackageHandler)
        END_INTERFACE_MAP

    public:
        //   IPlugin methods
        // -------------------------------------------------------------------------------------------------------
        const string Initialize(PluginHost::IShell* service) override;
        void Deinitialize(PluginHost::IShell* service) override;
        string Information() const override;


    private:
        void Deactivated(RPC::IRemoteConnection* connection);

    private:
        uint32_t mConnectionId;
        PluginHost::IShell* mService;
        Exchange::IPackageDownloader* mPackageDownloader;
        Exchange::IPackageInstaller* mPackageInstaller;
        Exchange::IPackageHandler* mPackageHandler;

        Core::Sink<NotificationHandler> mNotificationSink;

    };

} // namespace Plugin
} // namespace WPEFramework
