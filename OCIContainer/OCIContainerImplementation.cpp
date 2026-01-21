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

#include "OCIContainerImplementation.h"

namespace WPEFramework
{
    namespace Plugin
    {
        SERVICE_REGISTRATION(OCIContainerImplementation, 1, 0);

        OCIContainerImplementation::OCIContainerImplementation(): mDobbyInterface(nullptr)
        {
            LOGINFO("Create OCIContainerImplementation Instance");
            mDobbyInterface = new DobbyInterface();
            mDobbyInterface->initialize(this);
        }

        OCIContainerImplementation::~OCIContainerImplementation()
        {
            LOGINFO("Delete OCIContainerImplementation Instance");
            mDobbyInterface->terminate();
            delete mDobbyInterface;
            mDobbyInterface = nullptr;
        }

        Core::hresult OCIContainerImplementation::Register(Exchange::IOCIContainer::INotification *notification)
        {
            ASSERT (nullptr != notification);
        
            mAdminLock.Lock();
        
            /* Make sure we can't register the same notification callback multiple times */
            if (std::find(mOCIContainerNotification.begin(), mOCIContainerNotification.end(), notification) == mOCIContainerNotification.end())
            {
                LOGINFO("Register notification");
                mOCIContainerNotification.push_back(notification);
                notification->AddRef();
            }
        
            mAdminLock.Unlock();
        
            return Core::ERROR_NONE;
        }
        
        Core::hresult OCIContainerImplementation::Unregister(Exchange::IOCIContainer::INotification *notification)
        {
            Core::hresult status = Core::ERROR_GENERAL;
        
            ASSERT (nullptr != notification);
        
            mAdminLock.Lock();
        
            /* Make sure we can't unregister the same notification callback multiple times */
            auto itr = std::find(mOCIContainerNotification.begin(), mOCIContainerNotification.end(), notification);
            if (itr != mOCIContainerNotification.end())
            {
                (*itr)->Release();
                LOGINFO("Unregister notification");
                mOCIContainerNotification.erase(itr);
                status = Core::ERROR_NONE;
            }
            else
            {
                LOGERR("notification not found");
            }
        
            mAdminLock.Unlock();
        
            return status;
        }
        
        void OCIContainerImplementation::dispatchEvent(EventNames event, const JsonObject &params)
        {
            Core::IWorkerPool::Instance().Submit(Job::Create(this, event, params));
        }
        
        void OCIContainerImplementation::Dispatch(EventNames event, const JsonObject params)
        {
             //TODO: Convert params to proper values and pass
             mAdminLock.Lock();
             string containerId("");
             string name("");
             int state = 0;
             uint32_t error = 0;
             std::list<Exchange::IOCIContainer::INotification*>::const_iterator index(mOCIContainerNotification.begin());
             if(params.HasLabel("containerId"))
             {
                 containerId = params["containerId"].String();
             }
             if(params.HasLabel("name"))
             {
                 name = params["name"].String();
             }
             if(params.HasLabel("state"))
             {
                 state = params["state"].Number();
             }
             if(params.HasLabel("errorCode"))
             {
                 error = params["errorCode"].Number();
             }
             switch(event)
             {
                 case OCICONTAINER_EVENT_CONTAINER_STARTED:
                    while (index != mOCIContainerNotification.end())
                    {
                         if (*index != nullptr) {
                             (*index)->OnContainerStarted(containerId, name);
                         }
                         ++index;
                    }
                    break;
                 case OCICONTAINER_EVENT_CONTAINER_STOPPED:
                    while (index != mOCIContainerNotification.end())
                    {
                         if (*index != nullptr) {
                             (*index)->OnContainerStopped(containerId, name);
                         }
                         ++index;
                    }
                    break;
                 case OCICONTAINER_EVENT_CONTAINER_FAILED:
                    while (index != mOCIContainerNotification.end())
                    {
                         if (*index != nullptr) {
                             (*index)->OnContainerFailed(containerId, name, error);
                         }
                         ++index;
                    }
                    break;
                 case OCICONTAINER_EVENT_STATE_CHANGED:
                    while (index != mOCIContainerNotification.end())
                    {
                        if (*index != nullptr) {
                            ContainerState containerState = static_cast<ContainerState>(state);
                            (*index)->OnContainerStateChanged(containerId, containerState);
                        }
                        ++index;
                    }
                    break;
        
                 default:
                    LOGWARN("Event[%u] not handled", event);
                    break;
             }
        
             mAdminLock.Unlock();
        }
        

        Core::hresult OCIContainerImplementation::ListContainers(string& containers, bool& success, string& errorReason)
        {
            success = mDobbyInterface->listContainers(containers, errorReason);

            return Core::ERROR_NONE;
        }

