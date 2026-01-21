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


#include "TelemetryMetrics.h"

const string WPEFramework::Plugin::TelemetryMetrics::SERVICE_NAME = "org.rdk.TelemetryMetrics";

namespace WPEFramework
{

    namespace {

        static Plugin::Metadata<Plugin::TelemetryMetrics> metadata(
            // Version (Major, Minor, Patch)
            TELEMETRY_METRICS_API_VERSION_NUMBER_MAJOR, TELEMETRY_METRICS_API_VERSION_NUMBER_MINOR, TELEMETRY_METRICS_API_VERSION_NUMBER_PATCH,
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
     *Register TelemetryMetrics module as wpeframework plugin
     **/
    SERVICE_REGISTRATION(TelemetryMetrics, TELEMETRY_METRICS_API_VERSION_NUMBER_MAJOR, TELEMETRY_METRICS_API_VERSION_NUMBER_MINOR, TELEMETRY_METRICS_API_VERSION_NUMBER_PATCH);

    TelemetryMetrics::TelemetryMetrics() :
        mCurrentService(nullptr),
        mConnectionId(0),
        mTelemetryMetricsImpl(nullptr)
    {
        SYSLOG(Logging::Startup, (_T("TelemetryMetrics Constructor")));
    }

    TelemetryMetrics::~TelemetryMetrics()
    {
        SYSLOG(Logging::Shutdown, (string(_T("TelemetryMetrics Destructor"))));
    }

    const string TelemetryMetrics::Initialize(PluginHost::IShell* service)
    {
        string message="";

        ASSERT(nullptr != service);
        ASSERT(nullptr == mCurrentService);
        ASSERT(nullptr == mTelemetryMetricsImpl);
        ASSERT(0 == mConnectionId);

        SYSLOG(Logging::Startup, (_T("TelemetryMetrics::Initialize: PID=%u"), getpid()));
        mCurrentService = service;
        if (nullptr != mCurrentService)
        {
            mCurrentService->AddRef();
            if (nullptr == (mTelemetryMetricsImpl = mCurrentService->Root<Exchange::ITelemetryMetrics>(mConnectionId, 5000, _T("TelemetryMetricsImplementation"))))
            {
                SYSLOG(Logging::Startup, (_T("TelemetryMetrics::Initialize: object creation failed")));
                message = _T("TelemetryMetrics plugin could not be initialised");
            }
        }
        else
        {
            SYSLOG(Logging::Startup, (_T("TelemetryMetrics::Initialize: service is not valid")));
            message = _T("TelemetryMetrics plugin could not be initialised");
        }

        if (0 != message.length())
        {
            Deinitialize(service);
        }

        return message;
    }

    void TelemetryMetrics::Deinitialize(PluginHost::IShell* service)
    {
        ASSERT(mCurrentService == service);

        SYSLOG(Logging::Shutdown, (string(_T("TelemetryMetrics::Deinitialize"))));

        if (nullptr != mTelemetryMetricsImpl)
        {
            // Stop processing:
            RPC::IRemoteConnection* connection = service->RemoteConnection(mConnectionId);
            //VARIABLE_IS_NOT_USED uint32_t result = mTelemetryMetricsImpl->Release();
            if (mTelemetryMetricsImpl->Release() != Core::ERROR_DESTRUCTION_SUCCEEDED) {
                SYSLOG(Logging::Shutdown, (_T("TelemetryMetrics Plugin is not properly destructed.")));
            }

            mTelemetryMetricsImpl = nullptr;

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
               connection->Terminate();
               connection->Release();
            }
        }

        mConnectionId = 0;
        mCurrentService->Release();
        mCurrentService = nullptr;
        SYSLOG(Logging::Shutdown, (string(_T("TelemetryMetrics de-initialised"))));
    }

    string TelemetryMetrics::Information() const
    {
        // No additional info to report
        return (string());
    }
} /* namespace Plugin */
} /* namespace WPEFramework */
