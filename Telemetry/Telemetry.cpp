/**
* If not stated otherwise in this file or this component's LICENSE
* file the following copyright and licenses apply:
*
* Copyright 2019 RDK Management
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

#include "Telemetry.h"

#define API_VERSION_NUMBER_MAJOR 1
#define API_VERSION_NUMBER_MINOR 2
#define API_VERSION_NUMBER_PATCH 2

namespace WPEFramework
{

    namespace {

        static Plugin::Metadata<Plugin::Telemetry> metadata(
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
     *Register Telemetry module as wpeframework plugin
     **/
    SERVICE_REGISTRATION(Telemetry, API_VERSION_NUMBER_MAJOR, API_VERSION_NUMBER_MINOR, API_VERSION_NUMBER_PATCH);

    Telemetry::Telemetry() : _service(nullptr), _connectionId(0), _telemetry(nullptr), _telemetryNotification(this)
    {
        SYSLOG(Logging::Startup, (_T("Telemetry Constructor")));
    }

    Telemetry::~Telemetry()
    {
        SYSLOG(Logging::Shutdown, (string(_T("Telemetry Destructor"))));
    }

    const string Telemetry::Initialize(PluginHost::IShell* service)
    {
        string message="";

        ASSERT(nullptr != service);
        ASSERT(nullptr == _service);
        ASSERT(nullptr == _telemetry);
        ASSERT(0 == _connectionId);

        SYSLOG(Logging::Startup, (_T("Telemetry::Initialize: PID=%u"), getpid()));

        _service = service;
        _service->AddRef();
        _service->Register(&_telemetryNotification);
        _telemetry = _service->Root<Exchange::ITelemetry>(_connectionId, 5000, _T("TelemetryImplementation"));

        if(nullptr != _telemetry)
        {
	    configure = _telemetry->QueryInterface<Exchange::IConfiguration>();
	    if (configure != nullptr)
            {
		uint32_t result = configure->Configure(_service);
		if(result != Core::ERROR_NONE)
                {
                    message = _T("Telemetry could not be configured");
                }
            }
            else
            {
		message = _T("Telemetry implementation did not provide a configuration interface");
            }
            // Register for notifications
            _telemetry->Register(&_telemetryNotification);
            // Invoking Plugin API register to wpeframework
            Exchange::JTelemetry::Register(*this, _telemetry);
        }
        else
        {
            SYSLOG(Logging::Startup, (_T("Telemetry::Initialize: Failed to initialise Telemetry plugin")));
            message = _T("Telemetry plugin could not be initialised");
        }
        return message;
    }

    void Telemetry::Deinitialize(PluginHost::IShell* service)
    {
        ASSERT(_service == service);

        SYSLOG(Logging::Shutdown, (string(_T("Telemetry::Deinitialize"))));

        // Make sure the Activated and Deactivated are no longer called before we start cleaning up..
        _service->Unregister(&_telemetryNotification);

        if (nullptr != _telemetry)
        {

            _telemetry->Unregister(&_telemetryNotification);
            Exchange::JTelemetry::Unregister(*this);

            if (configure != nullptr) {
                configure->Release();
                configure = nullptr;
            }

            // Stop processing:
            RPC::IRemoteConnection* connection = service->RemoteConnection(_connectionId);
            VARIABLE_IS_NOT_USED uint32_t result = _telemetry->Release();

            _telemetry = nullptr;

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
                   LOGWARN("Connection terminated successfully.");
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
        SYSLOG(Logging::Shutdown, (string(_T("Telemetry de-initialised"))));
    }

    string Telemetry::Information() const
    {
       return ("This Telemetry Plugin facilitates to persist event data for monitoring applications");
    }

    void Telemetry::Deactivated(RPC::IRemoteConnection* connection)
    {
        if (connection->Id() == _connectionId) {
            ASSERT(nullptr != _service);
            Core::IWorkerPool::Instance().Submit(PluginHost::IShell::Job::Create(_service, PluginHost::IShell::DEACTIVATED, PluginHost::IShell::FAILURE));
        }
    }
} // namespace Plugin
} // namespace WPEFramework
