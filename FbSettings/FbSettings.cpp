/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
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
 */

#include "FbSettings.h"
#include <interfaces/IConfiguration.h>
#include "StringUtils.h"
#include "UtilsFirebolt.h"


#define API_VERSION_NUMBER_MAJOR    FBSETTINGS_MAJOR_VERSION
#define API_VERSION_NUMBER_MINOR    FBSETTINGS_MINOR_VERSION
#define API_VERSION_NUMBER_PATCH    FBSETTINGS_PATCH_VERSION

namespace WPEFramework {

namespace {
    static Plugin::Metadata<Plugin::FbSettings> metadata(
        // Version (Major, Minor, Patch)
        API_VERSION_NUMBER_MAJOR, API_VERSION_NUMBER_MINOR, API_VERSION_NUMBER_PATCH,
        // Preconditions
        {},
        // Terminations
        {},
        // Controls
        {}
    );
}

namespace Plugin {
    SERVICE_REGISTRATION(FbSettings, API_VERSION_NUMBER_MAJOR, API_VERSION_NUMBER_MINOR, API_VERSION_NUMBER_PATCH);

    FbSettings::FbSettings(): mShell(nullptr), mConnectionId(0)
    {
        SYSLOG(Logging::Startup, (_T("FbSettings Constructor")));
    }

    FbSettings::~FbSettings()
    {
        SYSLOG(Logging::Shutdown, (string(_T("FbSettings Destructor"))));
    }

    /* virtual */ const string FbSettings::Initialize(PluginHost::IShell* service)
    {
        ASSERT(service != nullptr);

        SYSLOG(Logging::Startup, (_T("FbSettings::Initialize: PID=%u"), getpid()));

        mShell = service;
        mShell->AddRef();

        // Initialize the settings delegate
        mDelegate = std::make_shared<SettingsDelegate>();
        mDelegate->setShell(mShell);

        return EMPTY_STRING;
    }

    /* virtual */ void FbSettings::Deinitialize(PluginHost::IShell* service)
    {
        SYSLOG(Logging::Shutdown, (string(_T("FbSettings::Deinitialize"))));
        ASSERT(service == mShell);
        mConnectionId = 0;

        mDelegate->Cleanup();
        // Clean up the delegate
        mDelegate.reset();

        mShell->Release();
        mShell = nullptr;
        SYSLOG(Logging::Shutdown, (string(_T("FbSettings de-initialised"))));
    }

    void FbSettings::Deactivated(RPC::IRemoteConnection* connection)
    {
        if (connection->Id() == mConnectionId) {

            ASSERT(mShell != nullptr);

            Core::IWorkerPool::Instance().Submit(PluginHost::IShell::Job::Create(mShell, PluginHost::IShell::DEACTIVATED, PluginHost::IShell::FAILURE));
        }
    }

    Core::hresult FbSettings::HandleAppEventNotifier(Exchange::IAppNotificationHandler::IEmitter *cb, const string& event /* @in */,
                                    bool listen /* @in */,
                                    bool &status /* @out */) {
            LOGTRACE("HandleFireboltNotifier [event=%s listen=%s]",
                    event.c_str(), listen ? "true" : "false");
            status = true;
            Core::IWorkerPool::Instance().Submit(EventRegistrationJob::Create(this, cb, event, listen));
            return Core::ERROR_NONE;
    }

