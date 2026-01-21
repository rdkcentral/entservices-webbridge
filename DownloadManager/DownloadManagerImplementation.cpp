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

#include <chrono>

#include "DownloadManagerImplementation.h"

#define DOWNLOADER_DEFAULT_PATH_LOCATION    "/opt/CDL/"
#define DOWNLOADER_DOWNLOAD_ID_START        (2000)

namespace WPEFramework {
namespace Plugin {

    SERVICE_REGISTRATION(DownloadManagerImplementation, 1, 0);

    DownloadManagerImplementation::DownloadManagerImplementation()
        : mDownloadManagerNotification()
        , mDownloaderRunFlag(true)
        , mDownloadId(DOWNLOADER_DOWNLOAD_ID_START)
        , mDownloadPath(DOWNLOADER_DEFAULT_PATH_LOCATION)
        , mCurrentservice(nullptr)
    {
        LOGINFO("DM: ctor DownloadManagerImplementation: %p", this);
        mHttpClient = std::unique_ptr<DownloadManagerHttpClient>(new DownloadManagerHttpClient);
    }

    DownloadManagerImplementation::~DownloadManagerImplementation()
    {
        LOGINFO("DM: dtor DownloadManagerImplementation: %p", this);

        std::list<Exchange::IDownloadManager::INotification*>::iterator itDownloader(mDownloadManagerNotification.begin());
        {
            while (itDownloader != mDownloadManagerNotification.end())
            {
                (*itDownloader)->Release();
                itDownloader++;
            }
        }
        mDownloadManagerNotification.clear();
    }

    Core::hresult DownloadManagerImplementation::Register(Exchange::IDownloadManager::INotification* notification)
    {
        LOGINFO("entry");
        ASSERT(notification != nullptr);

        mAdminLock.Lock();
        ASSERT(std::find(mDownloadManagerNotification.begin(), mDownloadManagerNotification.end(), notification) == mDownloadManagerNotification.end());
        if (std::find(mDownloadManagerNotification.begin(), mDownloadManagerNotification.end(), notification) == mDownloadManagerNotification.end())
        {
            mDownloadManagerNotification.push_back(notification);
            notification->AddRef();
        }

        mAdminLock.Unlock();
        LOGINFO("exit");

        return Core::ERROR_NONE;
    }

    Core::hresult DownloadManagerImplementation::Unregister(Exchange::IDownloadManager::INotification* notification)
    {
        LOGINFO();
        ASSERT(notification != nullptr);
        Core::hresult result = Core::ERROR_NONE;

        /* Remove the notification from the list */
        mAdminLock.Lock();
        auto item = std::find(mDownloadManagerNotification.begin(), mDownloadManagerNotification.end(), notification);
        if (item != mDownloadManagerNotification.end())
        {
            notification->Release();
            mDownloadManagerNotification.erase(item);
        }
        else
        {
            LOGERR("DM: Failed to unregister - notification not found!");
            result = Core::ERROR_GENERAL;
        }
        mAdminLock.Unlock();

        return result;
    }

    Core::hresult DownloadManagerImplementation::Initialize(PluginHost::IShell* service)
    {
        Core::hresult result = Core::ERROR_NONE;
        LOGINFO("entry");

        if (service != nullptr)
        {
            mCurrentservice = service;
            mCurrentservice->AddRef();

            LOGINFO("DM: ConfigLine=%s", service->ConfigLine().c_str());
            DownloadManagerImplementation::Configuration config;
            config.FromString(service->ConfigLine());

            if (config.downloadDir.IsSet() == true)
            {
                mDownloadPath = config.downloadDir;
            }
            if (config.downloadId.IsSet() == true)
            {
                mDownloadId = static_cast<uint32_t>(config.downloadId.Value());
            }

            int rc = mkdir(mDownloadPath.c_str(), 0777);
            if (rc != 0 && errno != EEXIST)
            {
                LOGERR("DM: Failed to create Download Path '%s' rc: %d errno=%d", mDownloadPath.c_str(), rc, errno);
                result = Core::ERROR_GENERAL;
            }
            else
            {
                LOGINFO("DM: Download path ready at '%s'", mDownloadPath.c_str());
                mDownloadThreadPtr = std::unique_ptr<std::thread>(new std::thread(&DownloadManagerImplementation::downloaderRoutine, this, 1));
            }
        }
        else
        {
            LOGERR("DM: Initialization failed - service is null!");
            result = Core::ERROR_GENERAL;
        }

        LOGINFO("exit");
        return result;
    }

