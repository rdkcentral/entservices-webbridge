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
#ifndef __USERSETTINGSDELEGATE_H__
#define __USERSETTINGSDELEGATE_H__
#include "StringUtils.h"
#include "BaseEventDelegate.h"
#include <interfaces/IUserSettings.h>
#include "UtilsLogging.h"
#include "ObjectUtils.h"
#include <set>
using namespace WPEFramework;
#define USERSETTINGS_CALLSIGN "org.rdk.UserSettings"

static const std::set<string> VALID_USER_SETTINGS_EVENT = {
    "localization.onlanguagechanged",
    "localization.onlocalechanged",
    "localization.onpreferredaudiolanguageschanged",
    "accessibility.onaudiodescriptionsettingschanged",
    "accessibility.onhighcontrastuichanged",
    "closedcaptions.onenabledchanged",
    "closedcaptions.onpreferredlanguageschanged",
    "accessibility.onclosedcaptionssettingschanged",
    "accessibility.onvoiceguidancesettingschanged",
};

class UserSettingsDelegate : public BaseEventDelegate{
    public:
        UserSettingsDelegate(PluginHost::IShell* shell):
            BaseEventDelegate(), mUserSettings(nullptr), mShell(shell), mNotificationHandler(*this) {}

        ~UserSettingsDelegate() {
            if (mUserSettings != nullptr) {
                mUserSettings->Release();
                mUserSettings = nullptr;
            }
        }

        bool HandleSubscription(Exchange::IAppNotificationHandler::IEmitter *cb, const string &event, const bool listen) {
            if (listen) {
                Exchange::IUserSettings* userSettings = GetUserSettingsInterface();
                if (userSettings == nullptr) {
                    LOGERR("UserSettings interface not available");
                    return false;
                }
	
                AddNotification(event, cb);

                if (!mNotificationHandler.GetRegistered()) {
                    LOGINFO("Registering for UserSettings notifications");
                    mUserSettings->Register(&mNotificationHandler);
                    mNotificationHandler.SetRegistered(true);
                    return true;
                } else {
                    LOGTRACE(" Is UserSettings registered = %s", mNotificationHandler.GetRegistered() ? "true" : "false");
                }
                
            } else {
                // Not removing the notification subscription for cases where only one event is removed 
                // Registration is lazy one but we need to evaluate if there is any value in unregistering
                // given these API calls are always made
                RemoveNotification(event, cb);
            }
            return false;
        }

        bool HandleEvent(Exchange::IAppNotificationHandler::IEmitter *cb, const string &event, const bool listen, bool &registrationError) {
            LOGDBG("Checking for handle event");
            // Check if event is present in VALID_USER_SETTINGS_EVENT make check case insensitive
            if (VALID_USER_SETTINGS_EVENT.find(StringUtils::toLower(event)) != VALID_USER_SETTINGS_EVENT.end()) {
                // Handle TextToSpeech event
                registrationError = HandleSubscription(cb, event, listen);
                return true;
            }
            return false;
        }

	 // Common method to ensure mUserSettings is available for all APIs and notifications
        Exchange::IUserSettings* GetUserSettingsInterface() {
            if (mUserSettings == nullptr && mShell != nullptr) {
                mUserSettings = mShell->QueryInterfaceByCallsign<Exchange::IUserSettings>(USERSETTINGS_CALLSIGN);
                if (mUserSettings == nullptr) {
                    LOGERR("Failed to get UserSettings COM interface");
                }
            }
            return mUserSettings;
        }

        Core::hresult GetVoiceGuidance(string& result) {
            LOGINFO("GetVoiceGuidance from UserSettings COM interface");
            result.clear();

            Exchange::IUserSettings* userSettings = GetUserSettingsInterface();
            if (userSettings == nullptr) {
                LOGERR("UserSettings COM interface not available");
                result = "{\"error\":\"couldnt get voiceguidance state\"}";
                return Core::ERROR_UNAVAILABLE;
            }

            bool enabled = false;
            Core::hresult rc = userSettings->GetVoiceGuidance(enabled);

            if (rc == Core::ERROR_NONE) {
                // Transform the response: return_or_error(.result, "couldnt get voiceguidance state")
                // Return the boolean result directly as per transform specification
                result = enabled ? "true" : "false";
                return Core::ERROR_NONE;
            } else {
                LOGERR("Failed to call GetVoiceGuidance on UserSettings COM interface, error: %u", rc);
                result = "{\"error\":\"couldnt get voiceguidance state\"}";
                return Core::ERROR_GENERAL;
            }
        }

