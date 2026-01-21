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

#include <Dobby/DobbyProtocol.h>
#include <Dobby/Public/Dobby/IDobbyProxy.h>
#include <interfaces/IOCIContainer.h>
#include <vector>
#include <map>
#include <omi_proxy.hpp>
#include "IEventHandler.h"

namespace WPEFramework
{
namespace Plugin
{
    class DobbyInterface
    {
        public:
            DobbyInterface();
            virtual ~DobbyInterface();
            DobbyInterface(const DobbyInterface &) = delete;
            DobbyInterface &operator=(const DobbyInterface &) = delete;
        
            bool initialize(IEventHandler* eventHandler);
            void terminate();

            bool listContainers(string& containers, string& errorReason);
            bool getContainerInfo(const string& containerId, string& info, string& errorReason);
            bool getContainerState(const string& containerId, Exchange::IOCIContainer::ContainerState& state, string& errorReason);
            bool startContainer(const string& containerId, const string& bundlePath, const string& command, const string& westerosSocket, int32_t& descriptor, string& errorReason);
            bool startContainerFromDobbySpec(const string& containerId, const string& dobbySpec, const string& command, const string& westerosSocket, int32_t& descriptor, string& errorReason);
            bool stopContainer(const string& containerId, bool force, string& errorReason);
            bool pauseContainer(const string& containerId, string& errorReason);
            bool resumeContainer(const string& containerId, string& errorReason);
            bool hibernateContainer(const string& containerId, const string& options, string& errorReason);
            bool wakeupContainer(const string& containerId, string& errorReason);
            bool executeCommand(const string& containerId, const string& options, const string& command, string& errorReason);
            bool annotate(const string& containerId, const string& key, const string& value, string& errorReason);
            bool removeAnnotation(const string& containerId, const string& key, string& errorReason);
            bool mount(const string& containerId, const string& source, const string& target, const string& type, const string& options, string& errorReason);
            bool unmount(const string& containerId, const string& target, string& errorReason);
        
            void onContainerStarted(int32_t descriptor, const std::string& name);
            void onContainerStopped(int32_t descriptor, const std::string& name);
            void onContainerStateChanged(int32_t descriptor, const std::string& name, IDobbyProxyEvents::ContainerState dobbyContainerState);
            void onVerityFailed(const std::string& name);
        
        private:
            int mEventListenerId; // Dobby event listener ID
            long unsigned mOmiListenerId;
            std::shared_ptr<IDobbyProxy> mDobbyProxy; // DobbyProxy instance
            std::shared_ptr<AI_IPC::IIpcService> mIpcService; // Ipc Service instance
            const int GetContainerDescriptorFromId(const std::string& containerId);
            const std::string GetContainerIdFromDescriptor(const int descriptor);
            static const void stateListener(int32_t descriptor, const std::string& name, IDobbyProxyEvents::ContainerState state, const void* _this);
            static const void omiErrorListener(const std::string& id, omi::IOmiProxy::ErrorType err, const void* _this);
            std::shared_ptr<omi::IOmiProxy> mOmiProxy;
            IEventHandler* mEventHandler;
    };
} // namespace Plugin
} // namespace WPEFramework
