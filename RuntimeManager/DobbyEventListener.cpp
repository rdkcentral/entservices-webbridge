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

#include "DobbyEventListener.h"
#include "UtilsLogging.h"
#include "tracing/Logging.h"
#include <sstream>
#include <interfaces/IRuntimeManager.h>

#define SEND_EVENT_TO_RUNTIME_MANAGER(eventMethod, name, data) \
    if ((_parent.mEventHandler) != nullptr)                 \
    {                                                       \
        (_parent.mEventHandler)->eventMethod(name, data);   \
    }                                                       \
    else                                                    \
    {                                                       \
        LOGERR("Event handler is null");                    \
    }

namespace WPEFramework {
    namespace Plugin {

        DobbyEventListener::DobbyEventListener()
            : mOCIContainerNotification(this),mEventHandler(nullptr)
        {
            LOGINFO("Creating DobbyEventListener instance");
        }

        DobbyEventListener::~DobbyEventListener()
        {
            LOGINFO("Delete DobbyEventListener instance");
        }

        bool DobbyEventListener::initialize(PluginHost::IShell* service, IEventHandler* eventHandler, Exchange::IOCIContainer* ociContainerObject)
        {
            bool ret = false;
            mEventHandler = eventHandler;
            mOCIContainerObject = ociContainerObject;
            if (nullptr == mOCIContainerObject)
            {
                LOGERR("Failed to get OCIContainer object");
            }
            else
            {
                Core::hresult registerResult = mOCIContainerObject->Register(&mOCIContainerNotification);
                if(Core::ERROR_NONE != registerResult)
                {
                    LOGERR("Failed to register OCIContainerNotification");
                    return false;
                }
                else
                {
                    LOGINFO("OCIContainerNotification registered successfully");
                }
                ret = true;
            }
            return ret;
        }

        bool DobbyEventListener::deinitialize()
        {
            if (nullptr != mOCIContainerObject)
            {
                Core::hresult unregisterResult = mOCIContainerObject->Unregister(&mOCIContainerNotification);
                if(Core::ERROR_NONE != unregisterResult)
                {
                    LOGERR("Failed to unregister OCIContainerNotification");
                }
                mOCIContainerObject = nullptr;
            }
            return true;
        }

        void DobbyEventListener::OCIContainerNotification::OnContainerStarted(const string& containerId, const string& name)
        {
            // LOGINFO("OnContainer started: %s", name.c_str());
            JsonObject data;
            data["containerId"] = containerId;
            data["name"] = name;
            data["eventName"] = "onContainerStarted";
            SEND_EVENT_TO_RUNTIME_MANAGER(onOCIContainerStartedEvent, name, data);
        }

        void DobbyEventListener::OCIContainerNotification::OnContainerStopped(const string& containerId, const string& name)
        {
            // LOGINFO("Container stopped: %s", name.c_str());
            JsonObject data;
            data["containerId"] = containerId;
            data["name"] = name;
            data["eventName"] = "onContainerStopped";
            SEND_EVENT_TO_RUNTIME_MANAGER(onOCIContainerStoppedEvent, name, data);
        }

        void DobbyEventListener::OCIContainerNotification::OnContainerFailed(const string& containerId, const string& name, uint32_t error)
        {
            // LOGINFO("Container failed: %s", name.c_str());
            JsonObject data;
            data["containerId"] = containerId;
            data["name"] = name;
            data["errorCode"] = error;
            data["eventName"] = "onContainerFailed";
            SEND_EVENT_TO_RUNTIME_MANAGER(onOCIContainerFailureEvent, name, data);
        }

        void DobbyEventListener::OCIContainerNotification::OnContainerStateChanged(const string& containerId, Exchange::IOCIContainer::ContainerState state)
        {
            Exchange::IRuntimeManager::RuntimeState runtimeState;
            switch(state)
            {
                case Exchange::IOCIContainer::ContainerState::STARTING:
                    runtimeState = Exchange::IRuntimeManager::RUNTIME_STATE_STARTING;
                    break;
                case Exchange::IOCIContainer::ContainerState::RUNNING:
                    runtimeState = Exchange::IRuntimeManager::RUNTIME_STATE_RUNNING;
                    break;
                case Exchange::IOCIContainer::ContainerState::STOPPING:
                    runtimeState = Exchange::IRuntimeManager::RUNTIME_STATE_TERMINATING;
                    break;
                case Exchange::IOCIContainer::ContainerState::PAUSED:
                    runtimeState = Exchange::IRuntimeManager::RUNTIME_STATE_SUSPENDED;
                    break;
                case Exchange::IOCIContainer::ContainerState::STOPPED:
                    runtimeState = Exchange::IRuntimeManager::RUNTIME_STATE_TERMINATED;
                    break;
                case Exchange::IOCIContainer::ContainerState::HIBERNATING:
                    runtimeState = Exchange::IRuntimeManager::RUNTIME_STATE_HIBERNATING;
                    break;
                case Exchange::IOCIContainer::ContainerState::HIBERNATED:
                    runtimeState = Exchange::IRuntimeManager::RUNTIME_STATE_HIBERNATED;
                    break;
                case Exchange::IOCIContainer::ContainerState::AWAKENING:
                    runtimeState = Exchange::IRuntimeManager::RUNTIME_STATE_WAKING;
                    break;
                default:
                    runtimeState = Exchange::IRuntimeManager::RUNTIME_STATE_UNKNOWN;
                    break;
            }
            JsonObject data;
            data["containerId"] = containerId;
            data["state"] = static_cast<int>(runtimeState);
            data["eventName"] = "onContainerStateChanged";
            LOGINFO("Container state changed: %s state: %d runtimeState: %d", containerId.c_str(), static_cast<int>(state), static_cast<int>(runtimeState));
            SEND_EVENT_TO_RUNTIME_MANAGER(onOCIContainerStateChangedEvent, containerId, data);
        }





    }// Namespace Plugin
}// Namespace WPEFramework