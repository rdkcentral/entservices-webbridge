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

#include <vector>
#include <string>

namespace WPEFramework
{
namespace Plugin
{

    typedef struct _AppStorageInfo
    {
        std::string path;
        int32_t userId;
        int32_t groupId;
        uint32_t size;
        uint32_t used;
    } AppStorageInfo;

    struct ApplicationConfiguration
    {
        std::string mAppId;

        std::string mAppInstanceId;
        
        std::vector<std::string> mArgs;
        
        // userId used by the container
        uint32_t mUserId;
        
        // groupId used by the container
        uint32_t mGroupId;
        
        std::vector<uint32_t> mPorts;
        
        // westeros socket path that the containerized app should use to communicate with compositor
        std::string mWesterosSocketPath;

        // TODO what to do with it?
        std::vector<std::string> mDebugSettings;

        // application storage info - storage path, uid, gid
        AppStorageInfo mAppStorageInfo;
    };
} /* namespace Plugin */
} /* namespace WPEFramework */
