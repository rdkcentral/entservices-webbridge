/**
* If not stated otherwise in this file or this component's LICENSE
* file the following copyright and licenses apply:
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
**/

#pragma once

#include "Module.h"
#include <interfaces/IRuntimeManager.h>
#include <interfaces/IConfiguration.h>
#include "UtilsLogging.h"
#include "tracing/Logging.h"
#include <mutex>
#include <interfaces/IOCIContainer.h>
#include <interfaces/IStorageManager.h>
#ifdef ENABLE_AIMANAGERS_TELEMETRY_METRICS
#include <interfaces/ITelemetryMetrics.h>
#endif
#include <condition_variable>
#include "ApplicationConfiguration.h"
#include "WindowManagerConnector.h"
#include "IEventHandler.h"
#include "DobbyEventListener.h"
#include "UserIdManager.h"

#ifdef RIALTO_IN_DAC_FEATURE_ENABLED
#include "RialtoConnector.h"
#define RIALTO_TIMEOUT_MILLIS 5000
#endif
#ifdef ENABLE_AIMANAGERS_TELEMETRY_METRICS
#define TELEMETRY_MARKER_LAUNCH_TIME                         "OverallLaunchTime_split"
#define TELEMETRY_MARKER_CLOSE_TIME                          "AppCloseTime_split"
#define TELEMETRY_MARKER_SUSPEND_TIME                        "SuspendTime_split"
#define TELEMETRY_MARKER_RESUME_TIME                         "ResumeTime_split"
#define TELEMETRY_MARKER_HIBERNATE_TIME                      "HibernateTime_split"
#define TELEMETRY_MARKER_WAKE_TIME                           "WakeTime_split"
#endif

namespace WPEFramework
{
    namespace Plugin
    {
        class RuntimeManagerImplementation : public Exchange::IRuntimeManager, public Exchange::IConfiguration, public IEventHandler
        {
            private:
                class Configuration : public Core::JSON::Container {
                    public:
                        Configuration()
                            : Core::JSON::Container()
                            , runtimeAppPortal()
                        {
                            Add(_T("runtimeAppPortal"), &runtimeAppPortal);
                        }
                        ~Configuration() = default;

                        Configuration(Configuration&&) = delete;
                        Configuration(const Configuration&) = delete;
                        Configuration& operator=(Configuration&&) = delete;
                        Configuration& operator=(const Configuration&) = delete;

                    public:
                        Core::JSON::String runtimeAppPortal;
                };

            public:
                enum RuntimeEventType
                {
                    RUNTIME_MANAGER_EVENT_UNKNOWN = 0,
                    RUNTIME_MANAGER_EVENT_STATECHANGED,
                    RUNTIME_MANAGER_EVENT_CONTAINERSTARTED,
                    RUNTIME_MANAGER_EVENT_CONTAINERSTOPPED,
                    RUNTIME_MANAGER_EVENT_CONTAINERFAILED
                };

#ifdef ENABLE_AIMANAGERS_TELEMETRY_METRICS
                enum RequestType {
                    REQUEST_TYPE_NONE,
                    REQUEST_TYPE_LAUNCH,
                    REQUEST_TYPE_SUSPEND,
                    REQUEST_TYPE_RESUME,
                    REQUEST_TYPE_HIBERNATE,
                    REQUEST_TYPE_WAKE,
                    REQUEST_TYPE_TERMINATE,
                    REQUEST_TYPE_KILL
                };

                enum TelemetryMarker {
                    TELEMETRY_MARKER_UNKNOWN = 0,
                    TELEMETRY_MARKER_LAUNCH,
                    TELEMETRY_MARKER_CLOSE,
                    TELEMETRY_MARKER_RESUME,
                    TELEMETRY_MARKER_SUSPEND,
                    TELEMETRY_MARKER_HIBERNATE,
                    TELEMETRY_MARKER_WAKE
                };
#endif

                typedef struct _RuntimeAppInfo
                {
                    std::string appId;
                    std::string appInstanceId;
                    uint32_t descriptor;
                    Exchange::IRuntimeManager::RuntimeState containerState;
#ifdef ENABLE_AIMANAGERS_TELEMETRY_METRICS
                    time_t requestTime = 0;
                    RuntimeManagerImplementation::RequestType requestType = RuntimeManagerImplementation::REQUEST_TYPE_NONE;
#endif
                } RuntimeAppInfo;

                class EXTERNAL Job : public Core::IDispatch
                {
                    protected:
                    Job(RuntimeManagerImplementation *mRuntimeManagerImpl, RuntimeEventType event, JsonValue &params)
                    : mRuntimeManagerImplementation(mRuntimeManagerImpl)
                    , _event(event)
                    , _params(params)
                    {
                        if (mRuntimeManagerImplementation != nullptr)
                        {
                            mRuntimeManagerImplementation->AddRef();
                        }
                    }

                    public:
                        Job() = delete;
                        Job(const Job&) = delete;
                        Job& operator=(const Job&) = delete;
                        ~Job()
                        {
                            if (mRuntimeManagerImplementation != nullptr)
                            {
                                mRuntimeManagerImplementation->Release();
                            }
                        }

