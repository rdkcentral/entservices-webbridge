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

#pragma once

#include "Module.h"
#include <interfaces/Ids.h>
#include <interfaces/IPreinstallManager.h>
#include <interfaces/IAppPackageManager.h>
#include <interfaces/IConfiguration.h>
#include "tracing/Logging.h"
#include "UtilsLogging.h"
#include <com/com.h>
#include <core/core.h>
#include <plugins/plugins.h>
#include <mutex>
#include <map>

namespace WPEFramework
{
    namespace Plugin
    {
        // typedef Exchange::IPreinstallManager::PreinstallState PreinstallState;
        // typedef Exchange::IPreinstallManager::PreinstallFailReason PreinstallFailReason;
        // typedef Exchange::IPreinstallManager::AppInstallInfo AppInstallInfo;

        class PreinstallManagerImplementation : public Exchange::IPreinstallManager, public Exchange::IConfiguration
        {

        public:
            PreinstallManagerImplementation();
            ~PreinstallManagerImplementation() override;

            static PreinstallManagerImplementation *getInstance();
            PreinstallManagerImplementation(const PreinstallManagerImplementation &) = delete;
            PreinstallManagerImplementation &operator=(const PreinstallManagerImplementation &) = delete;

            BEGIN_INTERFACE_MAP(PreinstallManagerImplementation)
            INTERFACE_ENTRY(Exchange::IPreinstallManager)
            INTERFACE_ENTRY(Exchange::IConfiguration)
            END_INTERFACE_MAP

        public:
            typedef struct _PackageInfo
            {
                string fileLocator;
                string packageId;
                string version;
                WPEFramework::Exchange::RuntimeConfig configMetadata;
                string installStatus; // optional field to store install status for logging purpose
            } PackageInfo;

            enum EventNames {
            PREINSTALL_MANAGER_UNKNOWN = 0,
            PREINSTALL_MANAGER_APP_INSTALLATION_STATUS
            };

            typedef Exchange::IPackageInstaller::InstallState InstallState;
            typedef Exchange::IPackageInstaller::FailReason FailReason;

        private:
        class PackageManagerNotification : public Exchange::IPackageInstaller::INotification {

        public:
            PackageManagerNotification(PreinstallManagerImplementation& parent) : mParent(parent){}
            ~PackageManagerNotification(){}

        void OnAppInstallationStatus(const string& jsonresponse)
        {
            // LOGINFO("Received Installation Status event from PackageManager");
            mParent.handleOnAppInstallationStatus(jsonresponse);
        }

        BEGIN_INTERFACE_MAP(PackageManagerNotification)
        INTERFACE_ENTRY(Exchange::IPackageInstaller::INotification)
        END_INTERFACE_MAP

        private:
            PreinstallManagerImplementation& mParent;
        };

        private:
            class EXTERNAL Job : public Core::IDispatch
            {
            protected:
                Job(PreinstallManagerImplementation *preinstallManagerImplementation, EventNames event, JsonObject &params)
                    : mPreinstallManagerImplementation(preinstallManagerImplementation), _event(event), _params(params)
                {
                    if (mPreinstallManagerImplementation != nullptr)
                    {
                        mPreinstallManagerImplementation->AddRef();
                    }
                }

            public:
                Job() = delete;
                Job(const Job &) = delete;
                Job &operator=(const Job &) = delete;
                ~Job()
                {
                    if (mPreinstallManagerImplementation != nullptr)
                    {
                        mPreinstallManagerImplementation->Release();
                    }
                }

            public:
                static Core::ProxyType<Core::IDispatch> Create(PreinstallManagerImplementation *preinstallManagerImplementation, EventNames event, JsonObject params)
                {
#ifndef USE_THUNDER_R4
                    return (Core::proxy_cast<Core::IDispatch>(Core::ProxyType<Job>::Create(preinstallManagerImplementation, event, params)));
#else
                    return (Core::ProxyType<Core::IDispatch>(Core::ProxyType<Job>::Create(preinstallManagerImplementation, event, params)));
#endif
                }

                virtual void Dispatch()
                {
                    mPreinstallManagerImplementation->Dispatch(_event, _params);
                }

            private:
                PreinstallManagerImplementation *mPreinstallManagerImplementation;
                const EventNames _event;
                const JsonObject _params;
            };

        public:
            Core::hresult Register(Exchange::IPreinstallManager::INotification *notification) override;
            Core::hresult Unregister(Exchange::IPreinstallManager::INotification *notification) override;
            Core::hresult StartPreinstall(bool forceInstall) override;
            void handleOnAppInstallationStatus(const std::string &jsonresponse);

            // // IConfiguration methods
            uint32_t Configure(PluginHost::IShell *service) override;

        private: /* private methods */
            Core::hresult createPackageManagerObject();
            void releasePackageManagerObject();
            bool isValidSemVer(const std::string &version);
            bool isNewerVersion(const std::string &v1, const std::string &v2);
            bool readPreinstallDirectory(std::list<PackageInfo> &packages);
            string getFailReason(FailReason reason);

        private:
            mutable Core::CriticalSection mAdminLock;
            std::list<Exchange::IPreinstallManager::INotification*> mPreinstallManagerNotifications;
            PluginHost::IShell *mCurrentservice;
            Exchange::IPackageInstaller* mPackageManagerInstallerObject;
            Core::Sink<PackageManagerNotification> mPackageManagerNotification;
            void dispatchEvent(EventNames, const JsonObject &params);
            void Dispatch(EventNames event, const JsonObject params);
            friend class Job;

        public /*members*/:
            static PreinstallManagerImplementation *_instance;

        public: /* public methods */
            // void updateCurrentAction(const std::string &appId, CurrentAction action);
        };
    } /* namespace Plugin */
} /* namespace WPEFramework */
