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

#ifndef __CONTEXTUTILS_H__
#define __CONTEXTUTILS_H__
#include <interfaces/IAppGateway.h>
#include <interfaces/IAppNotifications.h>

#include "UtilsCallsign.h"
using namespace WPEFramework;
using namespace std;
class ContextUtils {
    public:
        // Implement a static method which accepts a Exchange::IAppNotifications::Context object and
        // converts it into Exchange::IAppGateway::Context object
        static Exchange::GatewayContext ConvertNotificationToAppGatewayContext(const Exchange::IAppNotifications::AppNotificationContext& notificationsContext){
            Exchange::GatewayContext appGatewayContext;
            // Perform the conversion logic here
            appGatewayContext.requestId = notificationsContext.requestId;
            appGatewayContext.connectionId = notificationsContext.connectionId;
            appGatewayContext.appId = notificationsContext.appId;
            return appGatewayContext;
        }

        // Implement a static method which accepts a Exchange::IAppGateway::Context object and
        // converts it into Exchange::IAppNotifications::Context object
        static Exchange::IAppNotifications::AppNotificationContext ConvertAppGatewayToNotificationContext(const Exchange::GatewayContext& appGatewayContext, const string& origin){
            Exchange::IAppNotifications::AppNotificationContext notificationsContext;
            // Perform the conversion logic here
            notificationsContext.requestId = appGatewayContext.requestId;
            notificationsContext.connectionId = appGatewayContext.connectionId;
            notificationsContext.appId = appGatewayContext.appId;
            notificationsContext.origin = origin;
            return notificationsContext;
        }

        static bool IsOriginGateway(const string& origin) {
            return origin == APP_GATEWAY_CALLSIGN;
        }
};
#endif