    Core::hresult DownloadManagerImplementation::Deinitialize(PluginHost::IShell* service)
    {
        Core::hresult result = Core::ERROR_NONE;
        LOGINFO();

        /* Stop the downloader thread */
        mDownloaderRunFlag = false;
        mDownloadThreadCV.notify_one();
        if (mDownloadThreadPtr && mDownloadThreadPtr->joinable())
        {
            mDownloadThreadPtr->join();
            mDownloadThreadPtr.reset();
        }

        /* Clear download queues */
        {
            std::lock_guard<std::mutex> lock(mQueueMutex);
            /* Clear priority queue */
            while (!mPriorityDownloadQueue.empty())
            {
                mPriorityDownloadQueue.pop();
            }

            /* Clear regular queue */
            while (!mRegularDownloadQueue.empty())
            {
                mRegularDownloadQueue.pop();
            }
        }

        mCurrentservice->Release();
        mCurrentservice = nullptr;

        return result;
    }

    // IDownloadManager methods
    Core::hresult DownloadManagerImplementation::Download(const string& url,
        const Exchange::IDownloadManager::Options &options,
        string &downloadId)
    {
        Core::hresult result = Core::ERROR_GENERAL;

        mAdminLock.Lock();
        if (!mCurrentservice->SubSystems()->IsActive(PluginHost::ISubSystem::INTERNET))
        {
            LOGERR("DM: Download failed - no internet! url=%s priority=%d retries=%u rateLimit=%u",
                   url.c_str(), options.priority, options.retries, options.rateLimit);
            result = Core::ERROR_UNAVAILABLE;
        }
        else if (url.empty())
        {
            LOGERR("DM: Download failed - empty URL! priority=%d retries=%u rateLimit=%u",
                   options.priority, options.retries, options.rateLimit);
        }
        else
        {
            std::lock_guard<std::mutex> lock(mQueueMutex);
            std::string downloadIdStr = std::to_string(++mDownloadId);
            DownloadInfoPtr newDownload = std::make_shared<DownloadInfo>(url, downloadIdStr, options.priority, options.retries, options.rateLimit);
            if (newDownload != nullptr)
            {
                std::string filename = mDownloadPath + "package" + newDownload->getId();
                newDownload->setFileLocator(filename);

                /* Check the priority download request */
                if (options.priority)
                {
                    /* Added to priority queue download */
                    mPriorityDownloadQueue.push(newDownload);
                }
                else
                {
                    /* Added to regular queue download */
                    mRegularDownloadQueue.push(newDownload);
                }
                mDownloadThreadCV.notify_one();
                LOGINFO("DM: Download Request: id=%s url=%s priority=%d retries=%u rateLimit=%u",
                        newDownload->getId().c_str(), newDownload->getUrl().c_str(),
                        newDownload->getPriority(), newDownload->getRetries(),
                        newDownload->getRateLimit());

                /* Returning queued download id */
                downloadId = newDownload->getId();
                result = Core::ERROR_NONE;
            }
            else
            {
                LOGERR("DM: Failed to create new download - url=%s priority=%d retries=%u rateLimit=%u",
                       url.c_str(), options.priority, options.retries, options.rateLimit);
            }
        }
        mAdminLock.Unlock();

        return result;
    }

    Core::hresult DownloadManagerImplementation::Pause(const string &downloadId)
    {
        Core::hresult result = Core::ERROR_GENERAL;

        mAdminLock.Lock();
        if (!downloadId.empty() && (mCurrentDownload.get() != nullptr) && mHttpClient)
        {
            if (downloadId.compare(mCurrentDownload->getId()) == 0)
            {
                mHttpClient->pause();
                LOGINFO("DM: downloadId %s paused", downloadId.c_str());
                result = Core::ERROR_NONE;
            }
            else
            {
                LOGWARN("DM: Pause failed - ID mismatch! Requested=%s, Active=%s",
                        downloadId.c_str(), mCurrentDownload->getId().c_str());
                result = Core::ERROR_UNKNOWN_KEY;
            }
        }
        else
        {
            LOGERR("DM: Pause failed - downloadId=%s mCurrentDownload=%p mHttpClient=%p",
                   downloadId.c_str(), mCurrentDownload.get(), mHttpClient.get());
        }
        mAdminLock.Unlock();

        return result;
    }

