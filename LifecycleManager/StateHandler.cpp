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
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include "IEventHandler.h"
#include "RequestHandler.h"

namespace WPEFramework
{
    namespace Plugin
    {
        typedef Exchange::ILifecycleManager::LifecycleState Lifecycle;
        std::map<Exchange::ILifecycleManager::LifecycleState, std::list<Exchange::ILifecycleManager::LifecycleState>> StateHandler::mPossibleStateTransitions = std::map<Exchange::ILifecycleManager::LifecycleState, std::list<Exchange::ILifecycleManager::LifecycleState>>();
        std::map<Exchange::ILifecycleManager::LifecycleState, std::string> StateHandler::mStateStrings = std::map<Exchange::ILifecycleManager::LifecycleState, std::string>();
        uint32_t StateHandler::sStateChangeCount = 0;

        bool StateHandler::updateState(ApplicationContext* context, Exchange::ILifecycleManager::LifecycleState lifeCycleState, string& errorReason)
	{
            State* currentState = (State*) context->getState();
            bool result = false;
	    if ((nullptr != currentState) && (currentState->getValue() == lifeCycleState))
	    {
	        return true;	   
	    }
            State* newState = createState(context, lifeCycleState);
            if (nullptr != newState)
	    {
	        //context->setState(nullptr);
                result = newState->handle(errorReason);
                if (result)
		{
	           context->setState(newState);
		   delete currentState;
                }
            }
            return result;
	}

	State* StateHandler::createState(ApplicationContext* context, Exchange::ILifecycleManager::LifecycleState lifeCycleState)
	{
            State* state = nullptr;
            switch (lifeCycleState)
            {
	        case Exchange::ILifecycleManager::LifecycleState::UNLOADED:
                    state = new UnloadedState(context);
                    break;
	        case Exchange::ILifecycleManager::LifecycleState::LOADING:
                    state = new LoadingState(context);
                    break;
	        case Exchange::ILifecycleManager::LifecycleState::INITIALIZING:
                    state = new InitializingState(context);
                    break;
	        case Exchange::ILifecycleManager::LifecycleState::PAUSED:
                    state = new PausedState(context);
                    break;
	        case Exchange::ILifecycleManager::LifecycleState::ACTIVE:
                    state = new ActiveState(context);
                    break;
	        case Exchange::ILifecycleManager::LifecycleState::SUSPENDED:
                    state = new SuspendedState(context);
                    break;
	        case Exchange::ILifecycleManager::LifecycleState::HIBERNATED:
                    state = new HibernatedState(context);
                    break;
    	        case Exchange::ILifecycleManager::LifecycleState::TERMINATING:
                    state = new TerminatingState(context);
                    break;
                default:
		    state = nullptr;	
            }
            return state;
	}

        bool StateHandler::isValidTransition(Exchange::ILifecycleManager::LifecycleState start, Exchange::ILifecycleManager::LifecycleState target, std::map<Exchange::ILifecycleManager::LifecycleState, bool>& pathSequence, std::vector<Exchange::ILifecycleManager::LifecycleState>& foundPath)
        {
            bool pathPresent = false;
            if (start == target)
            {
                return true;
            }
            pathSequence[target] = true; 
            std::map<Exchange::ILifecycleManager::LifecycleState, std::list<Exchange::ILifecycleManager::LifecycleState>>::iterator transitionIter = mPossibleStateTransitions.find(target);
            if (transitionIter == mPossibleStateTransitions.end())
            {
                return false;
            }
            std::list<Exchange::ILifecycleManager::LifecycleState>& transitionList = transitionIter->second;
            for (auto iter = transitionList.begin(); iter != transitionList.end(); ++iter)
            {
                if (pathSequence.find(*iter) != pathSequence.end())
                {
                    continue;
                }

                pathPresent = isValidTransition(start, *iter, pathSequence, foundPath);
                if (pathPresent)
                {
	            foundPath.push_back(*iter);		
                    break;
                }
            }
            return pathPresent;
        }