        Core::hresult OCIContainerImplementation::GetContainerInfo(const string& containerId, string& info, bool& success, string& errorReason)
        {
            success = mDobbyInterface->getContainerInfo(containerId, info, errorReason);

            return Core::ERROR_NONE;
        }

        Core::hresult OCIContainerImplementation::GetContainerState(const string& containerId, ContainerState& state, bool& success, string& errorReason)
        {
            success = mDobbyInterface->getContainerState(containerId, state, errorReason);

            return Core::ERROR_NONE;
        }

        Core::hresult OCIContainerImplementation::StartContainer(const string& containerId, const string& bundlePath, const string& command, const string& westerosSocket, int32_t& descriptor, bool& success, string& errorReason)
        {
            success = mDobbyInterface->startContainer(containerId, bundlePath, command, westerosSocket, descriptor, errorReason);

            return Core::ERROR_NONE;
        }

        Core::hresult OCIContainerImplementation::StartContainerFromDobbySpec(const string& containerId, const string& dobbySpec, const string& command, const string& westerosSocket, int32_t& descriptor, bool& success, string& errorReason)
        {
            success = mDobbyInterface->startContainerFromDobbySpec(containerId, dobbySpec, command, westerosSocket, descriptor, errorReason);

            return Core::ERROR_NONE;
        }

        Core::hresult OCIContainerImplementation::StopContainer(const string& containerId, bool force, bool& success, string& errorReason)
        {
            success = mDobbyInterface->stopContainer(containerId, force, errorReason);

            return Core::ERROR_NONE;
        }

        Core::hresult OCIContainerImplementation::PauseContainer(const string& containerId, bool& success, string& errorReason)
        {
            success = mDobbyInterface->pauseContainer(containerId, errorReason);

            return Core::ERROR_NONE;
        }

        Core::hresult OCIContainerImplementation::ResumeContainer(const string& containerId, bool& success, string& errorReason)
        {
            success = mDobbyInterface->resumeContainer(containerId, errorReason);

            return Core::ERROR_NONE;
        }

        Core::hresult OCIContainerImplementation::HibernateContainer(const string& containerId, const string& options, bool& success, string& errorReason)
        {
            success = mDobbyInterface->hibernateContainer(containerId, options, errorReason);

            return Core::ERROR_NONE;
        }

        Core::hresult OCIContainerImplementation::WakeupContainer(const string& containerId, bool& success, string& errorReason)
        {
            success = mDobbyInterface->wakeupContainer(containerId, errorReason);

            return Core::ERROR_NONE;
        }

        Core::hresult OCIContainerImplementation::ExecuteCommand(const string& containerId, const string& options, const string& command, bool& success ,string& errorReason)
        {
            success = mDobbyInterface->executeCommand(containerId, options, command, errorReason);

            return Core::ERROR_NONE;
        }

        Core::hresult OCIContainerImplementation::Annotate(const string& containerId, const string& key, const string& value, bool& success, string& errorReason)
        {
            success = mDobbyInterface->annotate(containerId, key, value, errorReason);

            return Core::ERROR_NONE;
        }

        Core::hresult OCIContainerImplementation::RemoveAnnotation(const string& containerId, const string& key, bool& success, string& errorReason)
        {
            success = mDobbyInterface->removeAnnotation(containerId, key, errorReason);

            return Core::ERROR_NONE;
        }

        Core::hresult OCIContainerImplementation::Mount(const string& containerId, const string& source, const string& target, const string& type, const string& options, bool& success ,string& errorReason)
        {
            success = mDobbyInterface->mount(containerId, source, target, type, options, errorReason);

            return Core::ERROR_NONE;
        }

        Core::hresult OCIContainerImplementation::Unmount(const string& containerId, const string& target, bool& success, string& errorReason)
        {
            success = mDobbyInterface->unmount(containerId, target, errorReason);

            return Core::ERROR_NONE;
        }

        void OCIContainerImplementation::onContainerStarted(JsonObject& data)
        {
            dispatchEvent(OCIContainerImplementation::EventNames::OCICONTAINER_EVENT_CONTAINER_STARTED, data);
        }

        void OCIContainerImplementation::onContainerStopped(JsonObject& data)
        {
            dispatchEvent(OCIContainerImplementation::EventNames::OCICONTAINER_EVENT_CONTAINER_STOPPED, data);
        }

        void OCIContainerImplementation::onContainerFailed(JsonObject& data)
        {
            dispatchEvent(OCIContainerImplementation::EventNames::OCICONTAINER_EVENT_CONTAINER_FAILED, data);
        }

        void OCIContainerImplementation::onContainerStateChange(JsonObject& data)
        {
            dispatchEvent(OCIContainerImplementation::EventNames::OCICONTAINER_EVENT_STATE_CHANGED, data);
        }

    } /* namespace Plugin */
} /* namespace WPEFramework */