    Core::hresult DownloadManagerImplementation::Resume(const string &downloadId)
    {
        Core::hresult result = Core::ERROR_GENERAL;

        mAdminLock.Lock();
        if (!downloadId.empty() && (mCurrentDownload.get() != nullptr) && mHttpClient)
        {
            if (downloadId.compare(mCurrentDownload->getId()) == 0)
            {
                mHttpClient->resume();
                LOGINFO("DM: downloadId %s resumed", downloadId.c_str());
                result = Core::ERROR_NONE;
            }
            else
            {
                LOGWARN("DM: Resume failed - ID mismatch! Requested=%s, Active=%s",
                        downloadId.c_str(), mCurrentDownload->getId().c_str());
                result = Core::ERROR_UNKNOWN_KEY;
            }
        }
        else
        {
            LOGERR("DM: Resume failed - downloadId=%s mCurrentDownload=%p mHttpClient=%p",
                   downloadId.c_str(), mCurrentDownload.get(), mHttpClient.get());
        }
        mAdminLock.Unlock();

        return result;
    }

    Core::hresult DownloadManagerImplementation::Cancel(const string &downloadId)
    {
        Core::hresult result = Core::ERROR_GENERAL;

        mAdminLock.Lock();
        if (!downloadId.empty() && (mCurrentDownload.get() != nullptr) && mHttpClient)
        {
            if (downloadId.compare(mCurrentDownload->getId()) == 0)
            {
                mCurrentDownload->cancel();
                mHttpClient->cancel();
                LOGINFO("DM: downloadId %s cancelled", downloadId.c_str());
                result = Core::ERROR_NONE;
            }
            else
            {
                LOGWARN("DM: Cancel failed - ID mismatch! Requested=%s, Active=%s",
                        downloadId.c_str(), mCurrentDownload->getId().c_str());
                result = Core::ERROR_UNKNOWN_KEY;
            }
        }
        else
        {
            LOGERR("DM: Cancel failed - downloadId=%s mCurrentDownload=%p mHttpClient=%p",
                   downloadId.c_str(), mCurrentDownload.get(), mHttpClient.get());
        }
        mAdminLock.Unlock();

        return result;
    }

    Core::hresult DownloadManagerImplementation::Delete(const string &fileLocator)
    {
        Core::hresult result = Core::ERROR_GENERAL;

        mAdminLock.Lock();
        if (!fileLocator.empty() && (mCurrentDownload.get() != nullptr) && \
            (fileLocator.compare(mCurrentDownload->getFileLocator()) == 0))
        {
            LOGWARN("DM: fileLocator %s download is in-progress", fileLocator.c_str());
        }
        else
        {
            if (remove(fileLocator.c_str()) == 0)
            {
                LOGINFO("DM: fileLocator %s Deleted", fileLocator.c_str());
                result = Core::ERROR_NONE;
            }
            else
            {
                LOGERR("DM: fileLocator '%s' delete failed", fileLocator.c_str());
            }
        }
        mAdminLock.Unlock();

        return result;
    }

    Core::hresult DownloadManagerImplementation::Progress(const string &downloadId, uint8_t &percent)
    {
        Core::hresult result = Core::ERROR_GENERAL;

        mAdminLock.Lock();
        if (!downloadId.empty() && (mCurrentDownload.get() != nullptr) && mHttpClient)
        {
            if (downloadId.compare(mCurrentDownload->getId()) == 0)
            {
                percent = mHttpClient->getProgress();
                LOGINFO("DM: Download Progress percent %u", percent);
                result = Core::ERROR_NONE;
            }
            else
            {
                result = Core::ERROR_UNKNOWN_KEY;
            }
        }
        else
        {
            LOGERR("DM: Progress failed - downloadId=%s mCurrentDownload=%p mHttpClient=%p",
                   downloadId.c_str(), mCurrentDownload.get(), mHttpClient.get());
        }
        mAdminLock.Unlock();

        return result;
    }

