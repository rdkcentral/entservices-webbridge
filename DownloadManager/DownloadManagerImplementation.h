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

#pragma once

#include <string>
#include <vector>
#include <thread>
#include <memory>
#include <mutex>
#include <condition_variable>

#include <json/json.h>

#include "Module.h"
#include "UtilsLogging.h"
#include <interfaces/IDownloadManager.h>

#include "DownloadManagerHttpClient.h"

#define DOWNLOAD_REASON_NONE    (0xFF)

namespace WPEFramework {
namespace Plugin {
    typedef Exchange::IDownloadManager::FailReason DownloadReason;

    class DownloadManagerImplementation
        : public Exchange::IDownloadManager
    {
        class Configuration : public Core::JSON::Container {
            public:
                Configuration()
                    : Core::JSON::Container()
                    , downloadDir()
                    , downloadId()
                {
                    Add(_T("downloadDir"), &downloadDir); //
                    Add(_T("downloadId"), &downloadId); //
                }
                ~Configuration() = default;

                Configuration(Configuration&&) = delete;
                Configuration(const Configuration&) = delete;
                Configuration& operator=(Configuration&&) = delete;
                Configuration& operator=(const Configuration&) = delete;

            public:
                Core::JSON::String downloadDir;
                Core::JSON::DecUInt32 downloadId;
        };

        class DownloadInfo {
            const unsigned MIN_RETRIES = 2;
            public:
                DownloadInfo(const string& url, const string& id, bool priority, uint8_t retries, long limit)
                : id(id)
                , url(url)
                , priority(priority)
                , retries(retries ? retries : MIN_RETRIES)
                , rateLimit(limit)
                , isCancelled(false)
                {
                }

                string  getId() { return id; }
                string  getUrl() { return url; }
                bool    getPriority() { return priority; }
                uint8_t getRetries() { return retries; }
                void    setRateLimit(uint32_t limit) { rateLimit = limit; }
                uint32_t getRateLimit() { return rateLimit; }
                string  getFileLocator() { return fileLocator; }
                void    setFileLocator(string &locator) { fileLocator = locator; }
                void    cancel() { isCancelled = true; }
                bool    cancelled() { return isCancelled; }

            private:
                string  id;
                string  url;
                bool    priority;
                uint8_t retries;
                uint32_t rateLimit;
                string  fileLocator;
                bool    isCancelled;
        };

        typedef std::shared_ptr<DownloadInfo> DownloadInfoPtr;
        typedef std::queue<DownloadInfoPtr> DownloadQueue;

    public:
        DownloadManagerImplementation();
        virtual ~DownloadManagerImplementation();

        // IDownloadManager methods
        Core::hresult Download(const string &url, const Exchange::IDownloadManager::Options &options, string &downloadId) override;
        Core::hresult Pause(const string &downloadId) override;
        Core::hresult Resume(const string &downloadId) override;
        Core::hresult Cancel(const string &downloadId) override;
        Core::hresult Delete(const string &fileLocator) override;
        Core::hresult Progress(const string &downloadId, uint8_t &percent);
        Core::hresult GetStorageDetails(uint32_t &quotaKB, uint32_t &usedKB);
        Core::hresult RateLimit(const string &downloadId, const uint32_t &limit);

        Core::hresult Register(Exchange::IDownloadManager::INotification* notification) override;
        Core::hresult Unregister(Exchange::IDownloadManager::INotification* notification) override;

        Core::hresult Initialize(PluginHost::IShell* service) override;
        Core::hresult Deinitialize(PluginHost::IShell* service) override;

        BEGIN_INTERFACE_MAP(DownloadManagerImplementation)
            INTERFACE_ENTRY(Exchange::IDownloadManager)
        END_INTERFACE_MAP

    private:

        void downloaderRoutine(int waitTime);
        void notifyDownloadStatus(const string& id, const string& locator, const DownloadReason status);

        DownloadInfoPtr pickDownloadJob(void);
        int nextRetryDuration(int n) {
            const double goldenRatio = (1 + std::sqrt(5)) / 2.0;
            double next = n * goldenRatio;
            return static_cast<int>(std::round(next));
        }

        string getDownloadReason(DownloadReason reason) {
            switch (reason)
            {
                case DownloadReason::DISK_PERSISTENCE_FAILURE:
                    return "DISK_PERSISTENCE_FAILURE";

                case DownloadReason::DOWNLOAD_FAILURE:
                    return "DOWNLOAD_FAILURE";

                 default:
                    return "";
            }
        }

    private:
        mutable Core::CriticalSection mAdminLock;
        std::list<Exchange::IDownloadManager::INotification*> mDownloadManagerNotification;
        std::unique_ptr<DownloadManagerHttpClient> mHttpClient;

        mutable std::mutex mQueueMutex;
        std::condition_variable mDownloadThreadCV;
        std::unique_ptr<std::thread> mDownloadThreadPtr;
        bool            mDownloaderRunFlag;
        DownloadInfoPtr mCurrentDownload;

        uint32_t        mDownloadId;
        DownloadQueue   mPriorityDownloadQueue;
        DownloadQueue   mRegularDownloadQueue;
        std::string     mDownloadPath;

        PluginHost::IShell* mCurrentservice;
    };
} // namespace Plugin
} // namespace WPEFramework