    Core::hresult FbSettings::HandleAppGatewayRequest(const Exchange::GatewayContext &context /* @in */,
                                          const string& method /* @in */,
                                          const string &payload /* @in @opaque */,
                                          string& result /*@out @opaque */)
        {
            LOGTRACE("HandleAppGatewayRequest: method=%s, payload=%s, appId=%s",
                    method.c_str(), payload.c_str(), context.appId.c_str());
            std::string lowerMethod = StringUtils::toLower(method);
            // Route System/Device methods
            if (lowerMethod == "device.make")
            {
                return GetDeviceMake(result);
            }
            else if (lowerMethod == "device.name")
            {
                return GetDeviceName(result);
            }
            else if (lowerMethod == "device.setname")
            {
                JsonObject params;
                if (params.FromString(payload))
                {
                    string name = params.Get("value").String();
                    return ResponseUtils::SetNullResponseForSuccess(SetDeviceName(name), result);
                }
                result = "{\"error\":\"Invalid payload\"}";
                return Core::ERROR_BAD_REQUEST;
            }
            else if (lowerMethod == "device.sku")
            {
                return GetDeviceSku(result);
            }
            else if (lowerMethod == "localization.countrycode")
            {
                return GetCountryCode(result);
            }
            else if (lowerMethod == "localization.setcountrycode")
            {
                JsonObject params;
                if (params.FromString(payload))
                {
                    string countryCode = params.Get("value").String();
                    return ResponseUtils::SetNullResponseForSuccess(SetCountryCode(countryCode),result);
                }
                result = "{\"error\":\"Invalid payload\"}";
                return Core::ERROR_BAD_REQUEST;
            }
            else if (lowerMethod == "localization.timezone")
            {
                return GetTimeZone(result);
            }
            else if (lowerMethod == "localization.settimezone")
            {
                JsonObject params;
                if (params.FromString(payload))
                {
                    string timeZone = params.Get("value").String();
                    return ResponseUtils::SetNullResponseForSuccess(SetTimeZone(timeZone),result);
                }
                result = "{\"error\":\"Invalid payload\"}";
                return Core::ERROR_BAD_REQUEST;
            }
            else if (lowerMethod == "secondscreen.friendlyname")
            {
                return GetSecondScreenFriendlyName(result);
            }
            else if (lowerMethod == "localization.addadditionalinfo")
            {
                return ResponseUtils::SetNullResponseForSuccess(AddAdditionalInfo(payload, result), result);
            }

            // Route network-related methods
            else if (lowerMethod == "device.network")
            {
                return GetInternetConnectionStatus(result);
            }

            // Route voice guidance methods
            else if (lowerMethod == "voiceguidance.enabled")
            {
                return GetVoiceGuidance(result);
            }
            else if (lowerMethod == "voiceguidance.setenabled")
            {
                // Parse payload for boolean value
                JsonObject params;
                if (params.FromString(payload))
                {
                    bool enabled = params.Get("value").Boolean();
                    return ResponseUtils::SetNullResponseForSuccess(SetVoiceGuidance(enabled), result);
                }
                result = "{\"error\":\"Invalid payload\"}";
                return Core::ERROR_BAD_REQUEST;
            }
            else if (lowerMethod == "voiceguidance.speed" || lowerMethod == "voiceguidance.rate")
            {
                double speed;
                Core::hresult status = GetSpeed(speed);
                if (status == Core::ERROR_NONE)
                {
                    std::ostringstream jsonStream;
                    jsonStream << speed;
                    result = jsonStream.str();
                }
                return status;
            }
            else if (lowerMethod == "voiceguidance.setspeed" || lowerMethod == "voiceguidance.setrate")
            {
                JsonObject params;
                if (params.FromString(payload))
                {
                    double speed = params.Get("value").Number();
                    return ResponseUtils::SetNullResponseForSuccess(SetSpeed(speed), result);
                }
                result = "{\"error\":\"Invalid payload\"}";
                return Core::ERROR_BAD_REQUEST;
            }
            else if (lowerMethod == "voiceguidance.navigationhints")
            {
                return GetVoiceGuidanceHints(result);
            }
            else if (lowerMethod == "voiceguidance.setnavigationhints")
            {
                JsonObject params;
                if (params.FromString(payload))
                {
                    bool enabled = params.Get("value").Boolean();
                    return ResponseUtils::SetNullResponseForSuccess(SetVoiceGuidanceHints(enabled), result);
                }
                result = "{\"error\":\"Invalid payload\"}";
                return Core::ERROR_BAD_REQUEST;
            }
            else if (lowerMethod == "accessibility.voiceguidancesettings")
            {
                return GetVoiceGuidanceSettings(result);
            }
            else if (lowerMethod == "accessibility.voiceguidance")
            {
                return GetVoiceGuidanceSettings(result);
            }

            // Route audio description methods
            else if (lowerMethod == "accessibility.audiodescriptionsettings")
            {
                return GetAudioDescription(result);
            }
            else if (lowerMethod == "audiodescriptions.enabled")
            {
                return GetAudioDescriptionsEnabled(result);
            }
            else if (lowerMethod == "audiodescriptions.setenabled")
            {
                JsonObject params;
                if (params.FromString(payload))
                {
                    bool enabled = params.Get("value").Boolean();
                    return ResponseUtils::SetNullResponseForSuccess(SetAudioDescriptionsEnabled(enabled), result);
                }
                result = "{\"error\":\"Invalid payload\"}";
                return Core::ERROR_BAD_REQUEST;
            }

            // Route accessibility methods
            else if (lowerMethod == "accessibility.highcontrastui")
            {
                return GetHighContrast(result);
            }

            // Route closed captions methods
            else if (lowerMethod == "closedcaptions.enabled")
            {
                return GetCaptions(result);
            }
            else if (lowerMethod == "closedcaptions.setenabled")
            {
                JsonObject params;
                if (params.FromString(payload))
                {
                    bool enabled = params.Get("value").Boolean();
                    return ResponseUtils::SetNullResponseForSuccess(SetCaptions(enabled), result);
                }
                result = "{\"error\":\"Invalid payload\"}";
                return Core::ERROR_BAD_REQUEST;
            }
            else if (lowerMethod == "closedcaptions.preferredlanguages")
            {
                return GetPreferredCaptionsLanguages(result);
            }
            else if (lowerMethod == "closedcaptions.setpreferredlanguages")
            {
                JsonObject params;
                if (params.FromString(payload))
                {
                    string languages = params.Get("value").String();
                    return ResponseUtils::SetNullResponseForSuccess(SetPreferredCaptionsLanguages(languages), result);
                }
                result = "{\"error\":\"Invalid payload\"}";
                return Core::ERROR_BAD_REQUEST;
            }
            else if (lowerMethod == "accessibility.closedcaptions")
            {
                return GetClosedCaptionsSettings(result);
            }
            else if (lowerMethod == "accessibility.closedcaptionssettings")
            {
                return GetClosedCaptionsSettings(result);
            }

            // Route localization methods
            else if (lowerMethod == "localization.language")
            {
                return GetPresentationLanguage(result);
            }
            else if (lowerMethod == "localization.locale")
            {
                return GetLocale(result);
            }
            else if (lowerMethod == "localization.setlocale")
            {
                JsonObject params;
                if (params.FromString(payload))
                {
                    string locale = params.Get("value").String();
                    return ResponseUtils::SetNullResponseForSuccess(SetLocale(locale), result);
                }
                result = "{\"error\":\"Invalid payload\"}";
                return Core::ERROR_BAD_REQUEST;
            }
            else if (lowerMethod == "localization.preferredaudiolanguages")
            {
                return GetPreferredAudioLanguages(result);
            }
            else if (lowerMethod == "localization.setpreferredaudiolanguages")
            {
                JsonObject params;
                if (params.FromString(payload))
                {
                    string languages = params.Get("value").String();
                    return ResponseUtils::SetNullResponseForSuccess(SetPreferredAudioLanguages(languages), result);
                }
                result = "{\"error\":\"Invalid payload\"}";
                return Core::ERROR_BAD_REQUEST;
            }
            else if (lowerMethod == "device.version")
            {
                return GetFirmwareVersion(result);
            }
            else if (lowerMethod == "device.screenresolution")
            {
                return GetScreenResolution(result);
            }
            else if (lowerMethod == "device.videoresolution")
            {
                return GetVideoResolution(result);
            }            
            else if (lowerMethod == "device.hdcp")
            {
                return GetHdcp(result);
            }
            else if (lowerMethod == "device.hdr")
            {
                return GetHdr(result);
            }
            else if (lowerMethod == "device.audio")
            {
                return GetAudio(result);
            }

            // If method not found, return error
            ErrorUtils::NotSupported(result);
            LOGERR("Unsupported method: %s", method.c_str());
            return Core::ERROR_UNKNOWN_KEY;
        }
    
