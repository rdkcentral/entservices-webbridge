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

#include "Migration.h"

#define API_VERSION_NUMBER_MAJOR 1
#define API_VERSION_NUMBER_MINOR 0
#define API_VERSION_NUMBER_PATCH 0


namespace WPEFramework
{

    namespace {

        static Plugin::Metadata<Plugin::Migration> metadata(
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
     *Register Migration module as wpeframework plugin
     **/
    SERVICE_REGISTRATION(Migration, API_VERSION_NUMBER_MAJOR, API_VERSION_NUMBER_MINOR, API_VERSION_NUMBER_PATCH);

    Migration::Migration() : _service(nullptr), _connectionId(0), _migration(nullptr)
    {
        SYSLOG(Logging::Startup, (_T("Migration Constructor")));
    }

    Migration::~Migration()
    {
        SYSLOG(Logging::Shutdown, (string(_T("Migration Destructor"))));
    }

    const string Migration::Initialize(PluginHost::IShell* service)
    {
        string message="";

        ASSERT(nullptr != service);
        ASSERT(nullptr == _service);
        ASSERT(nullptr == _migration);
        ASSERT(0 == _connectionId);

        SYSLOG(Logging::Startup, (_T("Migration::Initialize: PID=%u"), getpid()));

        _service = service;
        _service->AddRef();
        _migration = _service->Root<Exchange::IMigration>(_connectionId, 5000, _T("MigrationImplementation"));

        if(nullptr != _migration)
        {
            // Invoking Plugin API register to wpeframework
            Exchange::JMigration::Register(*this, _migration);
        }
        else
        {
            SYSLOG(Logging::Startup, (_T("Migration::Initialize: Failed to initialise Migration plugin")));
            message = _T("Migration plugin could not be initialised");
        }

        return message;
    }

    void Migration::Deinitialize(PluginHost::IShell* service)
    {
        ASSERT(_service == service);

        SYSLOG(Logging::Shutdown, (string(_T("Migration::Deinitialize"))));

        if (nullptr != _migration)
        {
            Exchange::JMigration::Unregister(*this);

            // Stop processing:
            RPC::IRemoteConnection* connection = service->RemoteConnection(_connectionId);
            VARIABLE_IS_NOT_USED uint32_t result = _migration->Release();

            _migration = nullptr;

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
        SYSLOG(Logging::Shutdown, (string(_T("Migration de-initialised"))));
    }

    string Migration::Information() const
    {
       return string();
    }

    void Migration::Deactivated(RPC::IRemoteConnection* connection)
    {
        if (connection->Id() == _connectionId) {
            ASSERT(nullptr != _service);
            Core::IWorkerPool::Instance().Submit(PluginHost::IShell::Job::Create(_service, PluginHost::IShell::DEACTIVATED, PluginHost::IShell::FAILURE));
        }
    }
} // namespace Plugin
} // namespace WPEFramework