        Core::hresult GetAudioDescription(string& result) {
            LOGINFO("GetAudioDescription from UserSettings COM interface");
            result.clear();

            Exchange::IUserSettings* userSettings = GetUserSettingsInterface();
            if (userSettings == nullptr) {
                LOGERR("UserSettings COM interface not available");
                result = "{\"error\":\"couldnt get audio description settings\"}";
                return Core::ERROR_UNAVAILABLE;
            }

            bool enabled = false;
            Core::hresult rc = userSettings->GetAudioDescription(enabled);

            if (rc == Core::ERROR_NONE) {
                // Transform the response: return_or_error({ enabled: .result }, "couldnt get audio description settings")
                // Create JSON response with enabled state
                result = ObjectUtils::CreateBooleanJsonString("enabled", enabled);
                return Core::ERROR_NONE;
            } else {
                LOGERR("Failed to call GetAudioDescription on UserSettings COM interface, error: %u", rc);
                result = "{\"error\":\"couldnt get audio description settings\"}";
                return Core::ERROR_GENERAL;
            }
        }

        Core::hresult GetAudioDescriptionsEnabled(string& result) {
            LOGINFO("GetAudioDescriptionsEnabled from UserSettings COM interface");
            result.clear();

            Exchange::IUserSettings* userSettings = GetUserSettingsInterface();
            if (userSettings == nullptr) {
                LOGERR("UserSettings COM interface not available");
                result = "{\"error\":\"couldnt get audio descriptions enabled\"}";
                return Core::ERROR_UNAVAILABLE;
            }

            bool enabled = false;
            Core::hresult rc = userSettings->GetAudioDescription(enabled);

            if (rc == Core::ERROR_NONE) {
                // Transform the response: return_or_error(.result, "couldnt get audio descriptions enabled")
                // Return the boolean result directly as per transform specification
                result = enabled ? "true" : "false";
                return Core::ERROR_NONE;
            } else {
                LOGERR("Failed to call GetAudioDescription on UserSettings COM interface, error: %u", rc);
                result = "{\"error\":\"couldnt get audio descriptions enabled\"}";
                return Core::ERROR_GENERAL;
            }
        }

        Core::hresult GetHighContrast(string& result) {
            LOGINFO("GetHighContrast from UserSettings COM interface");
            result.clear();

            Exchange::IUserSettings* userSettings = GetUserSettingsInterface();
            if (userSettings == nullptr) {
                LOGERR("UserSettings COM interface not available");
                result = "{\"error\":\"couldnt get high contrast state\"}";
                return Core::ERROR_UNAVAILABLE;
            }

            bool enabled = false;
            Core::hresult rc = userSettings->GetHighContrast(enabled);

            if (rc == Core::ERROR_NONE) {
                 // Transform the response: return_or_error(.result, "couldnt get audio descriptions enabled")
                // Return the boolean result directly as per transform specification
                result = enabled ? "true" : "false";
                return Core::ERROR_NONE;
            } else {
                LOGERR("Failed to call GetHighContrast on UserSettings COM interface, error: %u", rc);
                result = "{\"error\":\"couldnt get high contrast state\"}";
                return Core::ERROR_GENERAL;
            }
        }

        Core::hresult GetCaptions(string& result) {
            LOGINFO("GetCaptions from UserSettings COM interface");
            result.clear();

            Exchange::IUserSettings* userSettings = GetUserSettingsInterface();
            if (userSettings == nullptr) {
                LOGERR("UserSettings COM interface not available");
                result = "{\"error\":\"couldnt get captions state\"}";
                return Core::ERROR_UNAVAILABLE;
            }

            bool enabled = false;
            Core::hresult rc = userSettings->GetCaptions(enabled);

            if (rc == Core::ERROR_NONE) {
                // Transform the response: return_or_error(.result, "couldnt get captions state")
                // Return the boolean result directly as per transform specification
                result = enabled ? "true" : "false";
                return Core::ERROR_NONE;
            } else {
                LOGERR("Failed to call GetCaptions on UserSettings COM interface, error: %u", rc);
                result = "{\"error\":\"couldnt get captions state\"}";
                return Core::ERROR_GENERAL;
            }
        }

