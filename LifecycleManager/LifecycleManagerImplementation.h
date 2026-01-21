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
#include <interfaces/ILifecycleManager.h>
#include <interfaces/ILifecycleManagerState.h>
#include <interfaces/IConfiguration.h>
#include "IEventHandler.h"
#include "UtilsLogging.h"
#include "tracing/Logging.h"
#include <mutex>
#include "ApplicationContext.h"

namespace WPEFramework
{
    namespace Plugin
    {
        class LifecycleManagerImplementation : public Exchange::ILifecycleManager
					     , public Exchange::ILifecycleManagerState
					     , public Exchange::IConfiguration
					     , public IEventHandler
	{
            public:
                enum EventNames
                {
                    LIFECYCLE_MANAGER_EVENT_APPSTATECHANGED,
                    LIFECYCLE_MANAGER_EVENT_RUNTIME,
                    LIFECYCLE_MANAGER_EVENT_WINDOW,
                    LIFECYCLE_MANAGER_EVENT_ONFAILURE
                };

                class EXTERNAL Job : public Core::IDispatch
	        {
                    protected:
                         Job(LifecycleManagerImplementation *lifeCycleManagerImplementation, EventNames event, JsonValue &params)
                            : mLifecycleManagerImplementation(lifeCycleManagerImplementation)
                            , _event(event)
                            , _params(params)
	                {
                            if (mLifecycleManagerImplementation != nullptr)
	            	    {
                                mLifecycleManagerImplementation->AddRef();
                            }
                        }

                    public:
                         Job() = delete;
                         Job(const Job&) = delete;
                         Job& operator=(const Job&) = delete;
                         ~Job()
	                 {
                             if (mLifecycleManagerImplementation != nullptr)
	            	     {
                                 mLifecycleManagerImplementation->Release();
                             }
                         }

                    public:
                         static Core::ProxyType<Core::IDispatch> Create(LifecycleManagerImplementation *lifeCycleManagerImplementation, EventNames event, JsonValue params)
	                 {
#ifndef  USE_THUNDER_R4
                             return (Core::proxy_cast<Core::IDispatch>(Core::ProxyType<Job>::Create(lifeCycleManagerImplementation, event, params)));
#else
                             return (Core::ProxyType<Core::IDispatch>(Core::ProxyType<Job>::Create(lifeCycleManagerImplementation, event, params)));
#endif
                         }

                         virtual void Dispatch()
	                 {
                             mLifecycleManagerImplementation->Dispatch(_event, _params);
                         }
                    private:
                        LifecycleManagerImplementation *mLifecycleManagerImplementation;
                        const EventNames _event;
                        const JsonValue _params;
                };

                LifecycleManagerImplementation ();
                ~LifecycleManagerImplementation () override;
                LifecycleManagerImplementation (const LifecycleManagerImplementation &) = delete;
                LifecycleManagerImplementation & operator=(const LifecycleManagerImplementation &) = delete;

		BEGIN_INTERFACE_MAP(LifecycleManagerImplementation)
		INTERFACE_ENTRY(Exchange::ILifecycleManager)
                INTERFACE_ENTRY(Exchange::ILifecycleManagerState)
                INTERFACE_ENTRY(Exchange::IConfiguration)
	        END_INTERFACE_MAP

                /* ILifecycleManager methods  */
                virtual Core::hresult Register(Exchange::ILifecycleManager::INotification *notification) override;
                virtual Core::hresult Unregister(Exchange::ILifecycleManager::INotification *notification) override;
                virtual Core::hresult GetLoadedApps(const bool verbose, string& apps) override;
                virtual Core::hresult IsAppLoaded(const string& appId, bool& loaded) const override;
                virtual Core::hresult SpawnApp(const string& appId, const string& launchIntent, const Exchange::ILifecycleManager::LifecycleState targetLifecycleState, const WPEFramework::Exchange::RuntimeConfig& runtimeConfigObject, const string& launchArgs, string& appInstanceId, string& errorReason, bool& success) override;
                virtual Core::hresult SetTargetAppState(const string& appInstanceId, const Exchange::ILifecycleManager::LifecycleState targetLifecycleState, const string& launchIntent) override;
                virtual Core::hresult UnloadApp(const string& appInstanceId, string& errorReason, bool& success) override;
                virtual Core::hresult KillApp(const string& appInstanceId, string& errorReason, bool& success) override;
                virtual Core::hresult SendIntentToActiveApp(const string& appInstanceId, const string& intent, string& errorReason, bool& success) override;

                /* ILifecycleManagerState methods  */
                /** Register notification interface */
                virtual Core::hresult Register(Exchange::ILifecycleManagerState::INotification *notification) override;
                /** Unregister notification interface */
                virtual Core::hresult Unregister(Exchange::ILifecycleManagerState::INotification *notification) override;
		virtual Core::hresult AppReady(const string& appId) override;
		virtual Core::hresult StateChangeComplete(const string& appId, const uint32_t stateChangedId, const bool success) override;
		virtual Core::hresult CloseApp(const string& appId, const AppCloseReason closeReason) override;

                /* IConfiguration methods */
                uint32_t Configure(PluginHost::IShell* service) override;

                /* IEventHandler methods  */
                virtual void onRuntimeManagerEvent(JsonObject& data) override;
                virtual void onWindowManagerEvent(JsonObject& data) override;
                virtual void onRippleEvent(string name, JsonObject& data) override;
                virtual void onStateChangeEvent(JsonObject& data) override;

	    private: /* members */
                mutable Core::CriticalSection mAdminLock;
	        std::list<Exchange::ILifecycleManager::INotification*> mLifecycleManagerNotification;
	        std::list<Exchange::ILifecycleManagerState::INotification*> mLifecycleManagerStateNotification;
                std::list<ApplicationContext*> mLoadedApplications;
                PluginHost::IShell* mService;
	    private: /* internal methods */
                bool initialize(PluginHost::IShell* service);
                void terminate();
                void dispatchEvent(EventNames, const JsonValue &params);
                void Dispatch(EventNames event, const JsonValue params);
                void handleRuntimeManagerEvent(const JsonObject &data);
                void notifyOnFailure(const string& appInstanceId, const string& errorCode);
                void handleStateChangeEvent(const JsonObject &data);
                void handleWindowManagerEvent(const JsonObject &data);
                ApplicationContext* getContext(const string& appInstanceId, const string& appId) const;
                void addStateTransitionRequest(ApplicationContext* context, std::string event);

                friend class Job;

#ifdef UNIT_TEST                
                friend class LifecycleManagerImplementationTest;
#endif
        };
    } /* namespace Plugin */
} /* namespace WPEFramework */
