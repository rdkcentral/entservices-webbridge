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

#ifndef __SETTINGSDELEGATE_H__
#define __SETTINGSDELEGATE_H__
#include "StringUtils.h"
#include "UserSettingsDelegate.h"
#include "SystemDelegate.h"
#include "NetworkDelegate.h"
#include "UtilsLogging.h"
#include <interfaces/IAppNotifications.h>

#define APP_NOTIFICATIONS_CALLSIGN "org.rdk.AppNotifications"
using namespace WPEFramework;

class SettingsDelegate {
    public:
        SettingsDelegate(): userSettings(nullptr), systemDelegate(nullptr), networkDelegate(nullptr) {}

        ~SettingsDelegate() {
            userSettings = nullptr;
            systemDelegate = nullptr;
            networkDelegate = nullptr;
        }

        void HandleAppEventNotifier(Exchange::IAppNotificationHandler::IEmitter *cb, const string event,
                                    const bool listen) {
            LOGDBG("Passing on HandleAppEventNotifier");
            bool registrationError;
            if (userSettings==nullptr || systemDelegate==nullptr || networkDelegate==nullptr) {
                LOGERR("Services not available");
                return;
            }

            std::vector<std::shared_ptr<BaseEventDelegate>> delegates = {userSettings, systemDelegate, networkDelegate};
            bool handled = false;

            for (const auto& delegate : delegates) {
                if (delegate==nullptr) {
                    continue;
                }
                if (delegate->HandleEvent(cb, event, listen, registrationError)) {
                    handled = true;
                    break;
                }
            }

            if (!handled) {
                LOGERR("No Matching registrations");
            }

            if (registrationError) {
                LOGERR("Error in registering/unregistering for event %s", event.c_str());
            }
        }

        void setShell(PluginHost::IShell* shell) {

            ASSERT(shell != nullptr);
            LOGDBG("SettingsDelegate::setShell");

            if (userSettings == nullptr) {
                userSettings = std::make_shared<UserSettingsDelegate>(shell);
            }

            if (systemDelegate == nullptr) {
                systemDelegate = std::make_shared<SystemDelegate>(shell);
            }

            if (networkDelegate == nullptr) {
                networkDelegate = std::make_shared<NetworkDelegate>(shell);
            }
        }

        void Cleanup() {
            systemDelegate.reset();
            userSettings.reset();
            networkDelegate.reset();
        }

	std::shared_ptr<SystemDelegate> getSystemDelegate() const {
            return systemDelegate;
        }

	std::shared_ptr<UserSettingsDelegate> getUserSettings() {
            return userSettings;
        }

        std::shared_ptr<NetworkDelegate> getNetworkDelegate() const {
            return networkDelegate;
        }
    private:
        std::shared_ptr<UserSettingsDelegate> userSettings;
        std::shared_ptr<SystemDelegate> systemDelegate;
        std::shared_ptr<NetworkDelegate> networkDelegate;
};

#endif


