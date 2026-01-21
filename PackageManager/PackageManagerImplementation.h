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

#include <string>
#include <vector>
#include <thread>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <fstream>
#include <cstdio>

#ifdef USE_LIBPACKAGE
#include <IPackageImpl.h>
#elif defined(UNIT_TEST)
#include "IPackageImplDummy.h"
#endif
#include <json/json.h>

#include "Module.h"
#include "UtilsLogging.h"
#include <interfaces/IAppPackageManager.h>
#include <interfaces/IStorageManager.h>

#include "HttpClient.h"

#ifdef ENABLE_AIMANAGERS_TELEMETRY_METRICS
#include <interfaces/ITelemetryMetrics.h>

#define TELEMETRY_MARKER_LAUNCH_TIME             "OverallLaunchTime_split"
#define TELEMETRY_MARKER_CLOSE_TIME              "AppCloseTime_split"
#define TELEMETRY_MARKER_INSTALL_TIME            "InstallTime_split"
#define TELEMETRY_MARKER_INSTALL_ERROR           "InstallError_split"
#define TELEMETRY_MARKER_UNINSTALL_TIME          "UninstallTime_split"
#define TELEMETRY_MARKER_UNINSTALL_ERROR         "UninstallError_split"
#endif /* ENABLE_AIMANAGERS_TELEMETRY_METRICS */

#define PACKAGE_MANAGER_MARKER_FILE              "/tmp/package_manager_ready"

namespace WPEFramework {
namespace Plugin {
    typedef Exchange::IPackageDownloader::Reason DownloadReason;
    typedef Exchange::IPackageInstaller::InstallState InstallState;
    typedef Exchange::IPackageInstaller::FailReason FailReason;

