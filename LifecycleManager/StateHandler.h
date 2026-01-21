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

#include <interfaces/ILifecycleManager.h>
#include "ApplicationContext.h"
#include "State.h"
#include <list>
#include <map>
#include <string>
#include "StateTransitionRequest.h"

namespace WPEFramework
{
    namespace Plugin
    {
        class StateHandler
	{
            public:
                static void initialize();
	        static bool changeState(StateTransitionRequest& request, string& errorReason);

            private:
                static uint32_t sStateChangeCount;

                static State* createState(ApplicationContext* context, Exchange::ILifecycleManager::LifecycleState lifeCycleState);
                static bool isValidTransition(Exchange::ILifecycleManager::LifecycleState start, Exchange::ILifecycleManager::LifecycleState target, std::map<Exchange::ILifecycleManager::LifecycleState, bool>& pathSequence, std::vector<Exchange::ILifecycleManager::LifecycleState>& foundPath);
	        static bool updateState(ApplicationContext* context, Exchange::ILifecycleManager::LifecycleState lifeCycleState, string& errorReason);
                static void sendEvent(ApplicationContext* context, Exchange::ILifecycleManager::LifecycleState oldLifecycleState, Exchange::ILifecycleManager::LifecycleState newLifecycleState, string& errorReason);
                static bool getStatePath(ApplicationContext* context, Exchange::ILifecycleManager::LifecycleState lifecycleState, std::vector<Exchange::ILifecycleManager::LifecycleState>& statePath, string& errorReason);
                static std::map<Exchange::ILifecycleManager::LifecycleState, std::list<Exchange::ILifecycleManager::LifecycleState>> mPossibleStateTransitions;
                static std::map<Exchange::ILifecycleManager::LifecycleState, std::string> mStateStrings;
        };
    } /* namespace Plugin */
} /* namespace WPEFramework */
