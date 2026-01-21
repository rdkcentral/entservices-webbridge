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

        uint32_t GetThunderSecurityToken(PluginHost::IShell *service, std::string &token)
        {
            uint32_t result = Core::ERROR_GENERAL;
            auto security = service->QueryInterfaceByCallsign<PluginHost::IAuthenticate>("SecurityAgent");
            if (security != nullptr)
            {
                string payload = "http://localhost";
                if (security->CreateToken(
                        static_cast<uint16_t>(payload.length()),
                        reinterpret_cast<const uint8_t *>(payload.c_str()),
                        token) == Core::ERROR_NONE)
                {
                    LOGINFO("Got security token\n");
                    result = Core::ERROR_NONE;
                }
                else
                {
                    LOGINFO("Failed to get security token\n");
                }
                security->Release();
            }
            else
            {
                LOGINFO("No security agent\n");
                result = Core::ERROR_NONE; // No security agent, so no token needed
            }

            return result;
        }

        struct JSONRPCDirectLink
        {
        private:
            uint32_t mId{0};
            std::string mCallSign{};
            std::string mThunderSecurityToken{};
            PluginHost::ILocalDispatcher *mDispatcher{nullptr};

            bool ToString(std::string &out, const std::string &in) const
            {
                out = in;
                return true;
            }

            bool ToString(std::string &out, const Core::JSON::IElement &in) const
            {
                std::string text;
                if (in.ToString(text) == false)
                {
                    LOGERR("Failed to serialize parameters!!!");
                    return false;
                }
                out = std::move(text);
                return true;
            }

            bool FromString(std::string &out, const std::string &in) const
            {
                out = in;
                return true;
            }

            bool FromString(Core::JSON::IElement& response, const std::string &in) const
            {
                Core::OptionalType<Core::JSON::Error> error;
                if (response.FromString(in, error) == false)
                {
                    LOGERR("Failed to parse response!!! Error: %s", error.Value().Message().c_str());
                    return false;
                }
                return true;
            }

        public:
            JSONRPCDirectLink(PluginHost::IShell *service, const std::string &callsign, const std::string &token = "")
                : mCallSign(callsign)
                , mThunderSecurityToken(token)
            {
                if (service)
                {
                    mDispatcher = service->QueryInterfaceByCallsign<PluginHost::ILocalDispatcher>(mCallSign);
                }
            }

            JSONRPCDirectLink(PluginHost::IShell *service)
                : JSONRPCDirectLink(service, "Controller")
            {
            }

            ~JSONRPCDirectLink()
            {
                if (mDispatcher)
                {
                    mDispatcher->Release();
                }
            }

            template <typename PARAMETERS, typename RESPONSE>
            Core::hresult Invoke(const string &method, const PARAMETERS &parameters, RESPONSE &response)
            {
                if (mDispatcher == nullptr)
                {
                    LOGERR("No JSON RPC dispatcher for %s", mCallSign.c_str());
                    return Core::ERROR_GENERAL;
                }

                const uint32_t channelId = ~0;
                uint32_t id = ++mId;
                std::string designator = mCallSign + ".1." + method;
                std::string parametersStr;
                if (ToString(parametersStr, parameters) == false)
                {
                    return Core::ERROR_GENERAL;
                }

                string responseStr = "";
                Core::hresult result = Core::ERROR_BAD_REQUEST;

                if (mDispatcher != nullptr)
                {
                    if (mDispatcher->Local() != nullptr)
                    {
                        result = mDispatcher->Invoke(channelId, id, mThunderSecurityToken, designator, parametersStr, responseStr);

                        if (result != Core::ERROR_NONE)
                        {
                            LOGERR("Call failed: %s (parameters: %s) error: %d, response: %s", designator.c_str(), parametersStr.c_str(), result, responseStr.c_str());
                        }
                        
                        if (FromString(response, responseStr) == false)
                        {
                            result = Core::ERROR_GENERAL;
                        }
                    }
                }

                return result;
            }
        };

        std::shared_ptr<Utils::JSONRPCDirectLink> GetThunderControllerClient(PluginHost::IShell *service, std::string callsign="")
        {
            static std::string sThunderSecurityToken("");
            static bool sThunderSecurityTokenAcquired(false);

            if (sThunderSecurityTokenAcquired == false) {
                sThunderSecurityTokenAcquired =
                    (GetThunderSecurityToken(service, sThunderSecurityToken) == Core::ERROR_NONE);
            }

            return std::make_shared<Utils::JSONRPCDirectLink>(service, callsign, sThunderSecurityToken);
        }

    }
}
