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
#include <interfaces/IOCIContainer.h>
#include "UtilsLogging.h"
#include "tracing/Logging.h"
#include <mutex>
#include "DobbyInterface.h"
#include "IEventHandler.h"

namespace WPEFramework
{
namespace Plugin
{
    class OCIContainerImplementation : public Exchange::IOCIContainer, public IEventHandler
    {
        public:
            enum EventNames
            {
                OCICONTAINER_EVENT_CONTAINER_STARTED,
                OCICONTAINER_EVENT_CONTAINER_STOPPED,
                OCICONTAINER_EVENT_CONTAINER_FAILED,
                OCICONTAINER_EVENT_STATE_CHANGED
            };
            class EXTERNAL Job : public Core::IDispatch
            {
                protected:
                        Job(OCIContainerImplementation *runtimeManagerImplementation, EventNames event, JsonObject &params)
                        : mOCIContainerImplementation(runtimeManagerImplementation)
                        , _event(event)
                        , _params(params)
                        {
                            if (mOCIContainerImplementation != nullptr)
                            {
                                mOCIContainerImplementation->AddRef();
                            }
                        }

                public:
                        Job() = delete;
                        Job(const Job&) = delete;
                        Job& operator=(const Job&) = delete;
                        ~Job()
                        {
                            if (mOCIContainerImplementation != nullptr)
                            {
                                mOCIContainerImplementation->Release();
                            }
                        }

                public:
                        static Core::ProxyType<Core::IDispatch> Create(OCIContainerImplementation *runtimeManagerImplementation, EventNames event, JsonObject params)
                        {
#ifndef  USE_THUNDER_R4
                            return (Core::proxy_cast<Core::IDispatch>(Core::ProxyType<Job>::Create(runtimeManagerImplementation, event, params)));
#else
                            return (Core::ProxyType<Core::IDispatch>(Core::ProxyType<Job>::Create(runtimeManagerImplementation, event, params)));
#endif
                        }

                        virtual void Dispatch()
                        {
                            mOCIContainerImplementation->Dispatch(_event, _params);
                        }
                private:
                    OCIContainerImplementation *mOCIContainerImplementation;
                    const EventNames _event;
                    const JsonObject _params;
            };

            OCIContainerImplementation ();
            ~OCIContainerImplementation () override;
            OCIContainerImplementation (const OCIContainerImplementation &) = delete;
            OCIContainerImplementation & operator=(const OCIContainerImplementation &) = delete;

            BEGIN_INTERFACE_MAP(OCIContainerImplementation)
            INTERFACE_ENTRY(Exchange::IOCIContainer)
            END_INTERFACE_MAP

            /* IOCIContainer methods  */
            virtual Core::hresult Register(Exchange::IOCIContainer::INotification *notification) override;
            virtual Core::hresult Unregister(Exchange::IOCIContainer::INotification *notification) override;
            virtual Core::hresult ListContainers(string& containers, bool& success, string& errorReason) override;
            virtual Core::hresult GetContainerInfo(const string& containerId, string& info, bool& success, string& errorReason) override;
            virtual Core::hresult GetContainerState(const string& containerId, ContainerState& state, bool& success, string& errorReason) override;
            virtual Core::hresult StartContainer(const string& containerId, const string& bundlePath, const string& command, const string& westerosSocket, int32_t& descriptor, bool& success, string& errorReason) override;
            virtual Core::hresult StartContainerFromDobbySpec(const string& containerId, const string& dobbySpec, const string& command, const string& westerosSocket, int32_t& descriptor, bool& success, string& errorReason) override;
            virtual Core::hresult StopContainer(const string& containerId, bool force, bool& success, string& errorReason) override;
            virtual Core::hresult PauseContainer(const string& containerId, bool& success, string& errorReason) override;
            virtual Core::hresult ResumeContainer(const string& containerId, bool& success, string& errorReason) override;
            virtual Core::hresult HibernateContainer(const string& containerId, const string& options, bool& success, string& errorReason) override;
            virtual Core::hresult WakeupContainer(const string& containerId, bool& success, string& errorReason) override;
            virtual Core::hresult ExecuteCommand(const string& containerId, const string& options, const string& command, bool& success ,string& errorReason) override;
            virtual Core::hresult Annotate(const string& containerId, const string& key, const string& value, bool& success, string& errorReason) override;
            virtual Core::hresult RemoveAnnotation(const string& containerId, const string& key, bool& success, string& errorReason) override;
            virtual Core::hresult Mount(const string& containerId, const string& source, const string& target, const string& type, const string& options, bool& success ,string& errorReason) override;
            virtual Core::hresult Unmount(const string& containerId, const string& target, bool& success, string& errorReason) override;

            virtual void onContainerStarted(JsonObject& data) override;
            virtual void onContainerStopped(JsonObject& data) override;
            virtual void onContainerFailed(JsonObject& data) override;
            virtual void onContainerStateChange(JsonObject& data) override;

        private: /* members */
            mutable Core::CriticalSection mAdminLock;
            std::list<Exchange::IOCIContainer::INotification*> mOCIContainerNotification;
            DobbyInterface* mDobbyInterface;

        private: /* internal methods */
            void dispatchEvent(EventNames, const JsonObject &params);
            void Dispatch(EventNames event, const JsonObject params);

            friend class Job;
    };
} /* namespace Plugin */
} /* namespace WPEFramework */
