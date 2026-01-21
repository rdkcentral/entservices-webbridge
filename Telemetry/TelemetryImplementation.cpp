/**
* If not stated otherwise in this file or this component's LICENSE
* file the following copyright and licenses apply:
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
**/

#include "TelemetryImplementation.h"

#include "UtilsJsonRpc.h"
#include "UtilsTelemetry.h"
#include "UtilsController.h"

#include "rfcapi.h"
#include <fstream>

#ifdef HAS_RBUS
#include "rbus.h"

#define RBUS_COMPONENT_NAME "TelemetryThunderPlugin"
#define T2_ON_DEMAND_REPORT "Device.X_RDKCENTRAL-COM_T2.UploadDCMReport"
#define T2_ABORT_ON_DEMAND_REPORT "Device.X_RDKCENTRAL-COM_T2.AbortDCMReport"
#endif

#define RFC_CALLERID "Telemetry"
#define RFC_REPORT_PROFILES "Device.X_RDKCENTRAL-COM_T2.ReportProfiles"
#define RFC_REPORT_DEFAULT_PROFILE_ENABLE "Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.Telemetry.FTUEReport.Enable"
#define T2_PERSISTENT_FOLDER "/opt/.t2reportprofiles/"
#define DEFAULT_PROFILES_FILE "/etc/t2profiles/default.json"
#define OPTOUT_TELEMETRY_STATUS "/opt/tmtryoptout"

#define USERSETTINGS_CALLSIGN "org.rdk.UserSettings"

#define API_VERSION_NUMBER_MAJOR 1
#define API_VERSION_NUMBER_MINOR 2
#define API_VERSION_NUMBER_PATCH 2

#ifdef HAS_RBUS
#define RBUS_PRIVACY_MODE_EVENT_NAME "Device.X_RDKCENTRAL-COM_Privacy.PrivacyMode"

static rbusError_t rbusHandleStatus = RBUS_ERROR_NOT_INITIALIZED;
static rbusHandle_t rbusHandle;

#endif
using PowerState = WPEFramework::Exchange::IPowerManager::PowerState;

namespace WPEFramework {
namespace Plugin {

    SERVICE_REGISTRATION(TelemetryImplementation, 1, 0);
    TelemetryImplementation* TelemetryImplementation::_instance = nullptr;
    
    TelemetryImplementation::TelemetryImplementation()
    : _adminLock()
    , _service(nullptr)
    , _pwrMgrNotification(*this)
#ifdef HAS_RBUS
    , _userSettingsPlugin(nullptr)
    , _userSettingsNotification(*this)
#endif
    , _registeredEventHandlers(false)
    {
        LOGINFO("Create TelemetryImplementation Instance");
        TelemetryImplementation::_instance = this;
        Utils::Telemetry::init();
    }

    TelemetryImplementation::~TelemetryImplementation()
    {
         if (_powerManagerPlugin) {
		 // Unregister from PowerManagerPlugin Notification
		_powerManagerPlugin->Unregister(_pwrMgrNotification.baseInterface<Exchange::IPowerManager::IModeChangedNotification>());
                _powerManagerPlugin.Reset();
            }

            LOGINFO("Call TelemetryImplementation destructor\n");
            _registeredEventHandlers = false;

            TelemetryImplementation::_instance = nullptr;
            _service = nullptr;
#ifdef HAS_RBUS
            if (RBUS_ERROR_SUCCESS == rbusHandleStatus) {
                rbus_close(rbusHandle);
                rbusHandleStatus = RBUS_ERROR_NOT_INITIALIZED;
            }

            if (_userSettingsPlugin) {
                 _userSettingsPlugin->Unregister(&_userSettingsNotification);
                 _userSettingsPlugin->Release();
                 _userSettingsPlugin = nullptr;
            }
#endif
    }