	void StateHandler::initialize()
	{
            mPossibleStateTransitions[Lifecycle::UNLOADED] = std::list<Exchange::ILifecycleManager::LifecycleState>();
            mPossibleStateTransitions[Lifecycle::LOADING] = std::list<Exchange::ILifecycleManager::LifecycleState>(1, Lifecycle::UNLOADED);
            mPossibleStateTransitions[Lifecycle::INITIALIZING] = std::list<Exchange::ILifecycleManager::LifecycleState>(1, Lifecycle::LOADING); 

            mPossibleStateTransitions[Lifecycle::PAUSED] = std::list<Exchange::ILifecycleManager::LifecycleState>();
	    std::list<Exchange::ILifecycleManager::LifecycleState>& pausedStatePre = mPossibleStateTransitions[Lifecycle::PAUSED];
	    pausedStatePre.push_back(Lifecycle::INITIALIZING);
	    pausedStatePre.push_back(Lifecycle::ACTIVE);
	    pausedStatePre.push_back(Lifecycle::SUSPENDED);
          
            mPossibleStateTransitions[Lifecycle::ACTIVE] = std::list<Exchange::ILifecycleManager::LifecycleState>();
	    std::list<Exchange::ILifecycleManager::LifecycleState>& activeStatePre = mPossibleStateTransitions[Lifecycle::ACTIVE];
            activeStatePre.push_back(Lifecycle::PAUSED);

            mPossibleStateTransitions[Lifecycle::SUSPENDED] = std::list<Exchange::ILifecycleManager::LifecycleState>();
	    std::list<Exchange::ILifecycleManager::LifecycleState>& suspendedStatePre = mPossibleStateTransitions[Lifecycle::SUSPENDED];
            suspendedStatePre.push_back(Lifecycle::INITIALIZING);
            suspendedStatePre.push_back(Lifecycle::PAUSED);
            suspendedStatePre.push_back(Lifecycle::HIBERNATED);

            mPossibleStateTransitions[Lifecycle::HIBERNATED] = std::list<Exchange::ILifecycleManager::LifecycleState>(1, Lifecycle::SUSPENDED);    

            mPossibleStateTransitions[Lifecycle::TERMINATING] = std::list<Exchange::ILifecycleManager::LifecycleState>();
	    std::list<Exchange::ILifecycleManager::LifecycleState>& terminatingStatePre = mPossibleStateTransitions[Lifecycle::TERMINATING];
            terminatingStatePre.push_back(Lifecycle::PAUSED);
            terminatingStatePre.push_back(Lifecycle::SUSPENDED);

	    //state strings
            mStateStrings[Lifecycle::UNLOADED] = "Unloaded";
            mStateStrings[Lifecycle::LOADING] = "Loading";
            mStateStrings[Lifecycle::INITIALIZING] = "Initializing"; 
            mStateStrings[Lifecycle::PAUSED] = "Paused";
            mStateStrings[Lifecycle::ACTIVE] = "Active"; 
            mStateStrings[Lifecycle::SUSPENDED] = "Suspended";
            mStateStrings[Lifecycle::HIBERNATED] = "Hibernated";
            mStateStrings[Lifecycle::TERMINATING] = "Terminating";
	}