    Core::hresult FbSettings::SetName(const string &value /* @in */, string &result)
        {
            result = "null"; // TBA
            return Core::ERROR_NONE;
        }

        Core::hresult FbSettings::AddAdditionalInfo(const string &value /* @in @opaque */, string &result)
        {
            result = "null"; // TBA
            return Core::ERROR_NONE;
        }
        // Delegated alias methods

        Core::hresult FbSettings::GetDeviceMake(string &make)
        {
            if (!mDelegate)
                return Core::ERROR_UNAVAILABLE;
            auto systemDelegate = mDelegate->getSystemDelegate();
            if (!systemDelegate)
                return Core::ERROR_UNAVAILABLE;
            return systemDelegate->GetDeviceMake(make);
        }

        Core::hresult FbSettings::GetDeviceName(string &name)
        {
            if (!mDelegate)
                return Core::ERROR_UNAVAILABLE;
            auto systemDelegate = mDelegate->getSystemDelegate();
            if (!systemDelegate)
                return Core::ERROR_UNAVAILABLE;
            return systemDelegate->GetDeviceName(name);
        }

        Core::hresult FbSettings::SetDeviceName(const string name)
        {
            if (!mDelegate)
                return Core::ERROR_UNAVAILABLE;
            auto systemDelegate = mDelegate->getSystemDelegate();
            if (!systemDelegate)
                return Core::ERROR_UNAVAILABLE;
            return systemDelegate->SetDeviceName(name);
        }