        Core::hresult SetVoiceGuidance(const bool enabled) {
            LOGINFO("SetVoiceGuidance to UserSettings COM interface: %s", enabled ? "true" : "false");

            Exchange::IUserSettings* userSettings = GetUserSettingsInterface();
            if (userSettings == nullptr) {
                LOGERR("UserSettings COM interface not available");
                return Core::ERROR_UNAVAILABLE;
            }

            // Transform request: { enabled: .value }
            // The enabled parameter is the .value from the request
            Core::hresult rc = userSettings->SetVoiceGuidance(enabled);

            if (rc == Core::ERROR_NONE) {
                // Transform response: return_or_error(null, "couldn't set voiceguidance enabled")
                // Success case - return null (no error)
                return Core::ERROR_NONE;
            } else {
                LOGERR("Failed to call SetVoiceGuidance on UserSettings COM interface, error: %u", rc);
                // Error case - return error
                return Core::ERROR_GENERAL;
            }
        }

        Core::hresult SetAudioDescriptionsEnabled(const bool enabled) {
            LOGINFO("SetAudioDescriptionsEnabled to UserSettings COM interface: %s", enabled ? "true" : "false");

            Exchange::IUserSettings* userSettings = GetUserSettingsInterface();
            if (userSettings == nullptr) {
                LOGERR("UserSettings COM interface not available");
                return Core::ERROR_UNAVAILABLE;
            }

            // Transform request: { enabled: .value }
            // The enabled parameter is the .value from the request
            Core::hresult rc = userSettings->SetAudioDescription(enabled);

            if (rc == Core::ERROR_NONE) {
                // Transform response: return_or_error(null, "couldn't set audio descriptions enabled")
                // Success case - return null (no error)
                return Core::ERROR_NONE;
            } else {
                LOGERR("Failed to call SetAudioDescription on UserSettings COM interface, error: %u", rc);
                // Error case - return error
                return Core::ERROR_GENERAL;
            }
        }

        Core::hresult SetCaptions(const bool enabled) {
            LOGINFO("SetCaptions to UserSettings COM interface: %s", enabled ? "true" : "false");

            Exchange::IUserSettings* userSettings = GetUserSettingsInterface();
            if (userSettings == nullptr) {
                LOGERR("UserSettings COM interface not available");
                return Core::ERROR_UNAVAILABLE;
            }

            // Transform request: { enabled: .value }
            // The enabled parameter is the .value from the request
            Core::hresult rc = userSettings->SetCaptions(enabled);

            if (rc == Core::ERROR_NONE) {
                // Transform response: return_or_error(null, "couldn't set captions enabled")
                // Success case - return null (no error)
                return Core::ERROR_NONE;
            } else {
                LOGERR("Failed to call SetCaptions on UserSettings COM interface, error: %u", rc);
                // Error case - return error
                return Core::ERROR_GENERAL;
            }
        }

        Core::hresult SetVoiceGuidanceRate(const double rate) {
            LOGINFO("SetVoiceGuidanceRate to UserSettings COM interface: %f", rate);

            Exchange::IUserSettings* userSettings = GetUserSettingsInterface();
            if (userSettings == nullptr) {
                LOGERR("UserSettings COM interface not available");
                return Core::ERROR_UNAVAILABLE;
            }

            // Transform request: { rate: transformed_rate }
            // The rate parameter is already the transformed value from vg_speed_firebolt2thunder
            Core::hresult rc = userSettings->SetVoiceGuidanceRate(rate);

            if (rc == Core::ERROR_NONE) {
                // Transform response: return_or_error(null, "couldnt set speed")
                // Success case - return null (no error)
                return Core::ERROR_NONE;
            } else {
                LOGERR("Failed to call SetVoiceGuidanceRate on UserSettings COM interface, error: %u", rc);
                // Error case - return error
                return Core::ERROR_GENERAL;
            }
        }

        Core::hresult SetVoiceGuidanceHints(const bool enabled) {
            LOGINFO("SetVoiceGuidanceHints to UserSettings COM interface: %s", enabled ? "true" : "false");

            Exchange::IUserSettings* userSettings = GetUserSettingsInterface();
            if (userSettings == nullptr) {
                LOGERR("UserSettings COM interface not available");
                return Core::ERROR_UNAVAILABLE;
            }

            // Transform request: { enabled: .value }
            // The enabled parameter is the .value from the request
            Core::hresult rc = userSettings->SetVoiceGuidanceHints(enabled);

            if (rc == Core::ERROR_NONE) {
                // Transform response: return_or_error(null, "couldn't set voice guidance hints")
                // Success case - return null (no error)
                return Core::ERROR_NONE;
            } else {
                LOGERR("Failed to call SetVoiceGuidanceHints on UserSettings COM interface, error: %u", rc);
                // Error case - return error
                return Core::ERROR_GENERAL;
            }
        }