    Core::hresult TelemetryImplementation::Register(Exchange::ITelemetry::INotification *notification)
    {
        ASSERT (nullptr != notification);

        _adminLock.Lock();

        // Make sure we can't register the same notification callback multiple times
        if (std::find(_telemetryNotification.begin(), _telemetryNotification.end(), notification) == _telemetryNotification.end())
        {
            _telemetryNotification.push_back(notification);
            notification->AddRef();
        }
        else
        {
            LOGERR("same notification is registered already");
        }

        _adminLock.Unlock();

        return Core::ERROR_NONE;
    }
    
    Core::hresult TelemetryImplementation::Unregister(Exchange::ITelemetry::INotification *notification )
    {
        Core::hresult status = Core::ERROR_GENERAL;

        ASSERT (nullptr != notification);

        _adminLock.Lock();

        // we just unregister one notification once
        auto itr = std::find(_telemetryNotification.begin(), _telemetryNotification.end(), notification);
        if (itr != _telemetryNotification.end())
        {
            (*itr)->Release();
            _telemetryNotification.erase(itr);
            status = Core::ERROR_NONE;
        }
        else
        {
            LOGERR("notification not found");
        }

        _adminLock.Unlock();

        return status;
    }

#ifdef HAS_RBUS
    void TelemetryImplementation::getPrivacyMode()
    {
         ASSERT(_service != nullptr);

         _userSettingsPlugin = _service->QueryInterfaceByCallsign<WPEFramework::Exchange::IUserSettings>(USERSETTINGS_CALLSIGN);
         if (_userSettingsPlugin)
         {
             _userSettingsPlugin->Register(&_userSettingsNotification);
                
             std::string privacyMode;
             if (_userSettingsPlugin->GetPrivacyMode(privacyMode) == Core::ERROR_NONE)
             {
                 notifyT2PrivacyMode(privacyMode);
             }
             else
             {
                LOGERR("Failed to get privacy mode");
             }
         }
		 else
         {
                LOGERR("Failed to get _userSettingsPlugin handle");
         }
    }
    
    static void t2EventHandler(rbusHandle_t handle, char const* methodName, rbusError_t error, rbusObject_t param)
    {
        LOGINFO("Got %s rbus callback", methodName);

        if (RBUS_ERROR_SUCCESS == error)
        {
            rbusValue_t uploadStatus = rbusObject_GetValue(param, "UPLOAD_STATUS");

            if(uploadStatus)
            {
                if (TelemetryImplementation::_instance)
                    TelemetryImplementation::_instance->onReportUploadStatus(rbusValue_GetString(uploadStatus, NULL));
            }
            else
            {
                LOGERR("No 'UPLOAD_STATUS' value");
                if (TelemetryImplementation::_instance)
                    TelemetryImplementation::_instance->onReportUploadStatus("No 'UPLOAD_STATUS' value");
            }
        }
        else
        {
            std::stringstream str;
            str << "Call failed with " << error << " error"; 
            LOGERR("%s", str.str().c_str());
            if (TelemetryImplementation::_instance)
                TelemetryImplementation::_instance->onReportUploadStatus(str.str().c_str());
        }
    }
    
    void TelemetryImplementation::onReportUploadStatus(const char* status)
    {
        JsonObject eventData;
        std::string s(status);
        eventData["telemetryUploadStatus"] = s == "SUCCESS" ? "UPLOAD_SUCCESS" : "UPLOAD_FAILURE";
        dispatchEvent(TELEMETRY_EVENT_ONREPORTUPLOAD, eventData);
    }

    void TelemetryImplementation::notifyT2PrivacyMode(std::string privacyMode)
    {
        LOGINFO("Privacy mode is %s", privacyMode.c_str());
        if (RBUS_ERROR_SUCCESS != rbusHandleStatus)
        {
            rbusHandleStatus = rbus_open(&rbusHandle, RBUS_COMPONENT_NAME);
        }

        if (RBUS_ERROR_SUCCESS == rbusHandleStatus)
        {
            rbusValue_t value;
            rbusSetOptions_t opts = {true, 0};

            rbusValue_Init(&value);
            rbusValue_SetString(value, privacyMode.c_str());
            int rc = rbus_set(rbusHandle, RBUS_PRIVACY_MODE_EVENT_NAME, value, &opts);
            if (rc != RBUS_ERROR_SUCCESS)
            {
                std::stringstream str;
                str << "Failed to set property " << RBUS_PRIVACY_MODE_EVENT_NAME << ": " << rc;
                LOGERR("%s", str.str().c_str());
            }
            rbusValue_Release(value);
        }
        else
        {
            LOGERR("rbus_open failed with error code %d", rbusHandleStatus);
        }
    }
    
