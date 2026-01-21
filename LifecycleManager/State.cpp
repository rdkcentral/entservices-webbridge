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

#include "StateHandler.h"
#include <interfaces/IRDKWindowManager.h>
#include "RuntimeManagerHandler.h"
#include "RequestHandler.h"
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

namespace WPEFramework
{
    namespace Plugin
    {
        bool UnloadedState::handle(string& errorReason)
	{
            return true;
	}

        bool LoadingState::handle(string& errorReason)
	{
            ApplicationContext* context = getContext();

            boost::uuids::uuid tag = boost::uuids::random_generator()();
            std::string generatedInstanceId =  boost::uuids::to_string(tag);
            context->setAppInstanceId(generatedInstanceId);
            sem_post(&context->mReachedLoadingStateSemaphore);
            return true;
	}

        bool InitializingState::handle(string& errorReason)
        {
            RuntimeManagerHandler* runtimeManagerHandler = RequestHandler::getInstance()->getRuntimeManagerHandler();
            bool ret = false;
	    if (nullptr != runtimeManagerHandler)
	    {
                ApplicationContext* context = getContext();
                ApplicationLaunchParams& launchParams = context->getApplicationLaunchParams();
                ret = runtimeManagerHandler->run(context->getAppId(), context->getAppInstanceId(), launchParams.mLaunchArgs, launchParams.mTargetState, launchParams.mRuntimeConfigObject, errorReason);
                ret = true;
                context->mPendingEventName = "onAppRunning";
                context->mPendingStateTransition = true;
	    }
            return ret;
        }

        bool PausedState::handle(string& errorReason)
	{
	    bool ret = false;
            ApplicationContext* context = getContext();
            if (Exchange::ILifecycleManager::LifecycleState::INITIALIZING == context->getCurrentLifecycleState())
	    {
                //TODO : Remove wait for now
                //context->mPendingEventName = "";
                //context->mPendingStateTransition = true;
                //sem_wait(&context->mAppReadySemaphore);
                ret = true;
	    }
	    else if (Exchange::ILifecycleManager::LifecycleState::SUSPENDED == context->getCurrentLifecycleState())
	    {
                RuntimeManagerHandler* runtimeManagerHandler = RequestHandler::getInstance()->getRuntimeManagerHandler();
	        if (nullptr != runtimeManagerHandler)
	        {
                    //ret = runtimeManagerHandler->resume(context->getAppInstanceId(), errorReason);
                    WindowManagerHandler *windowManagerHandler = RequestHandler::getInstance()->getWindowManagerHandler();
                    if (nullptr != windowManagerHandler)
                    {
                        ApplicationContext *context = getContext();
                        Core::hresult retValue = windowManagerHandler->enableDisplayRender(context->getAppInstanceId(), true);
                        printf("enabled display in window manager [%d] \n", retValue);
                        fflush(stdout);
                    }
                   ret = true;
                   //TODO: Error cases
	        }
            }
	    else if (Exchange::ILifecycleManager::LifecycleState::ACTIVE == context->getCurrentLifecycleState())
	    {
                ret = true;		    
            }
            return ret;
	}

        bool ActiveState::handle(string& errorReason)
	{
            WindowManagerHandler* windowManagerHandler = RequestHandler::getInstance()->getWindowManagerHandler();
	    if (nullptr != windowManagerHandler)
	    {
                ApplicationContext* context = getContext();
                bool isRenderReady = false;
		Core::hresult ret = windowManagerHandler->renderReady(context->getAppInstanceId(), isRenderReady);
                if (Core::ERROR_NONE == ret)
		{
                    if (isRenderReady)
		    {
		        return true;	
		    }
		    else
		    {
                        context->mPendingEventName = "onFirstFrame";
                        context->mPendingStateTransition = true;
		    }
                }
		else
		{
                    return false;
                }
	    }
            return true;
	}

        bool SuspendedState::handle(string& errorReason)
	{
	    bool ret = false;
            //TODO : Remove wait for now
            //ApplicationContext* context = getContext();
            //sem_wait(&context->mAppReadySemaphore);
            RuntimeManagerHandler* runtimeManagerHandler = RequestHandler::getInstance()->getRuntimeManagerHandler();
	    if (nullptr != runtimeManagerHandler)
	    {
                ApplicationContext* context = getContext();
                if (Exchange::ILifecycleManager::LifecycleState::HIBERNATED == context->getCurrentLifecycleState())
	        {
                    ret = runtimeManagerHandler->wake(context->getAppInstanceId(), Exchange::ILifecycleManager::LifecycleState::SUSPENDED, errorReason);
	        }
                else
	        {
                    //ret = runtimeManagerHandler->suspend(context->getAppInstanceId(), errorReason);
                    WindowManagerHandler *windowManagerHandler = RequestHandler::getInstance()->getWindowManagerHandler();
                    if (nullptr != windowManagerHandler)
                    {
                        ApplicationContext *context = getContext();
                        Core::hresult retValue = windowManagerHandler->enableDisplayRender(context->getAppInstanceId(), false);
                        printf("disabled display in window manager [%d] \n", retValue);
                        fflush(stdout);
                        ret = true;
                    }
                   //TODO: Handle error cases
                }
	    }
            return ret;
	}

        bool HibernatedState::handle(string& errorReason)
	{
            bool ret = false;
            RuntimeManagerHandler* runtimeManagerHandler = RequestHandler::getInstance()->getRuntimeManagerHandler();
	    if (nullptr != runtimeManagerHandler)
	    {
                ApplicationContext* context = getContext();
                ret = runtimeManagerHandler->hibernate(context->getAppInstanceId(), errorReason);
	    }
            return ret;
	}
/*
        bool WakeRequestedState::handle(string& errorReason)
        {
            bool ret = false;
            RuntimeManagerHandler* runtimeManagerHandler = RequestHandler::getInstance()->getRuntimeManagerHandler();
            if (nullptr != runtimeManagerHandler)
            {
                ApplicationContext* context = getContext();
                ret = runtimeManagerHandler->wake(context->getAppInstanceId(), context->getTargetLifecycleState(), errorReason);
            }
            return ret;
        }
*/

        bool TerminatingState::handle(string& errorReason)
        {
            bool success = false;
            RuntimeManagerHandler* runtimeManagerHandler = RequestHandler::getInstance()->getRuntimeManagerHandler();
            if (nullptr != runtimeManagerHandler)
            {
                ApplicationContext* context = getContext();
                ApplicationKillParams& killParams = context->getApplicationKillParams();
                if (killParams.mForce)
                {
                    success = runtimeManagerHandler->kill(context->getAppInstanceId(), errorReason);
                }
                else
                {
                    success = runtimeManagerHandler->terminate(context->getAppInstanceId(), errorReason);
                }
                if(success)
                {
                    context->mPendingEventName = "onAppTerminating";
                    context->mPendingStateTransition = true;
                }
            }
            return success;
        }
    }
} /* namespace WPEFramework */