        Core::hresult GetVoiceGuidanceRate(double& rate) {
            LOGINFO("GetVoiceGuidanceRate from UserSettings COM interface");

            Exchange::IUserSettings* userSettings = GetUserSettingsInterface();
            if (userSettings == nullptr) {
                LOGERR("UserSettings COM interface not available");
                return Core::ERROR_UNAVAILABLE;
            }

            Core::hresult rc = userSettings->GetVoiceGuidanceRate(rate);

            if (rc == Core::ERROR_NONE) {
                LOGINFO("Got voice guidance rate: %f", rate);
                return Core::ERROR_NONE;
            } else {
                LOGERR("Failed to call GetVoiceGuidanceRate on UserSettings COM interface, error: %u", rc);
                return Core::ERROR_GENERAL;
            }
        }

        Core::hresult GetVoiceGuidanceHints(string& result) {
            LOGINFO("GetVoiceGuidanceHints from UserSettings COM interface");
            result.clear();

            Exchange::IUserSettings* userSettings = GetUserSettingsInterface();
            if (userSettings == nullptr) {
                LOGERR("UserSettings COM interface not available");
                result = "{\"error\":\"couldnt get navigationHints\"}";
                return Core::ERROR_UNAVAILABLE;
            }

            bool hints = false;
            Core::hresult rc = userSettings->GetVoiceGuidanceHints(hints);

            if (rc == Core::ERROR_NONE) {
                // Transform: return_or_error(.result, "couldnt get navigationHints")
                // Return the boolean result directly as per transform specification
                result = hints ? "true" : "false";
                return Core::ERROR_NONE;
            } else {
                LOGERR("Failed to call GetVoiceGuidanceHints on UserSettings COM interface, error: %u", rc);
                result = "{\"error\":\"couldnt get navigationHints\"}";
                return Core::ERROR_GENERAL;
            }
        }

        Core::hresult GetPresentationLanguage(string& result) {
            LOGINFO("GetPresentationLanguage from UserSettings COM interface");
            result.clear();

            Exchange::IUserSettings* userSettings = GetUserSettingsInterface();
            if (userSettings == nullptr) {
                LOGERR("UserSettings COM interface not available");
                result = "{\"error\":\"couldn't get language\"}";
                return Core::ERROR_UNAVAILABLE;
            }

            string presentationLanguage;
            Core::hresult rc = userSettings->GetPresentationLanguage(presentationLanguage);

            if (rc == Core::ERROR_NONE) {
                // Transform: return_or_error(.result | split("-") | .[0], "couldn't get language")
                if (!presentationLanguage.empty()) {
                    // Extract language part (before "-") from locale like "en-US" -> "en"
                    size_t dashPos = presentationLanguage.find('-');
                    string language;
                    if (dashPos != string::npos) {
                        language = presentationLanguage.substr(0, dashPos);
                    } else {
                        // If no dash found, return the whole string
                        language = presentationLanguage;
                    }
                    // Wrap in quotes to make it a valid JSON string
                    result = "\"" + language + "\"";
                    return Core::ERROR_NONE;
                } else {
                    result = "{\"error\":\"couldn't get language\"}";
                    return Core::ERROR_GENERAL;
                }
            } else {
                LOGERR("Failed to call GetPresentationLanguage on UserSettings COM interface, error: %u", rc);
                result = "{\"error\":\"couldn't get language\"}";
                return Core::ERROR_GENERAL;
            }
        }

        Core::hresult GetLocale(string& result) {
            LOGINFO("GetLocale from UserSettings COM interface");
            result.clear();

            Exchange::IUserSettings* userSettings = GetUserSettingsInterface();
            if (userSettings == nullptr) {
                LOGERR("UserSettings COM interface not available");
                result = "{\"error\":\"couldn't get locale\"}";
                return Core::ERROR_UNAVAILABLE;
            }

            string presentationLanguage;
            Core::hresult rc = userSettings->GetPresentationLanguage(presentationLanguage);

            if (rc == Core::ERROR_NONE) {
                // Transform: return_or_error(.result, "couldn't get locale")
                // Return the full locale without any transformation
                if (!presentationLanguage.empty()) {
                    // Wrap in quotes to make it a valid JSON string
                    result = "\"" + presentationLanguage + "\"";
                    return Core::ERROR_NONE;
                } else {
                    result = "{\"error\":\"couldn't get locale\"}";
                    return Core::ERROR_GENERAL;
                }
            } else {
                LOGERR("Failed to call GetPresentationLanguage on UserSettings COM interface, error: %u", rc);
                result = "{\"error\":\"couldn't get locale\"}";
                return Core::ERROR_GENERAL;
            }
        }

