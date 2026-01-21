/*
 * Copyright 2023 Comcast Cable Communications Management, LLC
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
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "AppNotificationsImplementation.h"
#include "UtilsLogging.h"
#include "StringUtils.h"

namespace WPEFramework
{
    namespace Exchange {
        bool operator==(const IAppNotifications::AppNotificationContext& lhs,
                        const IAppNotifications::AppNotificationContext& rhs) {
            return lhs.requestId == rhs.requestId &&
                lhs.connectionId == rhs.connectionId &&
                lhs.appId == rhs.appId &&
                lhs.origin == rhs.origin;
        }
    }
    namespace Plugin
    {
        SERVICE_REGISTRATION(AppNotificationsImplementation, 1, 0);

        AppNotificationsImplementation::AppNotificationsImplementation() : 
        mShell(nullptr),
        mSubMap(*this),
        mThunderManager(*this),
        mEmitter(*this)
        {
        }

        AppNotificationsImplementation::~AppNotificationsImplementation()
        {
            // Cleanup resources if needed
            if (mShell != nullptr)
            {
                mShell->Release();
                mShell = nullptr;
            }
        }

        Core::hresult AppNotificationsImplementation::Subscribe(const Exchange::IAppNotifications::AppNotificationContext &context /* @in */,
                                            bool listen /* @in */,
                                            const string &module /* @in */,
                                            const string &event /* @in */) {
            LOGTRACE("Subscribe [requestId=%d appId=%s connectionId=%d] register=%s, module=%s, event=%s",
                    context.requestId, context.appId.c_str(), context.connectionId,
                    listen ? "true" : "false", module.c_str(), event.c_str());
            if (listen) {
                if (!mSubMap.Exists(module)) {
                    // Thunder subscription
                    Core::IWorkerPool::Instance().Submit(SubscriberJob::Create(this, module, event, listen));
                }
                mSubMap.Add(event, context);
            } else {
                mSubMap.Remove(event, context);
                // If all elements are removed the entry is erased automatically
                // This can be used to measure unsubscribe
                if (!mSubMap.Exists(event)) {
                    // Thunder unsubscription
                    Core::IWorkerPool::Instance().Submit(SubscriberJob::Create(this, module, event, listen));
                }
            }
            return Core::ERROR_NONE;
        }

        Core::hresult AppNotificationsImplementation::Emit(const string &event /* @in */,
                                    const string &payload /* @in @opaque */,
                                    const string &appId /* @in */) {
            LOGTRACE("Emit [event= %s payload=%s appId=%s]",
                    event.c_str(), payload.c_str(), appId.c_str());
            Core::IWorkerPool::Instance().Submit(EmitJob::Create(this, event, payload, appId));
            return Core::ERROR_NONE;
        }

        Core::hresult AppNotificationsImplementation::Cleanup(const uint32_t connectionId /* @in */, const string &origin /* @in */) {
            LOGTRACE("Cleanup [connectionId=%d origin=%s]", connectionId, origin.c_str());
            mSubMap.CleanupNotifications(connectionId, origin);
            return Core::ERROR_NONE;
        }

        void AppNotificationsImplementation::SubscriberMap::CleanupNotifications(const uint32_t &connectionId, const string& origin) {
            std::lock_guard<std::mutex> lock(mSubscriberMutex);
            for (auto it = mSubscribers.begin(); it != mSubscribers.end(); ) {
                auto& vec = it->second;
                vec.erase(std::remove_if(vec.begin(), vec.end(),
                    [&](const Exchange::IAppNotifications::AppNotificationContext& context) {
                        return (context.connectionId == connectionId) && (context.origin == origin);
                    }), vec.end());
                if (vec.empty()) {
                    it = mSubscribers.erase(it);
                } else {
                    ++it;
                }
            }
        }

        uint32_t AppNotificationsImplementation::Configure(PluginHost::IShell *shell)
        {
            LOGINFO("Configuring AppNotifications");
            uint32_t result = Core::ERROR_NONE;
            ASSERT(shell != nullptr);
            mShell = shell;
            mShell->AddRef();
            return result;
        }

        void AppNotificationsImplementation::SubscriberMap::Add(const string& key, const Exchange::IAppNotifications::AppNotificationContext& context) {
            string lowerKey = StringUtils::toLower(key);
            std::lock_guard<std::mutex> lock(mSubscriberMutex);
            mSubscribers[lowerKey].push_back(context);
        }
        
        void AppNotificationsImplementation::SubscriberMap::Remove(const string& key, const Exchange::IAppNotifications::AppNotificationContext& context) {
            std::lock_guard<std::mutex> lock(mSubscriberMutex);
            string lowerKey = StringUtils::toLower(key);
            auto it = mSubscribers.find(lowerKey);
            if (it != mSubscribers.end()) {
                auto& vec = it->second;
                vec.erase(std::remove(vec.begin(), vec.end(), context), vec.end());
                if (vec.empty()) {
                mSubscribers.erase(it);
                }
            }
        }

        std::vector<Exchange::IAppNotifications::AppNotificationContext> AppNotificationsImplementation::SubscriberMap::Get(const string& key) const {
            std::lock_guard<std::mutex> lock(mSubscriberMutex);
            string lowerKey = StringUtils::toLower(key);
            auto it = mSubscribers.find(lowerKey);
            if (it != mSubscribers.end()) {
                return it->second;
            }
            return {};
        }

        bool AppNotificationsImplementation::SubscriberMap::Exists(const string& key) const {
            std::lock_guard<std::mutex> lock(mSubscriberMutex);
            string lowerKey = StringUtils::toLower(key);
            auto it = mSubscribers.find(lowerKey);
            return it != mSubscribers.end();
        }

        void AppNotificationsImplementation::SubscriberMap::EventUpdate(const string& key, const string& payloadStr, const string& appId ) {                

            std::lock_guard<std::mutex> lock(mSubscriberMutex);
            string lowerKey = StringUtils::toLower(key);
            auto it = mSubscribers.find(lowerKey);
            if (it != mSubscribers.end()) {
                for (const auto& context : it->second) {
                    // check if app id is not empty if not empty check if context.appId matches appId
                    if (!appId.empty()) {

                        if(context.appId == appId) {
                            // Dispatch the event to the subscriber
                            Dispatch(context, payloadStr);
                        }
                        
                    } else {
                        // Dispatch the event to the subscriber
                        Dispatch(context, payloadStr);
                    }
                }
            } else {
                // using LOGWARN print a warning that there are no active listeners for this event
                LOGWARN("No active listeners for event: %s", key.c_str());
            }
        }

        void AppNotificationsImplementation::SubscriberMap::Dispatch(const Exchange::IAppNotifications::AppNotificationContext& context, const string& payload) {
            if (ContextUtils::IsOriginGateway(context.origin)) {
                DispatchToGateway(context, payload);
            } else {
                DispatchToLaunchDelegate(context, payload);
            }
        }

        void AppNotificationsImplementation::SubscriberMap::DispatchToGateway(const Exchange::IAppNotifications::AppNotificationContext& context, const string& payload) {
            if (nullptr == mAppGateway) {
                mAppGateway = mParent.mShell->QueryInterfaceByCallsign<Exchange::IAppGatewayResponder>(APP_GATEWAY_CALLSIGN);
                if (mAppGateway == nullptr) {
                    LOGERR("Failed to get IAppGateway interface");
                    return;
                }
            }
            Exchange::GatewayContext gatewayContext = ContextUtils::ConvertNotificationToAppGatewayContext(context);
            mAppGateway->Respond(gatewayContext, payload);
        }

        void AppNotificationsImplementation::SubscriberMap::DispatchToLaunchDelegate(const Exchange::IAppNotifications::AppNotificationContext& context, const string& payload) {
            if (nullptr == mInternalGatewayNotifier) {
                mInternalGatewayNotifier = mParent.mShell->QueryInterfaceByCallsign<Exchange::IAppGatewayResponder>(INTERNAL_GATEWAY_CALLSIGN);
                if (mInternalGatewayNotifier == nullptr) {
                    LOGERR("Failed to get ILaunchDelegate interface");
                    return;
                }
            }
            Exchange::GatewayContext gatewayContext = ContextUtils::ConvertNotificationToAppGatewayContext(context);
            mInternalGatewayNotifier->Respond(gatewayContext, payload);
        }

        AppNotificationsImplementation::ThunderSubscriptionManager::~ThunderSubscriptionManager() {
            // Copy notifications to avoid holding the lock during external calls
            std::vector<NotificationKey> notificationsCopy;
            {
                std::lock_guard<std::mutex> lock(mThunderSubscriberMutex);
                notificationsCopy = mRegisteredNotifications;
                mRegisteredNotifications.clear();
            }
            // Unsubscribe from all registered notifications outside the lock
            for (const auto& notification : notificationsCopy) {
                HandleNotifier(notification.module, notification.event, false);
            }
        }

        void AppNotificationsImplementation::ThunderSubscriptionManager::Subscribe(const string& module, const string& event) {
            // Subscribe to Thunder notifications

            // check if the notification is already registered
            if (IsNotificationRegistered(module, event)) {
                // log a trace message that the notification is already registered
                LOGTRACE("Notification is already registered: %s", event.c_str());
            } else {
                // trigger RegisterNotification
                RegisterNotification(module, event);
            }
        }

        void AppNotificationsImplementation::ThunderSubscriptionManager::Unsubscribe(const string& module, const string& event) {
            // Unsubscribe from Thunder notifications
            
            // check if the notification is already registered
            if (IsNotificationRegistered(module, event)) {
                // trigger UnregisterNotification
                UnregisterNotification(module, event);
            } else {
                // log an error that the notification is not registered
                LOGERR("Notification is not registered: %s", event.c_str());
            }
        }

        bool AppNotificationsImplementation::ThunderSubscriptionManager::HandleNotifier(const string& module, const string& event, const bool& listen) {
            // Check if Plugins is activated before making a request
            bool status = false;
            Exchange::IAppNotificationHandler *internalNotifier = mParent.mShell->QueryInterfaceByCallsign<Exchange::IAppNotificationHandler>(module);
            if (internalNotifier != nullptr) {
                if (Core::ERROR_NONE == internalNotifier->HandleAppEventNotifier(&mParent.mEmitter, event, listen, status)) {
                    LOGTRACE("Notifier status for %s:%s is %s", module.c_str(), event.c_str(), status ? "true" : "false");
                } else {
                    LOGERR("Notification subscription failure");
                }
                internalNotifier->Release();
            } else {
                LOGERR("Notification Handler not available for module=%s", module.c_str());
            }
            return status;
        }

        void AppNotificationsImplementation::ThunderSubscriptionManager::RegisterNotification(const string& module, const string& event) {
            string lowerEvent = StringUtils::toLower(event);
            // call notifier and start listening
            if (HandleNotifier(module, event, true)) {
                std::lock_guard<std::mutex> lock(mThunderSubscriberMutex);
                mRegisteredNotifications.push_back({module, std::move(lowerEvent)});
            }
        }

        void AppNotificationsImplementation::ThunderSubscriptionManager::UnregisterNotification(const string& module, const string& event) {
            string lowerEvent = StringUtils::toLower(event);
            if (HandleNotifier(module, lowerEvent, false)) {
                std::lock_guard<std::mutex> lock(mThunderSubscriberMutex);
                mRegisteredNotifications.erase(std::remove(mRegisteredNotifications.begin(), mRegisteredNotifications.end(), NotificationKey{module, lowerEvent}), mRegisteredNotifications.end());
            }
        }

        bool AppNotificationsImplementation::ThunderSubscriptionManager::IsNotificationRegistered(const string& module, const string& notification) const {
            string lowerEvent = StringUtils::toLower(notification);
            std::lock_guard<std::mutex> lock(mThunderSubscriberMutex);
            return std::find(mRegisteredNotifications.begin(), mRegisteredNotifications.end(), NotificationKey{module, lowerEvent}) != mRegisteredNotifications.end();
        }

    }
}
