/**
 * If not stated otherwise in this file or this component's LICENSE
 * file the following copyright and licenses apply:
 *
 * Copyright 2020 RDK Management
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

#ifndef __NETWORKDELEGATE_H__
#define __NETWORKDELEGATE_H__

#include "StringUtils.h"
#include "BaseEventDelegate.h"
#include <interfaces/INetworkManager.h>
#include "UtilsLogging.h"
#include <algorithm>
#include <sstream>
#include <set>

using namespace WPEFramework;

#define NETWORKMANAGER_CALLSIGN "org.rdk.NetworkManager"

// Valid network events that can be subscribed to
static const std::set<string> VALID_NETWORK_EVENT = {
    "device.onnetworkchanged"
};

class NetworkDelegate : public BaseEventDelegate
{
public:
    NetworkDelegate(PluginHost::IShell *shell)
        : BaseEventDelegate(), mNetworkManager(nullptr), mShell(shell), mNotificationHandler(*this)
    {
    }

    ~NetworkDelegate()
    {
        if (mNetworkManager != nullptr)
        {
            mNetworkManager->Release();
            mNetworkManager = nullptr;
        }
    }

    bool HandleSubscription(Exchange::IAppNotificationHandler::IEmitter *cb, const string &event, const bool listen)
    {
        if (listen)
        {
            Exchange::INetworkManager *networkManager = GetNetworkManagerInterface();
            if (networkManager == nullptr)
            {
                LOGERR("NetworkManager interface not available");
                return false;
            }

            AddNotification(event, cb);

            if (!mNotificationHandler.GetRegistered())
            {
                LOGINFO("Registering for NetworkManager notifications");
                mNetworkManager->Register(&mNotificationHandler);
                mNotificationHandler.SetRegistered(true);
                return true;
            }
            else
            {
                LOGTRACE("Is NetworkManager registered = %s", mNotificationHandler.GetRegistered() ? "true" : "false");
            }
        }
        else
        {
            // Not removing the notification subscription for cases where only one event is removed
            RemoveNotification(event, cb);
        }
        return false;
    }

    bool HandleEvent(Exchange::IAppNotificationHandler::IEmitter *cb, const string &event, const bool listen, bool &registrationError)
    {
        LOGDBG("Checking for handle event");
        // Check if event is present in VALID_NETWORK_EVENT make check case insensitive
        if (VALID_NETWORK_EVENT.find(StringUtils::toLower(event)) != VALID_NETWORK_EVENT.end())
        {
            // Handle NetworkManager event
            registrationError = HandleSubscription(cb, event, listen);
            return true;
        }
        return false;
    }

    // Common method to ensure mNetworkManager is available for all APIs
    Exchange::INetworkManager *GetNetworkManagerInterface()
    {
        if (mNetworkManager == nullptr && mShell != nullptr)
        {
            mNetworkManager = mShell->QueryInterfaceByCallsign<Exchange::INetworkManager>(NETWORKMANAGER_CALLSIGN);
            if (mNetworkManager == nullptr)
            {
                LOGERR("Failed to get NetworkManager COM interface");
            }
        }
        return mNetworkManager;
    }

    // PUBLIC_INTERFACE
    Core::hresult GetInternetConnectionStatus(std::string &result)
    {
        /**
         * Retrieve the first connected interface from GetAvailableInterfaces
         * Transform: Map connected interfaces and return type in lowercase with state
         * Transform logic: .result.interfaces| .[] | select(."connected"==true) |
         *                  {type: .interface, state: map_connected(.connected)} |
         *                  .type |= ascii_downcase | [., inputs][0]
         */
        LOGINFO("GetInternetConnectionStatus via NetworkManager");
        result.clear();

        // Get NetworkManager interface
        Exchange::INetworkManager *networkManager = GetNetworkManagerInterface();
        if (networkManager == nullptr)
        {
            LOGERR("NetworkManager interface not available");
            result = "{\"error\":\"NetworkManager not available\"}";
            return Core::ERROR_UNAVAILABLE;
        }

        // Get available interfaces
        Exchange::INetworkManager::IInterfaceDetailsIterator *interfaces = nullptr;
        uint32_t rc = mNetworkManager->GetAvailableInterfaces(interfaces);

        if (rc != Core::ERROR_NONE)
        {
            LOGERR("GetAvailableInterfaces call failed with error: %u", rc);
            result = "{\"error\":\"Failed to get available interfaces\"}";
            return Core::ERROR_GENERAL;
        }

        if (interfaces == nullptr)
        {
            LOGERR("GetAvailableInterfaces returned null iterator");
            result = "{}";
            return Core::ERROR_NONE;
        }

        // Iterate through interfaces and find the first connected one
        Exchange::INetworkManager::InterfaceDetails iface{};
        bool foundConnected = false;

        while (interfaces->Next(iface))
        {
            if (iface.connected)
            {
                // Get interface type string - map enum to string manually
                std::string interfaceType;
                switch (iface.type) {
                    case Exchange::INetworkManager::INTERFACE_TYPE_ETHERNET:
                        interfaceType = "ethernet";
                        break;
                    case Exchange::INetworkManager::INTERFACE_TYPE_WIFI:
                        interfaceType = "wifi";
                        break;
                    default:
                        interfaceType = "unknown";
                        break;
                }

                // Build the result JSON: {"type": "<type>", "state": "connected"}
                std::ostringstream jsonStream;
                jsonStream << "{\"type\":\"" << interfaceType
                           << "\",\"state\":\"connected\"}";
                result = jsonStream.str();

                LOGINFO("Found connected interface: %s", result.c_str());
                foundConnected = true;
                break;
            }
        }

        // Release the iterator
        interfaces->Release();

        if (!foundConnected)
        {
            // No connected interface found
            result = "{}";
            LOGINFO("No connected interface found");
        }

        return Core::ERROR_NONE;
    }