    static void t2OnAbortEventHandler(rbusHandle_t handle, char const* methodName, rbusError_t error, rbusObject_t param)
    {
        LOGINFO("Got %s rbus callback", methodName);
    }
    
    
#endif 
    
    void TelemetryImplementation::setRFCReportProfiles()
    {
        JsonObject config;
        config.FromString(_service->ConfigLine());
        std::string t2PersistentFolder = config.HasLabel("t2PersistentFolder") ? config["t2PersistentFolder"].String() : T2_PERSISTENT_FOLDER;
        bool isEMpty = true;
        DIR *d = opendir(t2PersistentFolder.c_str());
        if (NULL != d)
        {
            struct dirent *de;

            while ((de = readdir(d)))
            {
                if (0 == de->d_name[0] || 0 == strcmp(de->d_name, ".") || 0 == strcmp(de->d_name, ".."))
                    continue;

                isEMpty = false;
                break;
            }

            closedir(d);
        }

        if (isEMpty)
        {
            Core::File file;
            std::string defaultProfilesFile = config.HasLabel("defaultProfilesFile") ? config["defaultProfilesFile"].String() : DEFAULT_PROFILES_FILE;
            file = defaultProfilesFile.c_str();
            file.Open();
            if (file.IsOpen())
            {
                if (file.Size() > 0)
                {
                    std::vector <char> defaultProfile;
                    defaultProfile.resize(file.Size() + 1);
                    uint32_t rs = file.Read((uint8_t *)defaultProfile.data(), file.Size());
                    defaultProfile.data()[rs] = 0;
                    if (file.Size() == rs)
                    {

                        std::stringstream ss;
                        // Escaping quotes
                        for (uint32_t n = 0; n < rs; n++)
                        {
                            char ch = defaultProfile.data()[n];
                            if ('\"' == ch)
                                ss << "\\";
                            ss << ch;
                        }

                        WDMP_STATUS wdmpStatus = setRFCParameter((char *)RFC_CALLERID, RFC_REPORT_PROFILES, ss.str().c_str(), WDMP_STRING);
                        if (WDMP_SUCCESS != wdmpStatus)
                        {
                            LOGERR("Failed to set Device.X_RDKCENTRAL-COM_T2.ReportProfiles: %d", wdmpStatus);
                        }
                    }
                    else
                    {
                        LOGERR("Got wrong number of bytes, %d instead of %d", rs, (int)file.Size());
                    }
                }
                else
                {
                    LOGERR("%s is 0 size", defaultProfilesFile.c_str());
                }
            }
            else
            {
                LOGERR("Failed to open %s", defaultProfilesFile.c_str());
            }
        }
    }
    uint32_t TelemetryImplementation::Configure(PluginHost::IShell* service)
    {
        LOGINFO("Configuring TelemetryImplementation");
        uint32_t result = Core::ERROR_NONE;
        ASSERT(service != nullptr);
        _service = service;
        _service->AddRef();
        InitializePowerManager();

#ifdef HAS_RBUS        
        getPrivacyMode();
#endif        
        setRFCReportProfiles();
        
        return result;
    }
    
    void TelemetryImplementation::InitializePowerManager()
    {
        LOGINFO("Connect the COM-RPC socket\n");
        _powerManagerPlugin = PowerManagerInterfaceBuilder(_T("org.rdk.PowerManager"))
                            .withIShell(_service)
                            .withRetryIntervalMS(200)
                            .withRetryCount(25)
                            .createInterface();

        registerEventHandlers();
    }
    
    
    void TelemetryImplementation::registerEventHandlers()
    {
        ASSERT (_powerManagerPlugin);

        if(!_registeredEventHandlers && _powerManagerPlugin) 
        {
            _registeredEventHandlers = true;
            _powerManagerPlugin->Register(_pwrMgrNotification.baseInterface<Exchange::IPowerManager::IModeChangedNotification>());
        }
    }

