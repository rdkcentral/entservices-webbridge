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

#include "ApplicationContext.h"
#include "RuntimeManagerHandler.h"
#include "WindowManagerHandler.h"
#include "IEventHandler.h"

namespace WPEFramework
{
    namespace Plugin
    {
        class RequestHandler
	{
            public:
                RequestHandler(const RequestHandler& obj) = delete;
                static RequestHandler* getInstance();
                ~RequestHandler ();
                bool initialize(PluginHost::IShell* service, IEventHandler* eventHandler);
		void terminate();

                bool launch(ApplicationContext* context, const string& launchIntent, const Exchange::ILifecycleManager::LifecycleState targetLifecycleState, string& errorReason);
                bool terminate(ApplicationContext* context, bool force, string& errorReason);
                bool sendIntent(ApplicationContext* context, const string& intent, string& errorReason);
                bool updateState(ApplicationContext* context, Exchange::ILifecycleManager::LifecycleState targetLifecycleState, string& errorReason);
                RuntimeManagerHandler* getRuntimeManagerHandler();
		WindowManagerHandler* getWindowManagerHandler();
		IEventHandler* getEventHandler();

	    private: /* members */
                RequestHandler();

                static RequestHandler* mInstance;

                // different component handlers
                RuntimeManagerHandler* mRuntimeManagerHandler;
		WindowManagerHandler* mWindowManagerHandler;
                IEventHandler* mEventHandler;
        };
    } /* namespace Plugin */
} /* namespace WPEFramework */