    class PackageManagerImplementation
    : public Exchange::IPackageDownloader
    , public Exchange::IPackageInstaller
    , public Exchange::IPackageHandler
    {
        private:
        class State {
            class BlockedInstallData {
                public:
                string version;
                packagemanager::NameValues keyValues;
                string fileLocator;
            };

            public:
            State() {}
            State(const packagemanager::ConfigMetaData &config) {
                PackageManagerImplementation::getRuntimeConfig(config, runtimeConfig);
            }
            InstallState installState = InstallState::UNINSTALLED;
            bool preInsalled = false;
            uint32_t mLockCount = 0;
            Exchange::RuntimeConfig runtimeConfig;
            string gatewayMetadataPath;
            string unpackedPath;
            FailReason failReason = Exchange::IPackageInstaller::FailReason::NONE;
            std::list<Exchange::IPackageHandler::AdditionalLock> additionalLocks;
            BlockedInstallData  blockedInstallData;

        };

        typedef std::pair<std::string, std::string> StateKey;
        typedef std::map<StateKey, State> StateMap;

        class Configuration : public Core::JSON::Container {
            public:
                Configuration()
                    : Core::JSON::Container()
                    , downloadDir()
                {
                    Add(_T("downloadDir"), &downloadDir); //
                }
                ~Configuration() = default;

                Configuration(Configuration&&) = delete;
                Configuration(const Configuration&) = delete;
                Configuration& operator=(Configuration&&) = delete;
                Configuration& operator=(const Configuration&) = delete;

            public:
                Core::JSON::String downloadDir;
        };

        class DownloadInfo {
            const unsigned MIN_RETRIES = 2;
            public:
                DownloadInfo(const string& url, const string& id, uint8_t retries, long limit)
                : id(id)
                , url(url)
                , priority(false)
                , retries(retries ? retries : MIN_RETRIES)
                , rateLimit(limit)
                , cancel(false)
                {
                }

                string GetId() { return id; }
                string GetUrl() { return url; }
                bool GetPriority() { return priority; }
                uint8_t GetRetries() { return retries; }
                long GetRateLimit() { return rateLimit; }
                string GetFileLocator() { return fileLocator; }
                void SetFileLocator(string &locator) { fileLocator = locator; }
                void Cancel() { cancel = true ;}
                bool Cancelled() { return cancel; }

            private:
                string id;
                string url;
                bool priority;
                uint8_t retries;
                long rateLimit;
                string fileLocator;
                bool cancel;
        };

        typedef std::shared_ptr<DownloadInfo> DownloadInfoPtr;
        typedef std::list<DownloadInfoPtr> DownloadQueue;

    public:
#ifdef ENABLE_AIMANAGERS_TELEMETRY_METRICS
        enum PackageFailureErrorCode{
            ERROR_NONE,
            ERROR_SIGNATURE_VERIFICATION_FAILURE,
            ERROR_PACKAGE_MISMATCH_FAILURE,
            ERROR_INVALID_METADATA_FAILURE,
            ERROR_PERSISTENCE_FAILURE,
            ERROR_VERSION_NOT_FOUND
        };
#endif /* ENABLE_AIMANAGERS_TELEMETRY_METRICS */

        PackageManagerImplementation();
        virtual ~PackageManagerImplementation();

        // IPackageDownloader methods
        Core::hresult Download(const string& url, const Exchange::IPackageDownloader::Options &options, Exchange::IPackageDownloader::DownloadId &downloadId) override;
        Core::hresult Pause(const string &downloadId) override;
        Core::hresult Resume(const string &downloadId) override;
        Core::hresult Cancel(const string &downloadId) override;
        Core::hresult Delete(const string &fileLocator) override;
        Core::hresult Progress(const string &downloadId, Exchange::IPackageDownloader::ProgressInfo &progress);
        Core::hresult GetStorageInformation(uint32_t &quotaKB, uint32_t &usedKB);
        Core::hresult RateLimit(const string &downloadId, const uint64_t &limit);

        Core::hresult Register(Exchange::IPackageDownloader::INotification* notification) override;
        Core::hresult Unregister(Exchange::IPackageDownloader::INotification* notification) override;

        Core::hresult Initialize(PluginHost::IShell* service) override;
        Core::hresult Deinitialize(PluginHost::IShell* service) override;

        // IPackageInstaller methods
        Core::hresult Install(const string &packageId, const string &version, IPackageInstaller::IKeyValueIterator* const& additionalMetadata, const string &fileLocator, Exchange::IPackageInstaller::FailReason &failReason) override;
        Core::hresult Uninstall(const string &packageId, string &errorReason ) override;
        Core::hresult ListPackages(Exchange::IPackageInstaller::IPackageIterator*& packages);
        Core::hresult Config(const string &packageId, const string &version, Exchange::RuntimeConfig& configMetadata) override;
        Core::hresult PackageState(const string &packageId, const string &version, Exchange::IPackageInstaller::InstallState &state) override;
        Core::hresult GetConfigForPackage(const string &fileLocator, string& id, string &version, Exchange::RuntimeConfig& config) override;

        Core::hresult Register(Exchange::IPackageInstaller::INotification *sink) override;
        Core::hresult Unregister(Exchange::IPackageInstaller::INotification *sink) override;

        // IPackageHandler methods
        Core::hresult Lock(const string &packageId, const string &version, const Exchange::IPackageHandler::LockReason &lockReason,
            uint32_t &lockId, string &unpackedPath, Exchange::RuntimeConfig& configMetadata,
            Exchange::IPackageHandler::ILockIterator*& appMetadata
        ) override;

        Core::hresult Unlock(const string &packageId, const string &version) override;
        Core::hresult GetLockedInfo(const string &packageId, const string &version, string &unpackedPath, Exchange::RuntimeConfig& configMetadata,
            string& gatewayMetadataPath, bool &locked) override;

        static void getRuntimeConfig(const packagemanager::ConfigMetaData &config, Exchange::RuntimeConfig &runtimeConfig);
        static void getRuntimeConfig(const Exchange::RuntimeConfig &config, Exchange::RuntimeConfig &runtimeConfig);

        BEGIN_INTERFACE_MAP(PackageManagerImplementation)
            INTERFACE_ENTRY(Exchange::IPackageDownloader)
            INTERFACE_ENTRY(Exchange::IPackageInstaller)
            INTERFACE_ENTRY(Exchange::IPackageHandler)
        END_INTERFACE_MAP

    private:
        inline string GetInstalledVersion(const string& id) {
            for (auto const& [key, state] : mState) {
                if ((id.compare(key.first) == 0) &&
                    (state.installState == InstallState::INSTALLED || state.installState == InstallState::INSTALLATION_BLOCKED || state.installState == InstallState::UNINSTALL_BLOCKED)) {
                    return key.second;
                }
            }
            return "";
        }

        inline string GetBlockedVersion(const string& id) {
            for (auto const& [key, state] : mState) {
                if ((id.compare(key.first) == 0) &&
                    (state.installState == InstallState::INSTALLATION_BLOCKED || state.installState == InstallState::UNINSTALL_BLOCKED)) {
                    return key.second;
                }
            }
            return "";
        }

        inline bool IsInstallBlocked(const string &packageId, const string &version, const packagemanager::NameValues &keyValues, const string &fileLocator);
        Core::hresult Install(const string &packageId, const string &version, const packagemanager::NameValues &keyValues, const string &fileLocator, State& state);

        void InitializeState();
        void downloader(int n);
        void NotifyDownloadStatus(const string& id, const string& locator, const DownloadReason status);
        void NotifyInstallStatus(const string& id, const string& version, const State &state);

        DownloadInfoPtr getNext();
        int nextRetryDuration(int n) {
            double nxt = n * (1 + sqrt(5)) / 2.0;
            return round(nxt);
        }

        string getDownloadReason(DownloadReason reason) {
            switch (reason) {
                case DownloadReason::DOWNLOAD_FAILURE: return "DOWNLOAD_FAILURE";
                case DownloadReason::DISK_PERSISTENCE_FAILURE: return "DISK_PERSISTENCE_FAILURE";
                default: return "NONE";
            }
        }

        string getInstallState(InstallState state) {
            switch (state) {
                case InstallState::INSTALLING : return "INSTALLING";
                case InstallState::INSTALLATION_BLOCKED : return "INSTALLATION_BLOCKED";
                case InstallState::INSTALL_FAILURE : return "INSTALL_FAILURE";
                case InstallState::INSTALLED : return "INSTALLED";
                case InstallState::UNINSTALLING : return "UNINSTALLING";
                case InstallState::UNINSTALL_FAILURE : return "UNINSTALL_FAILURE";
                case InstallState::UNINSTALLED : return "UNINSTALLED";
                case InstallState::UNINSTALL_BLOCKED : return "UNINSTALL_BLOCKED";
                default: return "Unknown";
            }
        }

        string getFailReason(FailReason reason) {
            switch (reason) {
                case FailReason::SIGNATURE_VERIFICATION_FAILURE : return "SIGNATURE_VERIFICATION_FAILURE";
                case FailReason::PACKAGE_MISMATCH_FAILURE : return "PACKAGE_MISMATCH_FAILURE";
                case FailReason::INVALID_METADATA_FAILURE : return "INVALID_METADATA_FAILURE";
                case FailReason::PERSISTENCE_FAILURE : return "PERSISTENCE_FAILURE";
                default: return "NONE";
            }
        }
    Core::hresult createStorageManagerObject();
    void releaseStorageManagerObject();

#ifdef ENABLE_AIMANAGERS_TELEMETRY_METRICS
    void recordAndPublishTelemetryData(const std::string& marker, const std::string& appId, time_t requestTime, PackageManagerImplementation::PackageFailureErrorCode errorCode);
    time_t getCurrentTimestamp();
#endif /* ENABLE_AIMANAGERS_TELEMETRY_METRICS */


    private:
        mutable Core::CriticalSection mAdminLock;
        std::list<Exchange::IPackageDownloader::INotification*> mDownloaderNotifications;
        std::list<Exchange::IPackageInstaller::INotification*> mInstallNotifications;
        std::unique_ptr<HttpClient> mHttpClient;

        mutable std::mutex mMutex;
        std::condition_variable cv;
        std::unique_ptr<std::thread> mDownloadThreadPtr;
        bool done = false;
        DownloadInfoPtr mInprogressDownload;

        uint32_t mNextDownloadId;
        DownloadQueue  mDownloadQueue;
        std::recursive_mutex mtxState;
        StateMap  mState;
        bool cacheInitialized = false;

        std::string downloadDir = "/opt/CDL/";
        string configStr;

        #ifdef USE_LIBPACKAGE
        std::shared_ptr<packagemanager::IPackageImpl> packageImpl;
        #elif defined(UNIT_TEST)
        std::shared_ptr<packagemanager::IPackageImplDummy> packageImpl;
        #endif
        PluginHost::IShell* mCurrentservice;
        Exchange::IStorageManager* mStorageManagerObject;
#ifdef ENABLE_AIMANAGERS_TELEMETRY_METRICS
        Exchange::ITelemetryMetrics* mTelemetryMetricsObject;
#endif /* ENABLE_AIMANAGERS_TELEMETRY_METRICS */
    };
} // namespace Plugin
} // namespace WPEFramework