        bool StateHandler::changeState(StateTransitionRequest& request, string& errorReason)
	{
            Exchange::ILifecycleManager::LifecycleState lifecycleState = request.mTargetState;
            ApplicationContext* context = request.mContext;

            if (context == nullptr)
            {
                errorReason = "ApplicationContext is null";
                return false;
            }
            Exchange::ILifecycleManager::LifecycleState currentLifecycleState = context->getCurrentLifecycleState();

            if ((context->mPendingStateTransition) && (Exchange::ILifecycleManager::LifecycleState::TERMINATING != currentLifecycleState))
            {
                sendEvent(context, context->mPendingOldState, currentLifecycleState, errorReason);
            }

            if ((false == context->mPendingStateTransition) && (currentLifecycleState == lifecycleState))
	    {
	        return true;
	    }

            std::vector<Exchange::ILifecycleManager::LifecycleState> statePath;
            bool result = getStatePath(context, lifecycleState, statePath, errorReason);
            if (false == result)
            {
                printf("Unable to get sequence for target state \n");
                fflush(stdout);
                return false; 
            }

            bool isStateTerminating = false;
            size_t lastStateIndex = static_cast<size_t>(-1); 
            context->mPendingStateTransition = false;

            // start from next state
	    for (size_t stateIndex=1; stateIndex<statePath.size(); stateIndex++)
	    {
                Exchange::ILifecycleManager::LifecycleState oldLifecycleState = ((State*)context->getState())->getValue();
                isStateTerminating = (Exchange::ILifecycleManager::LifecycleState::TERMINATING == statePath[stateIndex]);
                if (!isStateTerminating)
		{
                    result = updateState(context, statePath[stateIndex], errorReason);
                    if(result)
                    {
                        printf("StateHandler::changeState: Success %s -> %s\n", mStateStrings[oldLifecycleState].c_str(), mStateStrings[statePath[stateIndex]].c_str());
                    }
                    else
                    {
                        printf("StateHandler::changeState: Failed to change state from %s to %s\n", mStateStrings[oldLifecycleState].c_str(), mStateStrings[statePath[stateIndex]].c_str());
                        break;
                    }
                    if (true == context->mPendingStateTransition)
                    {
                        context->mPendingOldState = oldLifecycleState;
                        lastStateIndex = stateIndex;
                        break;
                    }
                }

		Exchange::ILifecycleManager::LifecycleState newLifecycleState = ((State*)context->getState())->getValue();
                if (isStateTerminating)
		{
                    newLifecycleState = statePath[stateIndex];
                }
                sendEvent(context, oldLifecycleState, newLifecycleState, errorReason);

                if (isStateTerminating)
                {
                    result = updateState(context, statePath[stateIndex], errorReason);
                    if(result)
                    {
                        printf("StateHandler::changeState:Terminating: Success %s -> %s\n", mStateStrings[oldLifecycleState].c_str(), mStateStrings[statePath[stateIndex]].c_str());
                    }
                    else
                    {
                        printf("StateHandler::changeState:Terminating: Failed to change state from %s to %s\n", mStateStrings[oldLifecycleState].c_str(), mStateStrings[statePath[stateIndex]].c_str());
                        break;
                    }
                    if (true == context->mPendingStateTransition)
                    {
                        lastStateIndex = stateIndex;
                        break;
                    }
                }
            }
            if (true == context->mPendingStateTransition)
            {
                context->mPendingStates.clear();
                if (lastStateIndex < statePath.size())
                {
                    for (size_t stateIndex = lastStateIndex; stateIndex < statePath.size() ; stateIndex++)
                    {
                        context->mPendingStates.push_back(statePath[stateIndex]);
                    }
                }
            }
            else 
	    {
                context->mPendingStateTransition = false;
                context->mPendingEventName = "";
                context->mPendingStates.clear();
            }		  
            return result;
        }

        void StateHandler::sendEvent(ApplicationContext* context, Exchange::ILifecycleManager::LifecycleState oldLifecycleState, Exchange::ILifecycleManager::LifecycleState newLifecycleState, string& errorReason)
	{
            IEventHandler* eventHandler = RequestHandler::getInstance()->getEventHandler();

            if (nullptr != eventHandler)
            {
                struct timespec stateChangeTime;
                timespec_get(&stateChangeTime, TIME_UTC);
                context->setLastLifecycleStateChangeTime(stateChangeTime);
                context->setStateChangeId(sStateChangeCount);
                sStateChangeCount++;

                JsonObject eventData;
                eventData["appId"] = context->getAppId();
                eventData["appInstanceId"] = context->getAppInstanceId();
                eventData["oldLifecycleState"] = (uint32_t)oldLifecycleState;
                eventData["newLifecycleState"] = (uint32_t)newLifecycleState;
                if (newLifecycleState == Exchange::ILifecycleManager::LifecycleState::ACTIVE)
                {
                    eventData["navigationIntent"] = context->getMostRecentIntent();
                }
                eventData["errorReason"] = errorReason;
                eventHandler->onStateChangeEvent(eventData);
            }
        }

        bool StateHandler::getStatePath(ApplicationContext* context, Exchange::ILifecycleManager::LifecycleState lifecycleState, std::vector<Exchange::ILifecycleManager::LifecycleState>& statePath, string& errorReason)
        {
            if (false == context->mPendingStateTransition)
            {
                Exchange::ILifecycleManager::LifecycleState currentLifecycleState = context->getCurrentLifecycleState();
                std::map<Exchange::ILifecycleManager::LifecycleState, bool> seenPaths;

                bool isValidRequest = StateHandler::isValidTransition(currentLifecycleState, lifecycleState, seenPaths, statePath);
                if (!isValidRequest)
                {
                    errorReason = "Invalid launch request in current state";
                    return false;
                }
                //ensure final state is pushed here
                statePath.push_back(lifecycleState);

                if (Exchange::ILifecycleManager::LifecycleState::TERMINATING == lifecycleState)
	        {
                    statePath.push_back(Exchange::ILifecycleManager::LifecycleState::UNLOADED);
	        }
            }
            else
            {
                statePath = context->mPendingStates;
            }
            return true;
        }
    } /* namespace Plugin */
} /* namespace WPEFramework */
