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

#include "ApplicationContext.h"
#include "State.h"

namespace WPEFramework
{
    namespace Plugin
    {
	ApplicationLaunchParams::ApplicationLaunchParams(): mAppId(""), mLaunchIntent(""), mLaunchArgs(""), mTargetState(Exchange::ILifecycleManager::LifecycleState::UNLOADED), mRuntimeConfigObject()
        {
	}

        ApplicationContext::ApplicationContext (std::string appId)
        : mPendingStateTransition(false)
        , mPendingStates()
        , mPendingEventName("")
        , mAppInstanceId("")
        , mAppId(std::move(appId))
        , mLastLifecycleStateChangeTime{0, 0}
        , mActiveSessionId("")
        , mTargetLifecycleState()
        , mMostRecentIntent("")
        , mState(nullptr)
        , mStateChangeId(0)
#ifdef ENABLE_AIMANAGERS_TELEMETRY_METRICS
        , mRequestTime(0)
        , mRequestType(REQUEST_TYPE_NONE)
#endif
        {
            mState = (void*) new UnloadedState(this);
            sem_init(&mReachedLoadingStateSemaphore, 0, 0);
            sem_init(&mAppReadySemaphore, 0, 0);
            sem_init(&mFirstFrameAfterResumeSemaphore, 0, 0);
        }

	ApplicationKillParams::ApplicationKillParams(): mForce(false)
        {
	}

        ApplicationContext::~ApplicationContext()
        {
            if (nullptr != mState)
	    {
                State* state = (State*)mState;
                delete state;
	    }
	    mState = nullptr;
        }

        void ApplicationContext::setAppInstanceId(std::string& id)
        {
            mAppInstanceId = id;
        }

        void ApplicationContext::setActiveSessionId(std::string& id)
        {
            mActiveSessionId = id;
        }

        void ApplicationContext::setMostRecentIntent(const std::string& intent)
        {
            mMostRecentIntent = intent;
        }

        void ApplicationContext::setLastLifecycleStateChangeTime(timespec changeTime)
	{
            mLastLifecycleStateChangeTime = changeTime;
	}

        void ApplicationContext::setState(void* state)
	{
            mState = state;
	}

	void ApplicationContext::setTargetLifecycleState(Exchange::ILifecycleManager::LifecycleState state)
	{
            mTargetLifecycleState = state;
        }

        void ApplicationContext::setStateChangeId(uint32_t id)
	{
            mStateChangeId = id;		
	}

        void ApplicationContext::setApplicationLaunchParams(const string& appId, const string& launchIntent, const string& launchArgs, Exchange::ILifecycleManager::LifecycleState targetState, const WPEFramework::Exchange::RuntimeConfig& runtimeConfigObject)
	{
            mLaunchParams.mAppId = appId;
            mLaunchParams.mLaunchIntent = launchIntent;
            mLaunchParams.mLaunchArgs = launchArgs;
            mLaunchParams.mTargetState = targetState;
            mLaunchParams.mRuntimeConfigObject = runtimeConfigObject;
	}

        void ApplicationContext::setApplicationKillParams(bool force)
	{
            mKillParams.mForce = force;
        }

        void ApplicationContext::setRequestTime(time_t requestTime)
        {
#ifdef ENABLE_AIMANAGERS_TELEMETRY_METRICS
            mRequestTime = requestTime;
#endif
        }
        void ApplicationContext::setRequestType(RequestType requestType)
        {
#ifdef ENABLE_AIMANAGERS_TELEMETRY_METRICS
            mRequestType = requestType;
#endif
        }


	std::string ApplicationContext::getAppId()
	{
            return mAppId;
	}

	std::string ApplicationContext::getAppInstanceId()
	{
            return mAppInstanceId;
	}

	Exchange::ILifecycleManager::LifecycleState ApplicationContext::getCurrentLifecycleState()
	{
            State* state = (State*)mState;
            return state->getValue();
	}

        timespec ApplicationContext::getLastLifecycleStateChangeTime()
	{
	    return mLastLifecycleStateChangeTime;	
	}

        std::string ApplicationContext::getActiveSessionId()
	{
            return mActiveSessionId;
	}

	Exchange::ILifecycleManager::LifecycleState ApplicationContext::getTargetLifecycleState()
	{
            return mTargetLifecycleState;
        }

        std::string ApplicationContext::getMostRecentIntent()
        {
            return mMostRecentIntent;
        }

        void* ApplicationContext::getState()
        {
            return mState;
        }

        uint32_t ApplicationContext::getStateChangeId()
	{
            return mStateChangeId;
	}

	ApplicationLaunchParams& ApplicationContext::getApplicationLaunchParams()
	{
            return mLaunchParams;
	}

	ApplicationKillParams& ApplicationContext::getApplicationKillParams()
	{
            return mKillParams;
	}

        time_t ApplicationContext::getRequestTime()
        {
#ifdef ENABLE_AIMANAGERS_TELEMETRY_METRICS
            return mRequestTime;
#else
            return 0;
#endif
        }

        RequestType ApplicationContext::getRequestType()
        {
#ifdef ENABLE_AIMANAGERS_TELEMETRY_METRICS
            return mRequestType;
#else
            return REQUEST_TYPE_NONE;
#endif
        }
    } /* namespace Plugin */
} /* namespace WPEFramework */