                    public:
                        static Core::ProxyType<Core::IDispatch> Create(RuntimeManagerImplementation *mRuntimeManagerImpl, RuntimeEventType event, JsonValue params)
                        {
#ifndef  USE_THUNDER_R4
                            return (Core::proxy_cast<Core::IDispatch>(Core::ProxyType<Job>::Create(mRuntimeManagerImpl, event, params)));
#else
                            return (Core::ProxyType<Core::IDispatch>(Core::ProxyType<Job>::Create(mRuntimeManagerImpl, event, params)));
#endif
                        }

                        virtual void Dispatch()
                        {
                            mRuntimeManagerImplementation->Dispatch(_event, _params);
                        }

                    private:
                        RuntimeManagerImplementation *mRuntimeManagerImplementation;
                        const RuntimeEventType _event;
                        const JsonValue _params;
                };

                RuntimeManagerImplementation ();
                ~RuntimeManagerImplementation () override;

                RuntimeManagerImplementation (const RuntimeManagerImplementation &) = delete;
                RuntimeManagerImplementation & operator=(const RuntimeManagerImplementation &) = delete;

                static RuntimeManagerImplementation* getInstance();

                BEGIN_INTERFACE_MAP(RuntimeManagerImplementation)
                INTERFACE_ENTRY(Exchange::IRuntimeManager)
                INTERFACE_ENTRY(Exchange::IConfiguration)
                END_INTERFACE_MAP

                /* IRuntimeManager methods  */
                virtual Core::hresult Register(Exchange::IRuntimeManager::INotification *notification) override;
                virtual Core::hresult Unregister(Exchange::IRuntimeManager::INotification *notification) override;

                virtual Core::hresult Run(const string& appId, const string& appInstanceId, const uint32_t userId, const uint32_t groupId, IValueIterator* const& ports, IStringIterator* const& paths, IStringIterator* const& debugSettings, const WPEFramework::Exchange::RuntimeConfig& runtimeConfigObject) override;
                virtual Core::hresult Hibernate(const string& appInstanceId) override;
                virtual Core::hresult Wake(const string& appInstanceId, const RuntimeState runtimeState) override;
                virtual Core::hresult Suspend(const string& appInstanceId) override;
                virtual Core::hresult Resume(const string& appInstanceId) override;
                virtual Core::hresult Terminate(const string& appInstanceId) override;
                virtual Core::hresult Kill(const string& appInstanceId) override;
                virtual Core::hresult GetInfo(const string& appInstanceId, string& info) override;
                virtual Core::hresult Annotate(const string& appInstanceId, const string& key, const string& value) override;
                virtual Core::hresult Mount() override;
                virtual Core::hresult Unmount() override;

                // IConfiguration methods
                uint32_t Configure(PluginHost::IShell* service) override;

                // IEventHandler methods
                virtual void onOCIContainerStartedEvent(std::string name, JsonObject& data) override;
                virtual void onOCIContainerStoppedEvent(std::string name, JsonObject& data) override;
                virtual void onOCIContainerFailureEvent(std::string name, JsonObject& data) override;
                virtual void onOCIContainerStateChangedEvent(std::string name, JsonObject& data) override;

            private: /* private methods */
                Core::hresult createOCIContainerPluginObject();
                void releaseOCIContainerPluginObject();
                Core::hresult createStorageManagerPluginObject();
                void releaseStorageManagerPluginObject();
                static bool generate(const ApplicationConfiguration& config, const WPEFramework::Exchange::RuntimeConfig& runtimeConfig, std::string& dobbySpec);
                std::string getContainerId(const string& appInstanceId);
                bool isOCIPluginObjectValid(void);
                Exchange::IRuntimeManager::RuntimeState getRuntimeState(const string& appInstanceId);
                Core::hresult getAppStorageInfo(const string& appId, AppStorageInfo& appStorageInfo);

            private: /* members */
                mutable Core::CriticalSection mRuntimeManagerImplLock;
                PluginHost::IShell* mCurrentservice;
                Exchange::IOCIContainer* mOciContainerObject;
                std::list<Exchange::IRuntimeManager::INotification*> mRuntimeManagerNotification;
                std::map<std::string, RuntimeAppInfo> mRuntimeAppInfo;
                Exchange::IStorageManager *mStorageManagerObject;
                WindowManagerConnector* mWindowManagerConnector;
                DobbyEventListener *mDobbyEventListener;
                UserIdManager* mUserIdManager;
                std::string mRuntimeAppPortal;
#ifdef ENABLE_AIMANAGERS_TELEMETRY_METRICS
                Exchange::ITelemetryMetrics* mTelemetryMetricsObject;
#endif
#ifdef  RIALTO_IN_DAC_FEATURE_ENABLED
                std::shared_ptr<RialtoConnector>  mRialtoConnector;
#endif // RIALTO_IN_DAC_FEATURE_ENABLED

            private: /* internal methods */
                void dispatchEvent(RuntimeEventType, const JsonValue &params);
                void Dispatch(RuntimeEventType event, const JsonValue params);
                void notifyParameterCheckFailure(const string& appInstanceId, const string& errorCode);

#ifdef ENABLE_AIMANAGERS_TELEMETRY_METRICS
                void recordTelemetryData(const std::string& marker, const std::string& appId, uint64_t requestTime);
                TelemetryMarker getTelemetryMarker(const std::string& marker);
                time_t getCurrentTimestamp();
#endif

                friend class Job;

            public/*members*/:
                static RuntimeManagerImplementation* _instance;
        };
    } /* namespace Plugin */
} /* namespace WPEFramework */
