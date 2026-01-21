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

#include "UtilsLogging.h"
#include <plugins/plugins.h>

namespace WPEFramework
{
    namespace Utils
    {

        std::string ResolveQuery(const std::string &query, const std::string &key)
        {
            // Check if query is empty
            if (query.empty())
            {
                LOGWARN("Query is empty");
                return "";
            }

            // Find key position
            size_t pos = query.find(key);
            if (pos == std::string::npos)
            {
                LOGWARN("%s not found in query: %s\n", key.c_str(), query.c_str());
                return "";
            }

            std::string value = query.substr(pos + key.length()+1);

            if (value.empty())
            {
                LOGERR("ResolveQuery: '%s' value missing in query: %s\n", key.c_str(), query.c_str());
                return "";
            }

            LOGINFO("ResolveQuery: Extracted %s = %s\n", key.c_str(), value.c_str());
            return value;
        }
    }
}