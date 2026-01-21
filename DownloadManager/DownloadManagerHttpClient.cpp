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

#include <iostream>
#include <math.h>

#include "Module.h"
#include "DownloadManagerHttpClient.h"

DownloadManagerHttpClient::DownloadManagerHttpClient()
    : curl(nullptr)
    , httpCode(0)
    , bCancel(false)
    , progress(0)
{
    //curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();
    if (curl)
    {
        LOGDBG("curl initialized");
    }
    else
    {
        LOGERR("curl initialize failed");
    }
 }

DownloadManagerHttpClient::~DownloadManagerHttpClient()
{
    std::lock_guard<std::mutex> lock(mHttpClientMutex);
    if (curl)
    {
        curl_easy_cleanup(curl);
    }
    //curl_global_cleanup();
}

DownloadManagerHttpClient::Status DownloadManagerHttpClient::downloadFile(const std::string & url, const std::string & fileName, uint32_t rateLimit)
{
    Status status = Status::Success;
    CURLcode cc;
    FILE *fp;

    std::unique_lock<std::mutex> lock(mHttpClientMutex);
    bCancel = false;
    progress = 0;
    httpCode = 0;

    if (curl)
    {
        (void) curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        LOGDBG("curl rateLimit set to %u", rateLimit);
        curl_easy_setopt(curl, CURLOPT_MAX_RECV_SPEED_LARGE, (curl_off_t)rateLimit);

        fp = fopen(fileName.c_str(), "wb");
        if (fp != NULL)
        {
            (void) curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
            (void) curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);

            (void) curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
            (void) curl_easy_setopt(curl, CURLOPT_PROGRESSDATA, this);
            (void) curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION, progressCb);

            /* Unlock before blocking call */
            lock.unlock();
            cc = curl_easy_perform(curl);
            /* Re-lock mutex after curl call */
            lock.lock();
            curl_easy_getinfo (curl, CURLINFO_RESPONSE_CODE, &httpCode);
            if (cc == CURLE_OK)
            {
                if (httpCode == 404) {
                    status = Status::HttpError;
                    LOGERR("Download %s Failed, code: %ld", fileName.c_str(),  httpCode);
                } else {
                    LOGDBG("Download %s Success", fileName.c_str());
            }
            }
            else
            {
                LOGERR("Download %s Failed error: %s code: %ld", fileName.c_str(), curl_easy_strerror(cc), httpCode);
                if ( cc == CURLE_WRITE_ERROR ) {
                    status = Status::DiskError;
                } else {
                    status = Status::HttpError;
                }
            }
            fclose(fp);
        }
        else
        {
            LOGERR("Failed to open %s", fileName.c_str());
            status = Status::DiskError;
        }
    }

    return status;
}

size_t DownloadManagerHttpClient::progressCb(void *ptr, double dltotal, double dlnow, double ultotal, double ulnow)
{
    DownloadManagerHttpClient *pHttpClient = static_cast<DownloadManagerHttpClient *>(ptr);
    std::lock_guard<std::mutex> lock(pHttpClient->mHttpClientMutex);
    if (dltotal > 0.0)
    {
        uint8_t percent = static_cast<uint8_t>((dlnow * 100) / dltotal);
        pHttpClient->progress = percent;
        //LOGDBG("%u%% completed dlnow=%f / dltotal=%f ulnow=%f / ultotal=%f", percent, dlnow, dltotal, ulnow, ultotal);
    }

    /* If cancel requested, return non-zero to abort curl_easy_perform */
    return pHttpClient->bCancel ? 1 : 0;
}

size_t DownloadManagerHttpClient::write_data(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    // LOGTRACE("size=%ld nmemb=%ld", size, nmemb);
    return fwrite(ptr, size, nmemb, stream);
}

