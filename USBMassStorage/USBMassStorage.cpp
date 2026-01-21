/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
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
*/

#include "USBMassStorage.h"
#include "UtilsLogging.h"
#include "UtilsJsonRpc.h"

#define API_VERSION_NUMBER_MAJOR 1
#define API_VERSION_NUMBER_MINOR 0
#define API_VERSION_NUMBER_PATCH 0

namespace WPEFramework
{

    namespace {

        static Plugin::Metadata<Plugin::USBMassStorage> metadata(
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

    namespace Plugin
    {

    /*
     *Register USBMassStorage module as wpeframework plugin
     **/
    SERVICE_REGISTRATION(USBMassStorage, API_VERSION_NUMBER_MAJOR, API_VERSION_NUMBER_MINOR, API_VERSION_NUMBER_PATCH);

    USBMassStorage::USBMassStorage() : PluginHost::JSONRPCErrorAssessor<PluginHost::JSONRPCErrorAssessorTypes::FunctionCallbackType>(USBMassStorage::OnJSONRPCError),
    _service(nullptr), _connectionId(0), _usbMassStorage(nullptr), _usbStoragesNotification(this)
    {
        SYSLOG(Logging::Startup, (_T("USBMassStorage Constructor")));
    }

    USBMassStorage::~USBMassStorage()
    {
        SYSLOG(Logging::Shutdown, (string(_T("USBMassStorage Destructor"))));
    }

    const string USBMassStorage::Initialize(PluginHost::IShell* service)
    {
        string message="";

        ASSERT(nullptr != service);
        ASSERT(nullptr == _service);
        ASSERT(nullptr == _usbMassStorage);
        ASSERT(0 == _connectionId);

        SYSLOG(Logging::Startup, (_T("USBMassStorage::Initialize: PID=%u"), getpid()));

        _service = service;
        _service->AddRef();
        _service->Register(&_usbStoragesNotification);
        _usbMassStorage = _service->Root<Exchange::IUSBMassStorage>(_connectionId, 5000, _T("USBMassStorageImplementation"));

        if(nullptr != _usbMassStorage)
        {
            configure = _usbMassStorage->QueryInterface<Exchange::IConfiguration>();
            if (configure != nullptr)
            {
                uint32_t result = configure->Configure(_service);
                if(result != Core::ERROR_NONE)
                {
                    message = _T("UsbMassStorage could not be configured");
                }
            }
            else
            {
                message = _T("UsbMassStorage implementation did not provide a configuration interface");
            }

            // Register for notifications
            _usbMassStorage->Register(&_usbStoragesNotification);
            // Invoking Plugin API register to wpeframework
            Exchange::JUSBMassStorage::Register(*this, _usbMassStorage);
        }
        else
        {
            SYSLOG(Logging::Startup, (_T("USBMassStorage::Initialize: Failed to initialise USBMassStorage plugin")));
            message = _T("USBMassStorage plugin could not be initialised");
        }

        if (0 != message.length())
        {
           Deinitialize(service);
        }

        return message;
    }

    void USBMassStorage::Deinitialize(PluginHost::IShell* service)
    {
        ASSERT(_service == service);

        SYSLOG(Logging::Shutdown, (string(_T("USBMassStorage::Deinitialize"))));

        // Make sure the Activated and Deactivated are no longer called before we start cleaning up..
        _service->Unregister(&_usbStoragesNotification);

        if (nullptr != _usbMassStorage)
        {
            _usbMassStorage->Unregister(&_usbStoragesNotification);
            Exchange::JUSBMassStorage::Unregister(*this);

            if (configure != nullptr) {
                configure->Release();
                configure = nullptr;
            }

            // Stop processing:
            RPC::IRemoteConnection* connection = service->RemoteConnection(_connectionId);
            VARIABLE_IS_NOT_USED uint32_t result = _usbMassStorage->Release();

            _usbMassStorage = nullptr;

            // It should have been the last reference we are releasing,
            // so it should endup in a DESTRUCTION_SUCCEEDED, if not we
            // are leaking...
            ASSERT(result == Core::ERROR_DESTRUCTION_SUCCEEDED);

            // If this was running in a (container) process...
            if (nullptr != connection)
            {
               // Lets trigger the cleanup sequence for
               // out-of-process code. Which will guard
               // that unwilling processes, get shot if
               // not stopped friendly :-)
               try
               {
                   connection->Terminate();
                   // Log success if needed
                   LOGINFO("Connection terminated successfully.");
               }
               catch (const std::exception& e)
               {
                   std::string errorMessage = "Failed to terminate connection: ";
                   errorMessage += e.what();
                   LOGWARN("%s",errorMessage.c_str());
               }

               connection->Release();
            }
        }

        _connectionId = 0;
        _service->Release();
        _service = nullptr;
        SYSLOG(Logging::Shutdown, (string(_T("USBMassStorage de-initialised"))));
    }

    string USBMassStorage::Information() const
    {
       return ("The USBMassStorage Plugin manages device mounting and stores mount information.");
    }

    void USBMassStorage::Deactivated(RPC::IRemoteConnection* connection)
    {
        if (connection->Id() == _connectionId)
        {
            LOGINFO("USBMassStorage Deactivated");
            ASSERT(nullptr != _service);
            Core::IWorkerPool::Instance().Submit(PluginHost::IShell::Job::Create(_service, PluginHost::IShell::DEACTIVATED, PluginHost::IShell::FAILURE));
        }
    }

    void USBMassStorage::Notification::Activated(RPC::IRemoteConnection*)
    {
    }

    void USBMassStorage::Notification::Deactivated(RPC::IRemoteConnection *connection)
    {
       _parent.Deactivated(connection);
    }

    void USBMassStorage::Notification::OnDeviceMounted(const Exchange::IUSBMassStorage::USBStorageDeviceInfo &deviceInfo, Exchange::IUSBMassStorage::IUSBStorageMountInfoIterator* const mountPoints)
    {
        Core::JSON::ArrayType<JsonData::USBMassStorage::USBStorageMountInfoData> mountPointsArray;
        JsonData::USBMassStorage::USBStorageDeviceInfoData jsonDeviceInfo;

        if (mountPoints != nullptr)
        {
            Exchange::IUSBMassStorage::USBStorageMountInfo resultItem{};
            Core::JSON::String devicePath,deviceName;

            jsonDeviceInfo.DevicePath = deviceInfo.devicePath;
            jsonDeviceInfo.DeviceName = deviceInfo.deviceName;

            while (mountPoints->Next(resultItem) == true) { mountPointsArray.Add() = resultItem; }

            Core::JSON::Container eventPayload;
            eventPayload.Add(_T("deviceinfo"), &jsonDeviceInfo);
            eventPayload.Add(_T("mountPoints"), &mountPointsArray);

            _parent.Notify(_T("onDeviceMounted"), eventPayload);
        }
    }

    void USBMassStorage::Notification::OnDeviceUnmounted(const Exchange::IUSBMassStorage::USBStorageDeviceInfo &deviceInfo, Exchange::IUSBMassStorage::IUSBStorageMountInfoIterator* const mountPoints)
    {
        Core::JSON::ArrayType<JsonData::USBMassStorage::USBStorageMountInfoData> mountPointsArray;
        JsonData::USBMassStorage::USBStorageDeviceInfoData jsonDeviceInfo;

        if (mountPoints != nullptr)
        {
            Exchange::IUSBMassStorage::USBStorageMountInfo resultItem{};
            Core::JSON::String devicePath,deviceName;

            jsonDeviceInfo.DevicePath = deviceInfo.devicePath;
            jsonDeviceInfo.DeviceName = deviceInfo.deviceName;

            while (mountPoints->Next(resultItem) == true) { mountPointsArray.Add() = resultItem; }

            Core::JSON::Container eventPayload;
            eventPayload.Add(_T("deviceinfo"), &jsonDeviceInfo);
            eventPayload.Add(_T("mountPoints"), &mountPointsArray);

            _parent.Notify(_T("onDeviceUnmounted"), eventPayload);
        }
    }
    uint32_t USBMassStorage::OnJSONRPCError(const Core::JSONRPC::Context&, const string& method, const string& parameters, const uint32_t errorcode, string& errormessage) {
              if(IS_ENTSERVICES_ERRORCODE(errorcode))
               errormessage = ERROR_MESSAGE(errorcode);
           return errorcode;
    }
} // namespace Plugin
} // namespace WPEFramework
