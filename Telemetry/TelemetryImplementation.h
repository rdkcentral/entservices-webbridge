/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
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
*/

#pragma once

#include "Module.h"
#include <interfaces/Ids.h>
#include <interfaces/ITelemetry.h>
#include <interfaces/IConfiguration.h>
#include <interfaces/IPowerManager.h>
#include "PowerManagerInterface.h"

#ifdef HAS_RBUS
#include <interfaces/IUserSettings.h>
#endif

#include <com/com.h>
#include <core/core.h>

using namespace WPEFramework;
using PowerState = WPEFramework::Exchange::IPowerManager::PowerState;
using ThermalTemperature = WPEFramework::Exchange::IPowerManager::ThermalTemperature;

namespace WPEFramework {
namespace Plugin {
    class TelemetryImplementation : public Exchange::ITelemetry, public Exchange::IConfiguration
    {
        private:
            class PowerManagerNotification : public Exchange::IPowerManager::IModeChangedNotification {
            private:
                PowerManagerNotification(const PowerManagerNotification&) = delete;
                PowerManagerNotification& operator=(const PowerManagerNotification&) = delete;

            public:
                explicit PowerManagerNotification(TelemetryImplementation& parent)
                    : _parent(parent)
                {
                }
                ~PowerManagerNotification() override = default;

            public:
                void OnPowerModeChanged(const PowerState currentState, const PowerState newState) override
                {
                    _parent.onPowerModeChanged(currentState, newState);
                }

                template <typename T>
                T* baseInterface()
                {
                    static_assert(std::is_base_of<T, PowerManagerNotification>(), "base type mismatch");
                    return static_cast<T*>(this);
                }

                BEGIN_INTERFACE_MAP(PowerManagerNotification)
                INTERFACE_ENTRY(Exchange::IPowerManager::IModeChangedNotification)
                END_INTERFACE_MAP

            private:
                TelemetryImplementation& _parent;
            };

#ifdef HAS_RBUS
            class UserSettingsNotification : public Exchange::IUserSettings::INotification {
            private:
                UserSettingsNotification(const UserSettingsNotification&) = delete;
                UserSettingsNotification& operator=(const UserSettingsNotification&) = delete;

            public:
                explicit UserSettingsNotification(TelemetryImplementation& parent)
                    : _parent(parent)
                {
                }

                ~UserSettingsNotification() override = default;

                void OnPrivacyModeChanged(const string& privacyMode)
                {
                    _parent.notifyT2PrivacyMode(privacyMode);
                }

                BEGIN_INTERFACE_MAP(UserSettingsNotification)
                INTERFACE_ENTRY(Exchange::IUserSettings::INotification)
                END_INTERFACE_MAP

            private:
                TelemetryImplementation& _parent;
            };
#endif

    public:
        // We do not allow this plugin to be copied !!
        TelemetryImplementation();
        ~TelemetryImplementation() override;

        static TelemetryImplementation* instance(TelemetryImplementation *TelemetryImpl = nullptr);

        // We do not allow this plugin to be copied !!
        TelemetryImplementation(const TelemetryImplementation&) = delete;
        TelemetryImplementation& operator=(const TelemetryImplementation&) = delete;

        BEGIN_INTERFACE_MAP(TelemetryImplementation)
        INTERFACE_ENTRY(Exchange::ITelemetry)
	INTERFACE_ENTRY(Exchange::IConfiguration)
        END_INTERFACE_MAP

    public:
        enum Event
        {
            TELEMETRY_EVENT_ONREPORTUPLOAD,
            TELEMETRY_EVENT_UPLOADREPORT,
            TELEMETRY_EVENT_ABORTREPORT
        };
        class EXTERNAL Job : public Core::IDispatch {
        protected:
            Job(TelemetryImplementation* TelemetryImplementation, Event event, JsonValue &params)
                : _telemetryImplementation(TelemetryImplementation)
                , _event(event)
                , _params(params) {
                if (_telemetryImplementation != nullptr) {
                    _telemetryImplementation->AddRef();
                }
            }

       public:
            Job() = delete;
            Job(const Job&) = delete;
            Job& operator=(const Job&) = delete;
            ~Job() {
                if (_telemetryImplementation != nullptr) {
                    _telemetryImplementation->Release();
                }
            }

       public:
            static Core::ProxyType<Core::IDispatch> Create(TelemetryImplementation* telemetryImplementation, Event event, JsonValue  params ) {
#ifndef USE_THUNDER_R4
                return (Core::proxy_cast<Core::IDispatch>(Core::ProxyType<Job>::Create(telemetryImplementation, event, params)));
#else
                return (Core::ProxyType<Core::IDispatch>(Core::ProxyType<Job>::Create(telemetryImplementation, event, params)));
#endif
            }

            virtual void Dispatch() {
                _telemetryImplementation->Dispatch(_event, _params);
            }
            
        private:
            TelemetryImplementation *_telemetryImplementation;
            const Event _event;
            JsonValue _params;
        };
    public:
        virtual Core::hresult Register(Exchange::ITelemetry::INotification *notification ) override ;
        virtual Core::hresult Unregister(Exchange::ITelemetry::INotification *notification ) override;

        Core::hresult SetReportProfileStatus(const string& status) override;
        Core::hresult LogApplicationEvent(const string& eventName, const string& eventValue) override;
        Core::hresult UploadReport() override;
        Core::hresult AbortReport() override;
        Core::hresult SetOptOutTelemetry(const bool optOut, TelemetrySuccess& successResult) override;
        Core::hresult IsOptOutTelemetry(bool& optOut, bool& success) override;

        void InitializePowerManager();
        void onPowerModeChanged(const PowerState currentState, const PowerState newState);
        void registerEventHandlers();
        bool getFileContent(std::string fileName, std::string& fileContent);

        // IConfiguration interface
        uint32_t Configure(PluginHost::IShell* service) override;

#ifdef HAS_RBUS
        void notifyT2PrivacyMode(std::string privacyMode);
        void onReportUploadStatus(const char* status);
        void getPrivacyMode();
        void setRFCReportProfiles();
#endif

    private:
        mutable Core::CriticalSection _adminLock;
        PluginHost::IShell* _service;
        std::list<Exchange::ITelemetry::INotification*> _telemetryNotification;
        PowerManagerInterfaceRef _powerManagerPlugin;
        Core::Sink<PowerManagerNotification> _pwrMgrNotification;
#ifdef HAS_RBUS
        Exchange::IUserSettings* _userSettingsPlugin;
        Core::Sink<UserSettingsNotification> _userSettingsNotification;
#endif
        bool _registeredEventHandlers;
        
        void dispatchEvent(Event, const JsonValue &params);
        void Dispatch(Event event, const JsonValue params);
    public:
        static TelemetryImplementation* _instance;

        friend class Job;
    };
} // namespace Plugin
} // namespace WPEFramework