    void TelemetryImplementation::onPowerModeChanged(const PowerState currentState, const PowerState newState)
    {
        JsonObject params;

        if (WPEFramework::Exchange::IPowerManager::POWER_STATE_STANDBY == newState ||
            WPEFramework::Exchange::IPowerManager::POWER_STATE_STANDBY_LIGHT_SLEEP == newState)
        {
            if (WPEFramework::Exchange::IPowerManager::POWER_STATE_ON == currentState)
            {
                Core::IWorkerPool::Instance().Submit(Job::Create( this,TELEMETRY_EVENT_UPLOADREPORT, params));
            }
        }
        else if(WPEFramework::Exchange::IPowerManager::POWER_STATE_STANDBY_DEEP_SLEEP == newState)
        {
            Core::IWorkerPool::Instance().Submit(Job::Create( this,TELEMETRY_EVENT_ABORTREPORT, params));
        }
    }

    void TelemetryImplementation::dispatchEvent(Event event, const JsonValue &params)
    {
        Core::IWorkerPool::Instance().Submit(Job::Create(this, event, params));
    }
    
    void TelemetryImplementation::Dispatch(Event event, const JsonValue params)
    {
        _adminLock.Lock();
        
        std::list<Exchange::ITelemetry::INotification*>::const_iterator index(_telemetryNotification.begin());

        switch(event)
        {
            case TELEMETRY_EVENT_UPLOADREPORT:
                TelemetryImplementation::_instance->UploadReport();
                break;

            case TELEMETRY_EVENT_ABORTREPORT:
                TelemetryImplementation::_instance->AbortReport();
                break;

            case TELEMETRY_EVENT_ONREPORTUPLOAD:
                while (index != _telemetryNotification.end())
                {
                    (*index)->OnReportUpload(params.String());
                    ++index;
                }
                break;

            default:
                LOGWARN("Event[%u] not handled", event);
                break;
        }
        _adminLock.Unlock();
    }
    
    Core::hresult TelemetryImplementation::SetReportProfileStatus(const string& status)
    {
        LOGINFO();

        if (!status.empty())
        {
            if (status != "STARTED" && status != "COMPLETE")
            {
                LOGERR("Only the 'STARTED' or 'COMPLETE' status is allowed");
                return Core::ERROR_GENERAL;
            }

            WDMP_STATUS wdmpStatus = setRFCParameter((char *)RFC_CALLERID, RFC_REPORT_DEFAULT_PROFILE_ENABLE, status == "COMPLETE" ? "true" : "false", WDMP_BOOLEAN);
            if (WDMP_SUCCESS != wdmpStatus)
            {
                LOGERR("Failed to set %s: %d", RFC_REPORT_DEFAULT_PROFILE_ENABLE, wdmpStatus);
                return Core::ERROR_GENERAL;
            }
            return Core::ERROR_NONE;
        }

        LOGERR("Empty status' parameter");
        return Core::ERROR_GENERAL;        
    }
    
    Core::hresult TelemetryImplementation::LogApplicationEvent(const string& eventName, const string& eventValue)
    {
        LOGINFO();
        if (eventName.empty() || eventValue.empty())
        {
            LOGERR("No 'eventName' or 'eventValue' parameter");
            return Core::ERROR_GENERAL;
        }
        
        LOGINFO("eventName:%s, eventValue:%s", eventName.c_str(), eventValue.c_str());
        Utils::Telemetry::sendMessage((char *)eventName.c_str(), (char *)eventValue.c_str());
        return Core::ERROR_NONE;  
    }
    