        Core::hresult SetLocale(const string& locale) {
            LOGINFO("SetLocale to UserSettings COM interface: %s", locale.c_str());

            Exchange::IUserSettings* userSettings = GetUserSettingsInterface();
            if (userSettings == nullptr) {
                LOGERR("UserSettings COM interface not available");
                return Core::ERROR_UNAVAILABLE;
            }

            // Transform request: { presentationLanguage: .value }
            // The locale parameter is the .value from the request
            Core::hresult rc = userSettings->SetPresentationLanguage(locale);

            if (rc == Core::ERROR_NONE) {
                // Transform response: return_or_error(null, "couldn't set locale")
                // Success case - return null (no error)
                return Core::ERROR_NONE;
            } else {
                LOGERR("Failed to call SetPresentationLanguage on UserSettings COM interface, error: %u", rc);
                // Error case - return error
                return Core::ERROR_GENERAL;
            }
        }

        Core::hresult GetPreferredAudioLanguages(string& result) {
            LOGINFO("GetPreferredAudioLanguages from UserSettings COM interface");
            result.clear();

            Exchange::IUserSettings* userSettings = GetUserSettingsInterface();
            if (userSettings == nullptr) {
                LOGERR("UserSettings COM interface not available");
                result = "[]";  // Return empty array on error
                return Core::ERROR_UNAVAILABLE;
            }

            string preferredLanguages;
            Core::hresult rc = userSettings->GetPreferredAudioLanguages(preferredLanguages);

            if (rc == Core::ERROR_NONE) {
                // Transform: return_or_else(.result | split(","), [])
                if (!preferredLanguages.empty()) {
                    // Split comma-separated string into JSON array
                    // Example: "eng,fra,spa" -> ["eng","fra","spa"]
                    result = "[";
                    size_t pos = 0;
                    string token;
                    bool first = true;

                    while ((pos = preferredLanguages.find(',')) != string::npos) {
                        token = preferredLanguages.substr(0, pos);
                        // Trim whitespace
                        token.erase(0, token.find_first_not_of(" \t"));
                        token.erase(token.find_last_not_of(" \t") + 1);

                        if (!first) result += ",";
                        result += "\"" + token + "\"";
                        first = false;
                        preferredLanguages.erase(0, pos + 1);
                    }

                    // Handle the last token
                    if (!preferredLanguages.empty()) {
                        preferredLanguages.erase(0, preferredLanguages.find_first_not_of(" \t"));
                        preferredLanguages.erase(preferredLanguages.find_last_not_of(" \t") + 1);

                        if (!first) result += ",";
                        result += "\"" + preferredLanguages + "\"";
                    }

                    result += "]";
                } else {
                    // Empty string case - return empty array
                    result = "[]";
                }
                return Core::ERROR_NONE;
            } else {
                LOGERR("Failed to call GetPreferredAudioLanguages on UserSettings COM interface, error: %u", rc);
                result = "[]";  // Return empty array on error
                return Core::ERROR_GENERAL;
            }
        }