        Core::hresult FbSettings::GetDeviceSku(string &sku)
        {
            if (!mDelegate)
                return Core::ERROR_UNAVAILABLE;
            auto systemDelegate = mDelegate->getSystemDelegate();
            if (!systemDelegate)
                return Core::ERROR_UNAVAILABLE;
            return systemDelegate->GetDeviceSku(sku);
        }

        Core::hresult FbSettings::GetCountryCode(string &countryCode)
        {
            if (!mDelegate)
                return Core::ERROR_UNAVAILABLE;
            auto systemDelegate = mDelegate->getSystemDelegate();
            if (!systemDelegate)
                return Core::ERROR_UNAVAILABLE;
            return systemDelegate->GetCountryCode(countryCode);
        }

        Core::hresult FbSettings::SetCountryCode(const string countryCode)
        {
            if (!mDelegate)
                return Core::ERROR_UNAVAILABLE;
            auto systemDelegate = mDelegate->getSystemDelegate();
            if (!systemDelegate)
                return Core::ERROR_UNAVAILABLE;
            return systemDelegate->SetCountryCode(countryCode);
        }

        Core::hresult FbSettings::GetTimeZone(string &timeZone)
        {
            if (!mDelegate)
                return Core::ERROR_UNAVAILABLE;
            auto systemDelegate = mDelegate->getSystemDelegate();
            if (!systemDelegate)
                return Core::ERROR_UNAVAILABLE;
            return systemDelegate->GetTimeZone(timeZone);
        }

        Core::hresult FbSettings::SetTimeZone(const string timeZone)
        {
            if (!mDelegate)
                return Core::ERROR_UNAVAILABLE;
            auto systemDelegate = mDelegate->getSystemDelegate();
            if (!systemDelegate)
                return Core::ERROR_UNAVAILABLE;
            return systemDelegate->SetTimeZone(timeZone);
        }

        Core::hresult FbSettings::GetSecondScreenFriendlyName(string &name)
        {
            if (!mDelegate)
                return Core::ERROR_UNAVAILABLE;
            auto systemDelegate = mDelegate->getSystemDelegate();
            if (!systemDelegate)
                return Core::ERROR_UNAVAILABLE;
            return systemDelegate->GetSecondScreenFriendlyName(name);
        }

        // UserSettings APIs
        Core::hresult FbSettings::GetVoiceGuidance(string &result)
        {
            if (!mDelegate)
            {
                result = "{\"error\":\"couldnt get voiceguidance state\"}";
                return Core::ERROR_UNAVAILABLE;
            }

            auto userSettingsDelegate = mDelegate->getUserSettings();
            if (!userSettingsDelegate)
            {
                result = "{\"error\":\"couldnt get voiceguidance state\"}";
                return Core::ERROR_UNAVAILABLE;
            }

            return userSettingsDelegate->GetVoiceGuidance(result);
        }

        Core::hresult FbSettings::GetAudioDescription(string &result)
        {
            if (!mDelegate)
            {
                result = "{\"error\":\"couldnt get audio description settings\"}";
                return Core::ERROR_UNAVAILABLE;
            }

            auto userSettingsDelegate = mDelegate->getUserSettings();
            if (!userSettingsDelegate)
            {
                result = "{\"error\":\"couldnt get audio description settings\"}";
                return Core::ERROR_UNAVAILABLE;
            }

            return userSettingsDelegate->GetAudioDescription(result);
        }

        Core::hresult FbSettings::GetAudioDescriptionsEnabled(string &result)
        {
            if (!mDelegate)
            {
                result = "{\"error\":\"couldnt get audio descriptions enabled\"}";
                return Core::ERROR_UNAVAILABLE;
            }

            auto userSettingsDelegate = mDelegate->getUserSettings();
            if (!userSettingsDelegate)
            {
                result = "{\"error\":\"couldnt get audio descriptions enabled\"}";
                return Core::ERROR_UNAVAILABLE;
            }

            return userSettingsDelegate->GetAudioDescriptionsEnabled(result);
        }