    Core::hresult DownloadManagerImplementation::GetStorageDetails(uint32_t &quotaKB, uint32_t &usedKB)
    {
        Core::hresult result = Core::ERROR_NONE;
        /* TODO: Stub - behaves the same as the existing package manager for now */
        return result;
    }

    Core::hresult DownloadManagerImplementation::RateLimit(const string &downloadId, const uint32_t &limit)
    {
        Core::hresult result = Core::ERROR_GENERAL;

        mAdminLock.Lock();
        if (!downloadId.empty() && (mCurrentDownload.get() != nullptr) && mHttpClient)
        {
            if (downloadId.compare(mCurrentDownload->getId()) == 0)
            {
                LOGINFO("DM: downloadId='%s' limit=%u", downloadId.c_str(), limit);
                mCurrentDownload->setRateLimit(limit);
                mHttpClient->setRateLimit(limit);
                result = Core::ERROR_NONE;
            }
            else
            {
                LOGWARN("DM: '%s' download is not active - unable to set rate limit=%u!", downloadId.c_str(), limit);
                result = Core::ERROR_UNKNOWN_KEY;
            }
        }
        else
        {
            LOGERR("DM: Set RateLimit Failed - mCurrentDownload=%p", mCurrentDownload.get());
        }
        mAdminLock.Unlock();

        return result;
    }

    void DownloadManagerImplementation::downloaderRoutine(int waitTime)
    {
        while (mDownloaderRunFlag)
        {
            DownloadInfoPtr downloadRequest = pickDownloadJob();
            while (downloadRequest == nullptr && mDownloaderRunFlag)
            {
                LOGDBG("DM: Waiting for download request...");
                std::unique_lock<std::mutex> lock(mQueueMutex);
                mDownloadThreadCV.wait(lock);
                lock.unlock();

                downloadRequest = pickDownloadJob();
            }

            if (false == mDownloaderRunFlag)
            {
                LOGINFO("DM: Downloader is shutting down - exiting thread!");
                break;
            }

            if (!downloadRequest)
            {
                LOGWARN("DM: No download request available - continuing loop!");
                continue;
            }

            DownloadManagerHttpClient::Status status = DownloadManagerHttpClient::Status::Success;
            int attemptCount = 0;

            LOGINFO("DM: Starting downloadId=%s url=%s file=%s retries=%d rateLimit=%u",
                    downloadRequest->getId().c_str(), downloadRequest->getUrl().c_str(),
                    downloadRequest->getFileLocator().c_str(), downloadRequest->getRetries(),
                    downloadRequest->getRateLimit());

            for (int i = 0; i < downloadRequest->getRetries(); ++i)
            {
                attemptCount = i + 1;
                if (i > 0)
                {
                    int retryWaitTime = nextRetryDuration(waitTime);
                    LOGDBG("DM: Retry %d/%d: Waiting %d seconds before retrying...",
                            attemptCount, downloadRequest->getRetries(), retryWaitTime);
                    std::this_thread::sleep_for(std::chrono::seconds(retryWaitTime));

                    if (downloadRequest->cancelled())
                    {
                        LOGINFO("DM: Download cancelled: id=%s !", downloadRequest->getId().c_str());
                        break;
                    }
                }

                LOGDBG("DM: Attempting download (%d/%d): id=%s url=%s file=%s rateLimit=%u",
                        attemptCount, downloadRequest->getRetries(),
                        downloadRequest->getId().c_str(),
                        downloadRequest->getUrl().c_str(),
                        downloadRequest->getFileLocator().c_str(),
                        downloadRequest->getRateLimit());

                auto begin = std::chrono::steady_clock::now();
                status = mHttpClient->downloadFile(downloadRequest->getUrl(),
                                        downloadRequest->getFileLocator(),
                                        downloadRequest->getRateLimit());
                auto end = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();
                long httpCode = mHttpClient->getStatusCode();
                if (status == DownloadManagerHttpClient::Status::Success)
                {
                    LOGINFO("DM: Download succeeded (took %lldms): id=%s url=%s file=%s retries=%d rateLimit=%u http_code=%ld",
                    elapsed, downloadRequest->getId().c_str(), downloadRequest->getUrl().c_str(),
                    downloadRequest->getFileLocator().c_str(), downloadRequest->getRetries(),
                    downloadRequest->getRateLimit(), httpCode);
                    break;
                }

                if (httpCode == 404)
                {
                    LOGERR("DM: Download file not found (404) - id=%s url=%s status=%d",
                            downloadRequest->getId().c_str(), downloadRequest->getUrl().c_str(), status);
                    status = DownloadManagerHttpClient::Status::HttpError;
                    break;
                }

                LOGDBG("DM: Attempt download (%d/%d): status=%d http_code=%ld elapsed=%lld ms",
                        attemptCount, downloadRequest->getRetries(), status, httpCode, elapsed);
            }

            if (status != DownloadManagerHttpClient::Status::Success)
            {
                LOGERR("DM: Download failed %d/%d: id=%s status=%d",
                       attemptCount, downloadRequest->getRetries(), downloadRequest->getId().c_str(), status);
            }

            DownloadReason reason = static_cast<DownloadReason>(DOWNLOAD_REASON_NONE);
            switch (status)
            {
                case DownloadManagerHttpClient::Status::DiskError:
                    reason = DownloadReason::DISK_PERSISTENCE_FAILURE;
                    LOGERR("DM: Download failed due to disk error: id=%s", downloadRequest->getId().c_str());
                    break;

                case DownloadManagerHttpClient::Status::HttpError:
                    reason = DownloadReason::DOWNLOAD_FAILURE;
                    LOGERR("DM: Download failed due to HTTP error: id=%s", downloadRequest->getId().c_str());
                    break;

                default:
                    break; /* Do nothing */
            }

            notifyDownloadStatus(downloadRequest->getId(), downloadRequest->getFileLocator(), reason);
            mCurrentDownload.reset();
        }
        LOGINFO("DM: Downloader thread exiting!");
    }

