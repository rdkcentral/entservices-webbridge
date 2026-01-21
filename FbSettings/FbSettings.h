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

#pragma once

#include "Module.h"
#include <interfaces/IAppGateway.h>
#include <interfaces/IAppNotifications.h>
#include <mutex>
#include <map>
#include "UtilsLogging.h"
#include "UtilsController.h"
#include "delegate/SettingsDelegate.h"

namespace WPEFramework {

    namespace Plugin {


		class FbSettings : public PluginHost::IPlugin, Exchange::IAppGatewayRequestHandler, Exchange::IAppNotificationHandler{
        private:
            // We do not allow this plugin to be copied !!
            FbSettings(const FbSettings&) = delete;
            FbSettings& operator=(const FbSettings&) = delete;

            class EXTERNAL EventRegistrationJob : public Core::IDispatch
        {
            protected:
                EventRegistrationJob(FbSettings *parent,
                Exchange::IAppNotificationHandler::IEmitter *cb,
                const string &event,
                const bool listen): mParent(*parent), mCallback(cb), mEvent(event), mListen(listen) {
                    if (mCallback != nullptr) {
                        mCallback->AddRef();
                    }
                }
            public:
                EventRegistrationJob() = delete;
                EventRegistrationJob(const EventRegistrationJob &) = delete;
                EventRegistrationJob &operator=(const EventRegistrationJob &) = delete;
                ~EventRegistrationJob()
                {
                    if (mCallback != nullptr) {
                        mCallback->Release();
                        mCallback = nullptr;
                    }
                }

                static Core::ProxyType<Core::IDispatch> Create(FbSettings *parent,
                Exchange::IAppNotificationHandler::IEmitter *cb, const string& event, const bool listen)
                {
                    return (Core::ProxyType<Core::IDispatch>(Core::ProxyType<EventRegistrationJob>::Create(parent, cb, event, listen)));
                }
                virtual void Dispatch()
                {
                    mParent.mDelegate->HandleAppEventNotifier(mCallback, mEvent, mListen);
                }

            private:
            FbSettings &mParent;
            Exchange::IAppNotificationHandler::IEmitter *mCallback;
            const string mEvent;
            const bool mListen;

        };


        public:
            FbSettings();
            virtual ~FbSettings();
            virtual const string Initialize(PluginHost::IShell* shell) override;
            virtual void Deinitialize(PluginHost::IShell* service) override;
            virtual string Information() const override { return {}; }

            BEGIN_INTERFACE_MAP(FbSettings)
            INTERFACE_ENTRY(PluginHost::IPlugin)
            INTERFACE_ENTRY(Exchange::IAppGatewayRequestHandler)
            INTERFACE_ENTRY(Exchange::IAppNotificationHandler)
            END_INTERFACE_MAP
        
        public:
            virtual Core::hresult HandleAppEventNotifier(Exchange::IAppNotificationHandler::IEmitter *cb /* @in */,
                const string& event /* @in */,
                bool listen /* @in */,
                bool& status /* @out */) override;
            
            virtual Core::hresult HandleAppGatewayRequest(const Exchange::GatewayContext &context /* @in */,
                                          const string& method /* @in */,
                                          const string &payload /* @in @opaque */,
                                          string& result /*@out @opaque */) override;

        private:
            void Deactivated(RPC::IRemoteConnection* connection);
            // Helper methods for System/Device - called by HandleAppGatewayRequest
            Core::hresult GetDeviceMake(string &make /* @out */);
            Core::hresult GetDeviceName(string &name /* @out */);
            Core::hresult SetDeviceName(const string name /* @in */);
            Core::hresult GetDeviceSku(string &sku /* @out */);
            Core::hresult GetCountryCode(string &countryCode /* @out */);
            Core::hresult SetCountryCode(const string countryCode /* @in */);
            Core::hresult GetTimeZone(string &timeZone /* @out */);
            Core::hresult SetTimeZone(const string timeZone /* @in */);
            Core::hresult GetSecondScreenFriendlyName(string &name /* @out */);
            Core::hresult SetName(const string &value /* @in */, string &result /* @out */);
            Core::hresult AddAdditionalInfo(const string &value /* @in */, string &result /* @out */);

            // Helper methods for network status - called by HandleAppGatewayRequest
            Core::hresult GetInternetConnectionStatus(string &result /* @out */);

            // Helper methods for UserSettings - called by HandleAppGatewayRequest
            Core::hresult GetVoiceGuidance(string &result /* @out */);
            Core::hresult GetAudioDescription(string &result /* @out */);
            Core::hresult GetAudioDescriptionsEnabled(string &result /* @out */);
            Core::hresult GetHighContrast(string &result /* @out */);
            Core::hresult GetCaptions(string &result /* @out */);
            Core::hresult SetVoiceGuidance(const bool enabled /* @in */);
            Core::hresult SetAudioDescriptionsEnabled(const bool enabled /* @in */);
            Core::hresult SetCaptions(const bool enabled /* @in */);
            Core::hresult GetPresentationLanguage(string &result /* @out */);
            Core::hresult GetLocale(string &result /* @out */);
            Core::hresult SetLocale(const string &locale /* @in */);
            Core::hresult GetPreferredAudioLanguages(string &result /* @out */);
            Core::hresult GetPreferredCaptionsLanguages(string &result /* @out */);
            Core::hresult SetPreferredAudioLanguages(const string &languages /* @in */);
            Core::hresult SetPreferredCaptionsLanguages(const string &preferredLanguages /* @in */);
            Core::hresult SetSpeed(const double speed /* @in */);
            Core::hresult GetSpeed(double &speed /* @out */);
            Core::hresult GetVoiceGuidanceHints(string &result /* @out */);
            Core::hresult SetVoiceGuidanceHints(const bool enabled /* @in */);
            Core::hresult GetVoiceGuidanceSettings(string &result /* @out */);
            Core::hresult GetClosedCaptionsSettings(string &result /* @out */);
            Core::hresult GetFirmwareVersion(string &result /* @out */);
            Core::hresult GetScreenResolution(string &result /* out */);
            Core::hresult GetVideoResolution(string &result /* out */);
            Core::hresult GetHdcp(string &result /* @out */);
            Core::hresult GetHdr(string &result /* @out */);
            Core::hresult GetAudio(string &result /* @out */);

        private:
            PluginHost::IShell* mShell;
            uint32_t mConnectionId;
            std::shared_ptr<SettingsDelegate> mDelegate;
        };
	} // namespace Plugin
} // namespace WPEFramework