        Core::hresult FbSettings::GetHighContrast(string &result)
        {
            if (!mDelegate)
            {
                result = "{\"error\":\"couldnt get high contrast state\"}";
                return Core::ERROR_UNAVAILABLE;
            }

            auto userSettingsDelegate = mDelegate->getUserSettings();
            if (!userSettingsDelegate)
            {
                result = "{\"error\":\"couldnt get high contrast state\"}";
                return Core::ERROR_UNAVAILABLE;
            }

            return userSettingsDelegate->GetHighContrast(result);
        }

        Core::hresult FbSettings::GetCaptions(string &result)
        {
            if (!mDelegate)
            {
                result = "{\"error\":\"couldnt get captions state\"}";
                return Core::ERROR_UNAVAILABLE;
            }

            auto userSettingsDelegate = mDelegate->getUserSettings();
            if (!userSettingsDelegate)
            {
                result = "{\"error\":\"couldnt get captions state\"}";
                return Core::ERROR_UNAVAILABLE;
            }

            return userSettingsDelegate->GetCaptions(result);
        }

        Core::hresult FbSettings::GetPresentationLanguage(string &result)
        {
            if (!mDelegate)
            {
                result = "{\"error\":\"couldn't get language\"}";
                return Core::ERROR_UNAVAILABLE;
            }

            auto userSettingsDelegate = mDelegate->getUserSettings();
            if (!userSettingsDelegate)
            {
                result = "{\"error\":\"couldn't get language\"}";
                return Core::ERROR_UNAVAILABLE;
            }

            return userSettingsDelegate->GetPresentationLanguage(result);
        }

        Core::hresult FbSettings::GetLocale(string &result)
        {
            if (!mDelegate)
            {
                result = "{\"error\":\"couldn't get locale\"}";
                return Core::ERROR_UNAVAILABLE;
            }

            auto userSettingsDelegate = mDelegate->getUserSettings();
            if (!userSettingsDelegate)
            {
                result = "{\"error\":\"couldn't get locale\"}";
                return Core::ERROR_UNAVAILABLE;
            }

            return userSettingsDelegate->GetLocale(result);
        }

        Core::hresult FbSettings::SetLocale(const string &locale)
        {
            if (!mDelegate)
            {
                return Core::ERROR_UNAVAILABLE;
            }

            auto userSettingsDelegate = mDelegate->getUserSettings();
            if (!userSettingsDelegate)
            {
                return Core::ERROR_UNAVAILABLE;
            }

            return userSettingsDelegate->SetLocale(locale);
        }

        Core::hresult FbSettings::GetPreferredAudioLanguages(string &result)
        {
            if (!mDelegate)
            {
                result = "[]";
                return Core::ERROR_UNAVAILABLE;
            }

            auto userSettingsDelegate = mDelegate->getUserSettings();
            if (!userSettingsDelegate)
            {
                result = "[]";
                return Core::ERROR_UNAVAILABLE;
            }

            return userSettingsDelegate->GetPreferredAudioLanguages(result);
        }

        Core::hresult FbSettings::GetPreferredCaptionsLanguages(string &result)
        {
            if (!mDelegate)
            {
                result = "[\"eng\"]";
                return Core::ERROR_UNAVAILABLE;
            }

            auto userSettingsDelegate = mDelegate->getUserSettings();
            if (!userSettingsDelegate)
            {
                result = "[\"eng\"]";
                return Core::ERROR_UNAVAILABLE;
            }

            return userSettingsDelegate->GetPreferredCaptionsLanguages(result);
        }

        Core::hresult FbSettings::SetPreferredAudioLanguages(const string &languages)
        {
            if (!mDelegate)
            {
                return Core::ERROR_UNAVAILABLE;
            }

            auto userSettingsDelegate = mDelegate->getUserSettings();
            if (!userSettingsDelegate)
            {
                return Core::ERROR_UNAVAILABLE;
            }

            return userSettingsDelegate->SetPreferredAudioLanguages(languages);
        }