    Core::hresult TelemetryImplementation::UploadReport()
    {
        LOGINFO("");
#ifdef HAS_RBUS
        if (RBUS_ERROR_SUCCESS != rbusHandleStatus)
        {
            rbusHandleStatus = rbus_open(&rbusHandle, RBUS_COMPONENT_NAME);
        }

        if (RBUS_ERROR_SUCCESS == rbusHandleStatus)
        {
            int rc = rbusMethod_InvokeAsync(rbusHandle, T2_ON_DEMAND_REPORT, NULL, t2EventHandler, 0);
            if (RBUS_ERROR_SUCCESS != rc)
            {
                std::stringstream str;
                str << "Failed to call " << T2_ON_DEMAND_REPORT << ": " << rc;
                LOGERR("%s", str.str().c_str());

                return Core::ERROR_RPC_CALL_FAILED;
            }
        }
        else
        {
            std::stringstream str;
            str << "rbus_open failed with error code " << rbusHandleStatus;
            LOGERR("%s", str.str().c_str());
            return Core::ERROR_OPENING_FAILED;
        }
#else
        LOGERR("No RBus support");
        return Core::ERROR_NOT_EXIST;
#endif
        return Core::ERROR_NONE;
    }
    
    Core::hresult TelemetryImplementation::AbortReport()
    {
        LOGINFO("");
#ifdef HAS_RBUS
        if (RBUS_ERROR_SUCCESS != rbusHandleStatus)
        {
            rbusHandleStatus = rbus_open(&rbusHandle, RBUS_COMPONENT_NAME);
        }

        if (RBUS_ERROR_SUCCESS == rbusHandleStatus)
        {
            int rc = rbusMethod_InvokeAsync(rbusHandle, T2_ABORT_ON_DEMAND_REPORT, NULL, t2OnAbortEventHandler, 0);
            if (RBUS_ERROR_SUCCESS != rc)
            {
                std::stringstream str;
                str << "Failed to call " << T2_ABORT_ON_DEMAND_REPORT << ": " << rc;
                LOGERR("%s", str.str().c_str());
                return Core::ERROR_RPC_CALL_FAILED;
            }
        }
        else
        {
            std::stringstream str;
            str << "rbus_open failed with error code " << rbusHandleStatus;
            LOGERR("%s", str.str().c_str());
            return Core::ERROR_OPENING_FAILED;
        }
#else
        LOGERR("No RBus support");
        return Core::ERROR_NOT_EXIST;
#endif
        return Core::ERROR_NONE;
    }

    /***
     * @brief       : Used to read file contents into a string
     * @param1[in]  : Complete file name with path
     * @param2[out] : Destination string object filled with file contents
     * @return      : <bool>; TRUE if operation success; else FALSE.
     */
    bool TelemetryImplementation::getFileContent(std::string fileName, std::string& fileContent)
    {
        std::ifstream inFile(fileName.c_str(), ios::in);
        if (!inFile.is_open()) return false;

        std::stringstream buffer;
        buffer << inFile.rdbuf();
        fileContent = buffer.str();
        inFile.close();

        return true;
    }

    Core::hresult TelemetryImplementation::SetOptOutTelemetry(const bool optOut, TelemetrySuccess& successResult)
    {
        uint32_t result = Core::ERROR_GENERAL;
        ofstream optfile;
        optfile.open(OPTOUT_TELEMETRY_STATUS, ios::out);
        if (optfile) {
            optfile << (optOut ? "true" :"false");
            optfile.close();
            successResult.success = true;
            LOGINFO("TelemetryOptOut flag set to %s\n",optOut ? "true" :"false");
            result = Core::ERROR_NONE;
        } else {
            successResult.success = false;
            LOGERR("Couldn't update Telemetry Opt Out flag\n");
        }
        return result;
    }

    Core::hresult TelemetryImplementation::IsOptOutTelemetry(bool& optOut, bool& success)
    {
        string optOutStatus;

        bool retVal = getFileContent(OPTOUT_TELEMETRY_STATUS, optOutStatus);
        if (retVal && optOutStatus.length()) {
            if (optOutStatus.find("true") != string::npos) {
                optOut = true;
            } else {
                optOut = false;
            }
        }
        success = true;
        return Core::ERROR_NONE;
    }
} // namespace Plugin
} // namespace WPEFramework