private:
    class NetworkNotificationHandler : public Exchange::INetworkManager::INotification
    {
    public:
        NetworkNotificationHandler(NetworkDelegate &parent) : mParent(parent), registered(false) {}
        ~NetworkNotificationHandler() {}

        void onInterfaceStateChange(const Exchange::INetworkManager::InterfaceState state, const string interface)
        {
            LOGDBG("onInterfaceStateChange: interface=%s, state=%d", interface.c_str(), state);
        }

        void onActiveInterfaceChange(const string prevActiveInterface, const string currentActiveInterface)
        {
            LOGDBG("onActiveInterfaceChange: prev=%s, current=%s", prevActiveInterface.c_str(), currentActiveInterface.c_str());
        }

        void onIPAddressChange(const string interface, const string ipversion, const string ipaddress, const Exchange::INetworkManager::IPStatus status)
        {
            LOGDBG("onIPAddressChange: interface=%s, ip=%s, status=%d", interface.c_str(), ipaddress.c_str(), status);
        }

        void onInternetStatusChange(const Exchange::INetworkManager::InternetStatus prevState, const Exchange::INetworkManager::InternetStatus currState, const string interface)
        {
            LOGINFO("onInternetStatusChange: prevState=%d, currState=%d, interface=%s", prevState, currState, interface.c_str());

            // Map internet status to readable strings
            auto statusToString = [](Exchange::INetworkManager::InternetStatus status) -> string {
                switch (status) {
                    case Exchange::INetworkManager::INTERNET_FULLY_CONNECTED: return "connected";
                    case Exchange::INetworkManager::INTERNET_CAPTIVE_PORTAL: return "captive_portal";
                    case Exchange::INetworkManager::INTERNET_LIMITED: return "limited";
                    case Exchange::INetworkManager::INTERNET_NOT_AVAILABLE: return "not_available";
                    default: return "unknown";
                }
            };

            // Dispatch network change event for internet status changes
            std::ostringstream jsonStream;
            jsonStream << "{\"network\":{\"state\":\"" << statusToString(currState) 
                      << "\",\"prevState\":\"" << statusToString(prevState) << "\"}}";
            mParent.Dispatch("device.onNetworkChanged", jsonStream.str());
        }

        void onAvailableSSIDs(const string jsonOfScanResults)
        {
            LOGDBG("onAvailableSSIDs received");
        }

        void onWiFiStateChange(const Exchange::INetworkManager::WiFiState state)
        {
            LOGDBG("onWiFiStateChange: state=%d", state);
        }

        void onWiFiSignalQualityChange(const string ssid, const string strength, const string noise, const string snr, const Exchange::INetworkManager::WiFiSignalQuality quality)
        {
            LOGDBG("onWiFiSignalQualityChange: ssid=%s", ssid.c_str());
        }

        // Registration management methods
        void SetRegistered(bool state)
        {
            std::lock_guard<std::mutex> lock(registerMutex);
            registered = state;
        }

        bool GetRegistered()
        {
            std::lock_guard<std::mutex> lock(registerMutex);
            return registered;
        }

        BEGIN_INTERFACE_MAP(NotificationHandler)
        INTERFACE_ENTRY(Exchange::INetworkManager::INotification)
        END_INTERFACE_MAP

    private:
        NetworkDelegate &mParent;
        bool registered;
        std::mutex registerMutex;
    };

    Exchange::INetworkManager *mNetworkManager;
    PluginHost::IShell *mShell;
    Core::Sink<NetworkNotificationHandler> mNotificationHandler;
};

#endif // __NETWORKDELEGATE_H__