        Core::hresult FbSettings::SetPreferredCaptionsLanguages(const string &preferredLanguages)
        {
            if (!mDelegate)
            {
                return Core::ERROR_UNAVAILABLE;
            }

            auto userSettingsDelegate = mDelegate->getUserSettings();
            if (!userSettingsDelegate)
            {
                return Core::ERROR_UNAVAILABLE;
            }

            return userSettingsDelegate->SetPreferredCaptionsLanguages(preferredLanguages);
        }

        Core::hresult FbSettings::SetVoiceGuidance(const bool enabled)
        {
            if (!mDelegate)
            {
                return Core::ERROR_UNAVAILABLE;
            }

            auto userSettingsDelegate = mDelegate->getUserSettings();
            if (!userSettingsDelegate)
            {
                return Core::ERROR_UNAVAILABLE;
            }

            return userSettingsDelegate->SetVoiceGuidance(enabled);
        }

        Core::hresult FbSettings::SetAudioDescriptionsEnabled(const bool enabled)
        {
            if (!mDelegate)
            {
                return Core::ERROR_UNAVAILABLE;
            }

            auto userSettingsDelegate = mDelegate->getUserSettings();
            if (!userSettingsDelegate)
            {
                return Core::ERROR_UNAVAILABLE;
            }

            return userSettingsDelegate->SetAudioDescriptionsEnabled(enabled);
        }

        Core::hresult FbSettings::SetCaptions(const bool enabled)
        {
            if (!mDelegate)
            {
                return Core::ERROR_UNAVAILABLE;
            }

            auto userSettingsDelegate = mDelegate->getUserSettings();
            if (!userSettingsDelegate)
            {
                return Core::ERROR_UNAVAILABLE;
            }

            return userSettingsDelegate->SetCaptions(enabled);
        }

        Core::hresult FbSettings::SetSpeed(const double speed)
        {
            if (!mDelegate)
            {
                return Core::ERROR_UNAVAILABLE;
            }

            auto userSettingsDelegate = mDelegate->getUserSettings();
            if (!userSettingsDelegate)
            {
                return Core::ERROR_UNAVAILABLE;
            }

            // Transform the speed using vg_speed_firebolt2thunder function logic:
            // (if $speed == 2 then 10 elif $speed >= 1.67 then 1.38 elif $speed >= 1.33 then 1.19 elif $speed >= 1 then 1 else 0.1 end)
            double transformedRate;
            if (speed == 2.0)
            {
                transformedRate = 10.0;
            }
            else if (speed >= 1.67)
            {
                transformedRate = 1.38;
            }
            else if (speed >= 1.33)
            {
                transformedRate = 1.19;
            }
            else if (speed >= 1.0)
            {
                transformedRate = 1.0;
            }
            else
            {
                transformedRate = 0.1;
            }

            LOGINFO("SetSpeed: transforming speed %f to rate %f", speed, transformedRate);

            return userSettingsDelegate->SetVoiceGuidanceRate(transformedRate);
        }

        Core::hresult FbSettings::GetSpeed(double &speed)
        {
            if (!mDelegate)
            {
                return Core::ERROR_UNAVAILABLE;
            }

            auto userSettingsDelegate = mDelegate->getUserSettings();
            if (!userSettingsDelegate)
            {
                return Core::ERROR_UNAVAILABLE;
            }

            double rate;
            Core::hresult result = userSettingsDelegate->GetVoiceGuidanceRate(rate);

            if (result != Core::ERROR_NONE)
            {
                LOGERR("Failed to get voice guidance rate");
                return result;
            }

            // Transform the rate using vg_speed_thunder2firebolt function logic:
            // (if $speed >= 1.56 then 2 elif $speed >= 1.38 then 1.67 elif $speed >= 1.19 then 1.33 elif $speed >= 1 then 1 else 0.5 end)
            if (rate >= 1.56)
            {
                speed = 2.0;
            }
            else if (rate >= 1.38)
            {
                speed = 1.67;
            }
            else if (rate >= 1.19)
            {
                speed = 1.33;
            }
            else if (rate >= 1.0)
            {
                speed = 1.0;
            }
            else
            {
                speed = 0.5;
            }

            LOGINFO("GetSpeed: transforming rate %f to speed %f", rate, speed);

            return Core::ERROR_NONE;
        }