        Core::hresult GetPreferredCaptionsLanguages(string& result) {
            LOGINFO("GetPreferredCaptionsLanguages from UserSettings COM interface");
            result.clear();

            Exchange::IUserSettings* userSettings = GetUserSettingsInterface();
            if (userSettings == nullptr) {
                LOGERR("UserSettings COM interface not available");
                result = "[\"eng\"]";  // Return default ["eng"] on error
                return Core::ERROR_UNAVAILABLE;
            }

            string preferredLanguages;
            Core::hresult rc = userSettings->GetPreferredCaptionsLanguages(preferredLanguages);

            if (rc == Core::ERROR_NONE) {
                // Transform: if .result | length > 0 then .result | split(",") else ["eng"] end
                if (!preferredLanguages.empty()) {
                    // Split comma-separated string into JSON array
                    // Example: "eng,fra,spa" -> ["eng","fra","spa"]
                    result = "[";
                    size_t pos = 0;
                    string token;
                    bool first = true;

                    while ((pos = preferredLanguages.find(',')) != string::npos) {
                        token = preferredLanguages.substr(0, pos);
                        // Trim whitespace
                        token.erase(0, token.find_first_not_of(" \t"));
                        token.erase(token.find_last_not_of(" \t") + 1);

                        if (!first) result += ",";
                        result += "\"" + token + "\"";
                        first = false;
                        preferredLanguages.erase(0, pos + 1);
                    }

                    // Handle the last token
                    if (!preferredLanguages.empty()) {
                        preferredLanguages.erase(0, preferredLanguages.find_first_not_of(" \t"));
                        preferredLanguages.erase(preferredLanguages.find_last_not_of(" \t") + 1);

                        if (!first) result += ",";
                        result += "\"" + preferredLanguages + "\"";
                    }

                    result += "]";
                } else {
                    // Empty string case - return ["eng"] as default
                    result = "[\"eng\"]";
                }
                return Core::ERROR_NONE;
            } else {
                LOGERR("Failed to call GetPreferredCaptionsLanguages on UserSettings COM interface, error: %u", rc);
                result = "[\"eng\"]";  // Return default ["eng"] on error
                return Core::ERROR_GENERAL;
            }
        }

	Core::hresult SetPreferredAudioLanguages(const string& languages) {
            LOGINFO("SetPreferredAudioLanguages to UserSettings COM interface: %s", languages.c_str());

            Exchange::IUserSettings* userSettings = GetUserSettingsInterface();
            if (userSettings == nullptr) {
                LOGERR("UserSettings COM interface not available");
                return Core::ERROR_UNAVAILABLE;
            }

            // Transform request: { preferredLanguages: (.value | join(","))}
            // The languages parameter can be either:
            // 1. A JSON array: ["eng","fra","spa"] -> "eng,fra,spa"
            // 2. A single string: "tam" -> "tam"

            string commaSeparatedLanguages;

            // Check if input is a JSON array
            if (languages.length() >= 2 && languages[0] == '[' && languages.back() == ']') {
                // Handle JSON array format
                string arrayContent = languages.substr(1, languages.length() - 2); // Remove [ ]

                if (!arrayContent.empty()) {
                    // Use a cleaner parsing approach similar to GetPreferredAudioLanguages
                    string remaining = arrayContent;
                    bool first = true;

                    while (!remaining.empty()) {
                        // Skip whitespace and commas
                        size_t start = remaining.find_first_not_of(" \t,");
                        if (start == string::npos) break;

                        remaining = remaining.substr(start);

                        string token;
                        if (!remaining.empty() && remaining[0] == '"') {
                            // Find closing quote
                            size_t endQuote = remaining.find('"', 1);
                            if (endQuote != string::npos) {
                                token = remaining.substr(1, endQuote - 1); // Extract content between quotes
                                remaining = remaining.substr(endQuote + 1);
                            } else {
                                // Malformed JSON - no closing quote
                                LOGERR("Malformed JSON: missing closing quote");
                                break;
                            }
                        } else {
                            // Malformed JSON - expected quoted string
                            LOGERR("Malformed JSON: expected quoted string");
                            break;
                        }

                        // Add token to result if not empty
                        if (!token.empty()) {
                            if (!first) commaSeparatedLanguages += ",";
                            commaSeparatedLanguages += token;
                            first = false;
                        }
                    }
                }
            } else if (languages == "[]") {
                // Handle empty array case
                commaSeparatedLanguages = "";
            } else {
                // Handle single string value case (e.g., "tam")
                // Remove quotes if present and use as-is
                if (languages.length() >= 2 && languages[0] == '"' && languages.back() == '"') {
                    commaSeparatedLanguages = languages.substr(1, languages.length() - 2);
                } else {
                    commaSeparatedLanguages = languages;
                }
                LOGINFO("Handling single string value: %s", commaSeparatedLanguages.c_str());
            }

            LOGINFO("Converted JSON array to comma-separated: %s", commaSeparatedLanguages.c_str());

            Core::hresult rc = userSettings->SetPreferredAudioLanguages(commaSeparatedLanguages);

            if (rc == Core::ERROR_NONE) {
                // Transform response: return_or_error(null, "couldn't set preferred audio languages")
                // Success case - return null (no error)
                return Core::ERROR_NONE;
            } else {
                LOGERR("Failed to call SetPreferredAudioLanguages on UserSettings COM interface, error: %u", rc);
                // Error case - return error
                return Core::ERROR_GENERAL;
            }
        }

