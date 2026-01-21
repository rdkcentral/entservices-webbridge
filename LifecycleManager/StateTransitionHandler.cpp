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

#include "StateTransitionHandler.h"
#include "StateHandler.h"
#include <thread>
#include <mutex>
#include <vector>

namespace WPEFramework
{
    namespace Plugin
    {
        static std::thread requestHandlerThread;
        std::mutex gRequestMutex;
        sem_t gRequestSemaphore;
        std::vector<std::shared_ptr<StateTransitionRequest>> gRequests;
        static bool sRunning = true;

        StateTransitionHandler* StateTransitionHandler::mInstance = nullptr;

        StateTransitionHandler* StateTransitionHandler::getInstance()
	{
            if (nullptr == mInstance)
            {
                mInstance = new StateTransitionHandler();
            }
            return mInstance;
	}

        StateTransitionHandler::StateTransitionHandler()
	{
	}

        StateTransitionHandler::~StateTransitionHandler()
	{
	}

        bool StateTransitionHandler::initialize()
	{
            sRunning = true;
            StateHandler::initialize();
            sem_init(&gRequestSemaphore, 0, 0);
            requestHandlerThread = std::thread([=]() {
                bool isRunning = true;
                gRequestMutex.lock();
                isRunning = sRunning;
                gRequestMutex.unlock();
                while(isRunning)
		{
                    gRequestMutex.lock();
                    while (gRequests.size() > 0)
                    {
	                std::shared_ptr<StateTransitionRequest> request = gRequests.front();
                        if (!request)
                        {
                            gRequests.erase(gRequests.begin());
                            continue;
                        }
                        std::string errorReason;
                        bool success = StateHandler::changeState(*request, errorReason);
                        if (!success)
                        {
                            printf("ERROR IN STATE TRANSITION ... %s\n",errorReason.c_str());
			    fflush(stdout);
                            //TODO: Decide on what to do on state transition error
                            break;
                        }
                        gRequests.erase(gRequests.begin());
                    }
                    gRequestMutex.unlock();
                    sem_wait(&gRequestSemaphore);
                    gRequestMutex.lock();
                    isRunning = sRunning;
                    gRequestMutex.unlock();
                }
            });
	    return true;	
	}

	void StateTransitionHandler::terminate()
	{
            gRequestMutex.lock();
            sRunning = false;
            gRequestMutex.unlock();
            sem_post(&gRequestSemaphore);
            requestHandlerThread.join();
	}

	void StateTransitionHandler::addRequest(StateTransitionRequest& request)
	{
           //TODO: Pass contect and state as argument to function
	   std::shared_ptr<StateTransitionRequest> stateTransitionRequest = std::make_shared<StateTransitionRequest>(request.mContext, request.mTargetState);
	   gRequestMutex.lock();
           gRequests.push_back(stateTransitionRequest);
	   gRequestMutex.unlock();
           sem_post(&gRequestSemaphore);
	}

    } /* namespace Plugin */
} /* namespace WPEFramework */