        Core::hresult FbSettings::GetVoiceGuidanceHints(string &result)
        {
            if (!mDelegate)
            {
                result = "{\"error\":\"couldnt get navigationHints\"}";
                return Core::ERROR_UNAVAILABLE;
            }

            auto userSettingsDelegate = mDelegate->getUserSettings();
            if (!userSettingsDelegate)
            {
                result = "{\"error\":\"couldnt get navigationHints\"}";
                return Core::ERROR_UNAVAILABLE;
            }

            return userSettingsDelegate->GetVoiceGuidanceHints(result);
        }

        Core::hresult FbSettings::SetVoiceGuidanceHints(const bool enabled)
        {
            if (!mDelegate)
            {
                return Core::ERROR_UNAVAILABLE;
            }

            auto userSettingsDelegate = mDelegate->getUserSettings();
            if (!userSettingsDelegate)
            {
                return Core::ERROR_UNAVAILABLE;
            }

            return userSettingsDelegate->SetVoiceGuidanceHints(enabled);
        }

        Core::hresult FbSettings::GetVoiceGuidanceSettings(string &result)
        {
            if (!mDelegate)
            {
                result = "{\"error\":\"couldn't get voice guidance settings\"}";
                return Core::ERROR_UNAVAILABLE;
            }

            auto userSettingsDelegate = mDelegate->getUserSettings();
            if (!userSettingsDelegate)
            {
                result = "{\"error\":\"couldn't get voice guidance settings\"}";
                return Core::ERROR_UNAVAILABLE;
            }

            // Get voice guidance enabled state
            string enabledResult;
            Core::hresult enabledStatus = userSettingsDelegate->GetVoiceGuidance(enabledResult);
            if (enabledStatus != Core::ERROR_NONE)
            {
                result = "{\"error\":\"couldn't get voiceguidance enabled state\"}";
                return enabledStatus;
            }

            // Get voice guidance rate (speed)
            double rate;
            Core::hresult rateStatus = userSettingsDelegate->GetVoiceGuidanceRate(rate);
            if (rateStatus != Core::ERROR_NONE)
            {
                result = "{\"error\":\"couldn't get voiceguidance rate\"}";
                return rateStatus;
            }

            // Get navigation hints
            string hintsResult;
            Core::hresult hintsStatus = userSettingsDelegate->GetVoiceGuidanceHints(hintsResult);
            if (hintsStatus != Core::ERROR_NONE)
            {
                result = "{\"error\":\"couldn't get voiceguidance hints\"}";
                return hintsStatus;
            }

            // Construct the combined JSON response
            // Format: {"enabled": <bool>, "speed": <rate>, "rate": <rate>, "navigationHints": <bool>}
            std::ostringstream jsonStream;
            jsonStream << "{\"enabled\": " << enabledResult
                       << ", \"speed\": " << rate
                       << ", \"rate\": " << rate
                       << ", \"navigationHints\": " << hintsResult << "}";

            result = jsonStream.str();

            return Core::ERROR_NONE;
        }

        Core::hresult FbSettings::GetClosedCaptionsSettings(string &result)
        {
            if (!mDelegate)
            {
                result = "{\"error\":\"couldn't get closed captions settings\"}";
                return Core::ERROR_UNAVAILABLE;
            }

            auto userSettingsDelegate = mDelegate->getUserSettings();
            if (!userSettingsDelegate)
            {
                result = "{\"error\":\"couldn't get closed captions settings\"}";
                return Core::ERROR_UNAVAILABLE;
            }

            // Get closed captions enabled state
            string enabledResult;
            Core::hresult enabledStatus = userSettingsDelegate->GetCaptions(enabledResult);
            if (enabledStatus != Core::ERROR_NONE)
            {
                result = "{\"error\":\"couldn't get closed captions enabled state\"}";
                return enabledStatus;
            }

            // Get preferred captions languages
            string languagesResult;
            Core::hresult languagesStatus = userSettingsDelegate->GetPreferredCaptionsLanguages(languagesResult);
            if (languagesStatus != Core::ERROR_NONE)
            {
                result = "{\"error\":\"couldn't get preferred captions languages\"}";
                return languagesStatus;
            }

            // Construct the combined JSON response
            // Format: {"enabled": <bool>, "preferredLanguages": <array>, "styles": {}}
            std::ostringstream jsonStream;
            jsonStream << "{\"enabled\": " << enabledResult
                       << ", \"preferredLanguages\": " << languagesResult
                       << ", \"styles\": {}}";

            result = jsonStream.str();

            return Core::ERROR_NONE;
        }