    void DownloadManagerImplementation::notifyDownloadStatus(const string& id, const string& locator, const DownloadReason reason)
    {
        JsonArray list = JsonArray();
        JsonObject obj;
        obj["downloadId"] = id;
        obj["fileLocator"] = locator;
        if (reason != (DownloadReason)DOWNLOAD_REASON_NONE)
        {
            obj["failReason"] = getDownloadReason(reason);
        }
        list.Add(obj);
        std::string jsonstr;
        if (!list.ToString(jsonstr))
        {
            LOGERR("DM: Failed to stringify JsonArray");
        }
        else
        {
            mAdminLock.Lock();
            LOGDBG("DM: OnAppDownloadStatus event: '%s'", jsonstr.c_str());
            for (auto notification: mDownloadManagerNotification)
            {
                notification->OnAppDownloadStatus(jsonstr);
            }
            mAdminLock.Unlock();
        }
    }

    DownloadManagerImplementation::DownloadInfoPtr DownloadManagerImplementation::pickDownloadJob(void)
    {
        std::lock_guard<std::mutex> lock(mQueueMutex);
        if ((!mPriorityDownloadQueue.empty() || !mRegularDownloadQueue.empty()) && mCurrentDownload == nullptr)
        {
            if (!mPriorityDownloadQueue.empty())
            {
                /* Priority queue download request */
                mCurrentDownload = mPriorityDownloadQueue.front();
                mPriorityDownloadQueue.pop();
                LOGINFO("DM: PriorityQ Job: DownloadId=%s url=%s file=%s rateLimit=%u",
                        mCurrentDownload->getId().c_str(), mCurrentDownload->getUrl().c_str(),
                        mCurrentDownload->getFileLocator().c_str(), mCurrentDownload->getRateLimit());
            }
            else
            {
                /* Regular queue download request */
                if (!mRegularDownloadQueue.empty())
                {
                    mCurrentDownload = mRegularDownloadQueue.front();
                    mRegularDownloadQueue.pop();
                    LOGINFO("DM: RegularQ Job: DownloadId=%s url=%s file=%s rateLimit=%u",
                            mCurrentDownload->getId().c_str(), mCurrentDownload->getUrl().c_str(),
                            mCurrentDownload->getFileLocator().c_str(), mCurrentDownload->getRateLimit());
                }
            }
        }
        return mCurrentDownload;
    }

} // namespace Plugin
} // namespace WPEFramework
