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
#include <map>

//class ApplicationContext;

namespace WPEFramework
{
    namespace Plugin
    {
        class State
	{
            public:
	        State(ApplicationContext* context, Exchange::ILifecycleManager::LifecycleState state): mState(state), mContext(context) {}
                virtual ~State() {}

                virtual bool handle(string& errorReason)
		{
                    return true;
		}

                virtual Exchange::ILifecycleManager::LifecycleState getValue()
		{
                    return mState;
		}

                virtual ApplicationContext* getContext()
		{
                    return mContext;
		}

	    private: /* members */
                Exchange::ILifecycleManager::LifecycleState mState; 
                ApplicationContext* mContext;
        };

        class UnloadedState : public State
	{
            public:
                UnloadedState(ApplicationContext* context): State(context, Exchange::ILifecycleManager::LifecycleState::UNLOADED) {}
                ~UnloadedState() {}
                bool handle(string& errorReason);
        };

        class LoadingState : public State
	{
            public:
                LoadingState(ApplicationContext* context): State(context, Exchange::ILifecycleManager::LifecycleState::LOADING) {}
                ~LoadingState() {}
                bool handle(string& errorReason);
        };

        class InitializingState : public State
	{
            public:
                InitializingState(ApplicationContext* context): State(context, Exchange::ILifecycleManager::LifecycleState::INITIALIZING) {}
                ~InitializingState() {}
                bool handle(string& errorReason);
        };

        class PausedState : public State
	{
            public:
                PausedState(ApplicationContext* context): State(context, Exchange::ILifecycleManager::LifecycleState::PAUSED) {}
                ~PausedState() {}
                bool handle(string& errorReason);
        };

        class ActiveState : public State
	{
            public:
                ActiveState(ApplicationContext* context): State(context, Exchange::ILifecycleManager::LifecycleState::ACTIVE) {}
                ~ActiveState() {}
                bool handle(string& errorReason);
        };

        class SuspendedState : public State
	{
            public:
                SuspendedState(ApplicationContext* context): State(context, Exchange::ILifecycleManager::LifecycleState::SUSPENDED) {}
                ~SuspendedState() {}
                bool handle(string& errorReason);
        };

        class HibernatedState : public State
	{
            public:
                HibernatedState(ApplicationContext* context): State(context, Exchange::ILifecycleManager::LifecycleState::HIBERNATED) {}
                ~HibernatedState() {}
                bool handle(string& errorReason);
        };

        class TerminatingState : public State
	{
            public:
                TerminatingState(ApplicationContext* context): State(context, Exchange::ILifecycleManager::LifecycleState::TERMINATING) {}
                ~TerminatingState() {}
                bool handle(string& errorReason);
        };
    } /* namespace Plugin */
} /* namespace WPEFramework */