        Core::hresult SetPreferredCaptionsLanguages(const string& preferredLanguages) {
            LOGINFO("SetPreferredCaptionsLanguages to UserSettings COM interface: %s", preferredLanguages.c_str());

            Exchange::IUserSettings* userSettings = GetUserSettingsInterface();
            if (userSettings == nullptr) {
                LOGERR("UserSettings COM interface not available");
                return Core::ERROR_UNAVAILABLE;
            }

            // Transform request: { preferredLanguages: (.value | join(","))}
            // The preferredLanguages parameter can be either:
            // 1. A JSON array: ["eng","fra","spa"] -> "eng,fra,spa"
            // 2. A single string: "tam" -> "tam"

            string commaSeparatedLanguages;

            // Check if input is a JSON array
            if (preferredLanguages.length() >= 2 && preferredLanguages[0] == '[' && preferredLanguages.back() == ']') {
                // Handle JSON array format
                string arrayContent = preferredLanguages.substr(1, preferredLanguages.length() - 2); // Remove [ ]

                if (!arrayContent.empty()) {
                    // Use a cleaner parsing approach similar to SetPreferredAudioLanguages
                    string remaining = arrayContent;
                    bool first = true;

                    while (!remaining.empty()) {
                        // Skip whitespace and commas
                        size_t start = remaining.find_first_not_of(" \t,");
                        if (start == string::npos) break;

                        remaining = remaining.substr(start);

                        string token;
                        if (!remaining.empty() && remaining[0] == '"') {
                            // Find closing quote
                            size_t endQuote = remaining.find('"', 1);
                            if (endQuote != string::npos) {
                                token = remaining.substr(1, endQuote - 1); // Extract content between quotes
                                remaining = remaining.substr(endQuote + 1);
                            } else {
                                // Malformed JSON - no closing quote
                                LOGERR("Malformed JSON: missing closing quote");
                                break;
                            }
                        } else {
                            // Malformed JSON - expected quoted string
                            LOGERR("Malformed JSON: expected quoted string");
                            break;
                        }

                        // Add token to result if not empty
                        if (!token.empty()) {
                            if (!first) commaSeparatedLanguages += ",";
                            commaSeparatedLanguages += token;
                            first = false;
                        }
                    }
                }
            } else if (preferredLanguages == "[]") {
                // Handle empty array case
                commaSeparatedLanguages = "";
            } else {
                // Handle single string value case (e.g., "tam")
                // Remove quotes if present and use as-is
                if (preferredLanguages.length() >= 2 && preferredLanguages[0] == '"' && preferredLanguages.back() == '"') {
                    commaSeparatedLanguages = preferredLanguages.substr(1, preferredLanguages.length() - 2);
                } else {
                    commaSeparatedLanguages = preferredLanguages;
                }
                LOGINFO("Handling single string value: %s", commaSeparatedLanguages.c_str());
            }

            LOGINFO("Converted JSON array to comma-separated: %s", commaSeparatedLanguages.c_str());

            Core::hresult rc = userSettings->SetPreferredCaptionsLanguages(commaSeparatedLanguages);

            if (rc == Core::ERROR_NONE) {
                // Transform response: return_or_error(null, "couldn't set preferred captions languages")
                // Success case - return null (no error)
                return Core::ERROR_NONE;
            } else {
                LOGERR("Failed to call SetPreferredCaptionsLanguages on UserSettings COM interface, error: %u", rc);
                // Error case - return error
                return Core::ERROR_GENERAL;
            }
        }

    private:
        class UserSettingsNotificationHandler: public Exchange::IUserSettings::INotification {
            public:
                 UserSettingsNotificationHandler(UserSettingsDelegate& parent) : mParent(parent),registered(false){}
                ~UserSettingsNotificationHandler(){}

        void OnAudioDescriptionChanged(const bool enabled) {
            mParent.Dispatch( "accessibility.onaudiodescriptionsettingschanged", ObjectUtils::CreateBooleanJsonString("enabled", enabled));
        }

        void OnPreferredAudioLanguagesChanged(const string& preferredLanguages) {
            mParent.Dispatch( "localization.onpreferredaudiolanguageschanged", preferredLanguages);
        }

