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

#include <iostream>
#include <math.h>

#include "Module.h"

#include "HttpClient.h"

HttpClient::HttpClient() {
    //curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();
    if (curl) {
        LOGDBG("curl initialized");
    } else {
        LOGERR("curl initialize failed");
    }
 }

HttpClient::~HttpClient() {
    if (curl) {
        curl_easy_cleanup(curl);
    }
    //curl_global_cleanup();
}

HttpClient::Status
HttpClient::downloadFile(const std::string & url, const std::string & fileName, uint32_t rateLimit) {
    Status status = Status::Success;
    CURLcode cc = CURLE_OK;
    FILE *fp;
    bCancel = false;
    httpCode = 0;

    if (curl) {
        (void) curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        setRateLimit(rateLimit);

        fp = fopen(fileName.c_str(), "wb");
        if (fp != NULL) {
            (void) curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
            (void) curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);

            (void) curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
            (void) curl_easy_setopt(curl, CURLOPT_PROGRESSDATA, this);
            (void) curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION, progressCb);

            cc = curl_easy_perform(curl);
            curl_easy_getinfo (curl, CURLINFO_RESPONSE_CODE, &httpCode);
            if (cc == CURLE_OK) {
                LOGDBG("Download %s Success", fileName.c_str());
            } else {
                LOGERR("Download %s Failed error: %s curl code: %d http code: %ld", fileName.c_str(), curl_easy_strerror(cc), cc, httpCode);
                if ( cc == CURLE_WRITE_ERROR ) {
                    status = Status::DiskError;
                } else {
                    status = Status::HttpError;
                }
            }
            fclose(fp);
        } else {
            LOGERR("Failed to open %s", fileName.c_str());
            status = Status::DiskError;
        }
    }

    return status;
}

size_t HttpClient::progressCb(void *ptr, double dltotal, double dlnow, double ultotal, double ulnow)
{
    HttpClient *pHttpClient = static_cast<HttpClient *>(ptr);

    int percent = (int)(dlnow * 100 / dltotal);
    pHttpClient->progress = percent;
    // LOGTRACE("%d completed", percent);

    return pHttpClient->bCancel;
}

size_t HttpClient::write_data(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    // LOGTRACE("size=%ld nmemb=%ld", size, nmemb);
    return fwrite(ptr, size, nmemb, stream);
}

