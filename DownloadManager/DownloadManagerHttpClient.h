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
#include <mutex>
#include <curl/curl.h>

#include "UtilsLogging.h"

class DownloadManagerHttpClient {
    public:
        enum Status {
            Success,
            HttpError,
            DiskError
        };

        DownloadManagerHttpClient();
        ~DownloadManagerHttpClient();

        Status downloadFile(const std::string & url, const std::string & fileName, uint32_t rateLimit = 0);

        void pause() {
            std::lock_guard<std::mutex> lock(mHttpClientMutex);
            curl_easy_pause(curl, CURLPAUSE_RECV | CURLPAUSE_SEND);
        }
        void resume() {
            std::lock_guard<std::mutex> lock(mHttpClientMutex);
            curl_easy_pause(curl, CURLPAUSE_CONT);
        }
        void cancel() {
            std::lock_guard<std::mutex> lock(mHttpClientMutex);
            bCancel = true;
        }
        void setRateLimit(uint32_t rateLimit) {
            LOGDBG("curl rateLimit set to %u", rateLimit);
            std::lock_guard<std::mutex> lock(mHttpClientMutex);
            (void) curl_easy_setopt(curl, CURLOPT_MAX_RECV_SPEED_LARGE, (curl_off_t)rateLimit);
        }
        uint8_t getProgress() {
            std::lock_guard<std::mutex> lock(mHttpClientMutex);
            return progress;
        }

        // SSL
        void setToken() {}
        void setCert() {}
        void setCertPath() {}

        long getStatusCode() {
           std::lock_guard<std::mutex> lock(mHttpClientMutex);
           return httpCode;
        }

    private:
        static size_t progressCb(void *ptr, double dltotal, double dlnow, double ultotal, double ulnow);
        static size_t write_data(void *ptr, size_t size, size_t nmemb, FILE *stream);

    private:
        std::mutex mHttpClientMutex;
        CURL    *curl;
        long    httpCode = 0;
        bool    bCancel = false;
        uint8_t progress = 0;
};