        void OnPresentationLanguageChanged(const string& presentationLanguage) {
            
            mParent.Dispatch( "localization.onlocalechanged", presentationLanguage);

            // check presentationLanguage is a delimitted string like "en-US"
            // add logic to get the "en" if the value is "en-US"
            if (presentationLanguage.find('-') != string::npos) {
                string language = presentationLanguage.substr(0, presentationLanguage.find('-'));
                // Wrap in quotes to make it a valid JSON string
                string languageJson = "\"" + language + "\"";
                mParent.Dispatch( "localization.onlanguagechanged", languageJson);
            } else {
                LOGWARN("invalid value=%s set it must be a delimited string like en-US", presentationLanguage.c_str());
            }
        }

        void OnCaptionsChanged(const bool enabled) {
            mParent.Dispatch( "accessibility.onclosedcaptionssettingschanged", ObjectUtils::CreateBooleanJsonString("enabled", enabled));
        }

        void OnPreferredCaptionsLanguagesChanged(const string& preferredLanguages) {
            mParent.Dispatch( "closedcaptions.onpreferredlanguageschanged", preferredLanguages);
        }

        void OnPreferredClosedCaptionServiceChanged(const string& service) {
            mParent.Dispatch( "OnPreferredClosedCaptionServiceChanged", service);
        }

        void OnPrivacyModeChanged(const string& privacyMode) {
            mParent.Dispatch( "OnPrivacyModeChanged", privacyMode);
        }

        void OnPinControlChanged(const bool pinControl) {
            mParent.Dispatch( "OnPinControlChanged", ObjectUtils::BoolToJsonString(pinControl));
        }

        void OnViewingRestrictionsChanged(const string& viewingRestrictions) {
            mParent.Dispatch( "OnViewingRestrictionsChanged", viewingRestrictions);
        }

        void OnViewingRestrictionsWindowChanged(const string& viewingRestrictionsWindow) {
            mParent.Dispatch( "OnViewingRestrictionsWindowChanged", viewingRestrictionsWindow);
        }

        void OnLiveWatershedChanged(const bool liveWatershed) {
            mParent.Dispatch( "OnLiveWatershedChanged", ObjectUtils::BoolToJsonString(liveWatershed));
        }

        void OnPlaybackWatershedChanged(const bool playbackWatershed) {
            mParent.Dispatch( "OnPlaybackWatershedChanged", ObjectUtils::BoolToJsonString(playbackWatershed));
        }

        void OnBlockNotRatedContentChanged(const bool blockNotRatedContent) {
            mParent.Dispatch( "OnBlockNotRatedContentChanged", ObjectUtils::BoolToJsonString(blockNotRatedContent));
        }

        void OnPinOnPurchaseChanged(const bool pinOnPurchase) {
            mParent.Dispatch( "OnPinOnPurchaseChanged", ObjectUtils::BoolToJsonString(pinOnPurchase));
        }

        void OnHighContrastChanged(const bool enabled) {
            mParent.Dispatch( "accessibility.onhighcontrastuichanged", ObjectUtils::BoolToJsonString(enabled));
        }

        void OnVoiceGuidanceChanged(const bool enabled) {
            mParent.Dispatch( "accessibility.onvoiceguidancesettingschanged", ObjectUtils::CreateBooleanJsonString("enabled", enabled));
        }

        void OnVoiceGuidanceRateChanged(const double rate) {
            mParent.Dispatch( "OnVoiceGuidanceRateChanged", std::to_string(rate));
        }

        void OnVoiceGuidanceHintsChanged(const bool hints) {
            mParent.Dispatch( "OnVoiceGuidanceHintsChanged", std::to_string(hints));
        }

        void OnContentPinChanged(const string& contentPin) {
            mParent.Dispatch( "OnContentPinChanged", contentPin);
        }
                
                // New Method for Set registered
                void SetRegistered(bool state) {
                    std::lock_guard<std::mutex> lock(registerMutex);
                    registered = state;
                }

                // New Method for get registered
                bool GetRegistered() {
                    std::lock_guard<std::mutex> lock(registerMutex);
                    return registered;
                }

                BEGIN_INTERFACE_MAP(NotificationHandler)
                INTERFACE_ENTRY(Exchange::IUserSettings::INotification)
                END_INTERFACE_MAP
            private:
                    UserSettingsDelegate& mParent;
                    bool registered;
                    std::mutex registerMutex;

        };
        Exchange::IUserSettings *mUserSettings;
        PluginHost::IShell* mShell;
        Core::Sink<UserSettingsNotificationHandler> mNotificationHandler;

        
};
#endif
