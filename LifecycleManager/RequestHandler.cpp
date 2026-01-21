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

#include "RequestHandler.h"
#include "StateTransitionHandler.h"
#include <interfaces/IRDKWindowManager.h>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

namespace WPEFramework
{
    namespace Plugin
    {
        RequestHandler* RequestHandler::mInstance = nullptr;

        RequestHandler* RequestHandler::getInstance()
	{
            if (nullptr == mInstance)
            {
                mInstance = new RequestHandler();
            }
            return mInstance;
	}

        RequestHandler::RequestHandler(): mRuntimeManagerHandler(nullptr), mWindowManagerHandler(nullptr), mEventHandler(nullptr)
	{
	}

        RequestHandler::~RequestHandler()
	{
	}

        bool RequestHandler::initialize(PluginHost::IShell* service, IEventHandler* eventHandler)
	{
	    bool ret = false;
	    mEventHandler = eventHandler;	
            mRuntimeManagerHandler = new RuntimeManagerHandler();
            ret = mRuntimeManagerHandler->initialize(service, eventHandler);
	    if (!ret)
	    {
                printf("unable to initialize with runtimemanager \n");
		fflush(stdout);
		return ret;
	    }
            mWindowManagerHandler = new WindowManagerHandler();
            ret = mWindowManagerHandler->initialize(service, eventHandler);
	    if (!ret)
	    {
                printf("unable to initialize with windowmanager \n");
		fflush(stdout);
		return ret;
	    }
            StateTransitionHandler::getInstance()->initialize();
            return ret;
	}

        void RequestHandler::terminate()
	{
            StateTransitionHandler::getInstance()->terminate();
            if (mWindowManagerHandler)
            {
                mWindowManagerHandler->terminate();
                delete mWindowManagerHandler;
		mWindowManagerHandler = nullptr;
            }

            if (mRuntimeManagerHandler)
            {
                mRuntimeManagerHandler->deinitialize();
                delete mRuntimeManagerHandler;
		mRuntimeManagerHandler = nullptr;
            }
	}

        bool RequestHandler::launch(ApplicationContext* context, const string& launchIntent, const Exchange::ILifecycleManager::LifecycleState targetLifecycleState, string& errorReason)
	{
            bool success = updateState(context, targetLifecycleState, errorReason);
            return success;
	}

        bool RequestHandler::terminate(ApplicationContext* context, bool force, string& errorReason)
	{
            bool success = false;
	    success = updateState(context, Exchange::ILifecycleManager::LifecycleState::TERMINATING, errorReason);
            return success;
	}

        bool RequestHandler::sendIntent(ApplicationContext* context, const string& intent, string& errorReason)
	{
           context->setMostRecentIntent(intent);
           return true;
	}

	bool RequestHandler::updateState(ApplicationContext* context, Exchange::ILifecycleManager::LifecycleState state, string& errorReason)
	{
           StateTransitionRequest request(context, state);
           StateTransitionHandler::getInstance()->addRequest(request); 
           return true;
	}

        RuntimeManagerHandler* RequestHandler::getRuntimeManagerHandler()
	{
            return mRuntimeManagerHandler;
	}

        WindowManagerHandler* RequestHandler::getWindowManagerHandler()
	{
            return mWindowManagerHandler;
	}

        IEventHandler* RequestHandler::getEventHandler()
	{
            return mEventHandler;
	}

    } /* namespace Plugin */
} /* namespace WPEFramework */