        Core::hresult FbSettings::GetInternetConnectionStatus(string &result)
        {
            if (!mDelegate)
            {
                result = "{\"error\":\"couldn't get internet connection status\"}";
                return Core::ERROR_UNAVAILABLE;
            }

            auto networkDelegate = mDelegate->getNetworkDelegate();
            if (!networkDelegate)
            {
                result = "{\"error\":\"couldn't get internet connection status\"}";
                return Core::ERROR_UNAVAILABLE;
            }

            return networkDelegate->GetInternetConnectionStatus(result);
        }

        Core::hresult FbSettings::GetFirmwareVersion(string &result /* @out */)
        {
            if (!mDelegate)
                return Core::ERROR_UNAVAILABLE;
            auto systemDelegate = mDelegate->getSystemDelegate();
            if (!systemDelegate)
                return Core::ERROR_UNAVAILABLE;
            return systemDelegate->GetFirmwareVersion(result);
        }

        // Core::hresult FbSettings::GetScreenResolution(string &result /* out */) {
        //     result = R"([1920,1080])";
        //     return Core::ERROR_NONE;
        // }

        // Core::hresult FbSettings::GetVideoResolution(string &result /* out */) {
        //     result = R"([1920,1080])";
        //     return Core::ERROR_NONE;
        // }

        Core::hresult FbSettings::GetScreenResolution(string &result)
        {
            LOGINFO("GetScreenResolution FbSettings");
            if (!mDelegate) {
                result = "[1920,1080]";
                return Core::ERROR_UNAVAILABLE;
            }
            auto systemDelegate = mDelegate->getSystemDelegate();
            if (!systemDelegate) {
                result = "[1920,1080]";
                return Core::ERROR_UNAVAILABLE;
            }
            return systemDelegate->GetScreenResolution(result);
        }

        Core::hresult FbSettings::GetVideoResolution(string &result)
        {
            LOGINFO("GetVideoResolution FbSettings");
            if (!mDelegate) {
                result = "[1920,1080]";
                return Core::ERROR_UNAVAILABLE;
            }
            auto systemDelegate = mDelegate->getSystemDelegate();
            if (!systemDelegate) {
                result = "[1920,1080]";
                return Core::ERROR_UNAVAILABLE;
            }
            return systemDelegate->GetVideoResolution(result);
        }

        Core::hresult FbSettings::GetHdcp(string &result)
        {
            LOGINFO("GetHdcp FbSettings");
            if (!mDelegate) {
                result = "{\"hdcp1.4\":false,\"hdcp2.2\":false}";
                return Core::ERROR_UNAVAILABLE;
            }
            auto systemDelegate = mDelegate->getSystemDelegate();
            if (!systemDelegate) {
                result = "{\"hdcp1.4\":false,\"hdcp2.2\":false}";
                return Core::ERROR_UNAVAILABLE;
            }
            return systemDelegate->GetHdcp(result);
        }

        Core::hresult FbSettings::GetHdr(string &result)
        {
            LOGINFO("GetHdr FbSettings");
            if (!mDelegate) {
                result = "{\"hdr10\":false,\"dolbyVision\":false,\"hlg\":false,\"hdr10Plus\":false}";
                return Core::ERROR_UNAVAILABLE;
            }
            auto systemDelegate = mDelegate->getSystemDelegate();
            if (!systemDelegate) {
                result = "{\"hdr10\":false,\"dolbyVision\":false,\"hlg\":false,\"hdr10Plus\":false}";
                return Core::ERROR_UNAVAILABLE;
            }
            return systemDelegate->GetHdr(result);
        }

        Core::hresult FbSettings::GetAudio(string &result)
        {
            LOGINFO("GetAudio FbSettings");
            if (!mDelegate) {
                result = "{\"stereo\":true,\"dolbyDigital5.1\":false,\"dolbyDigital5.1+\":false,\"dolbyAtmos\":false}";
                return Core::ERROR_UNAVAILABLE;
            }
            auto systemDelegate = mDelegate->getSystemDelegate();
            if (!systemDelegate) {
                result = "{\"stereo\":true,\"dolbyDigital5.1\":false,\"dolbyDigital5.1+\":false,\"dolbyAtmos\":false}";
                return Core::ERROR_UNAVAILABLE;
            }
            return systemDelegate->GetAudio(result);
        }


} // namespace Plugin
} // namespace WPEFramework

