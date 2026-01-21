/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
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
*/

#include "RuntimeManagerHandler.h"
#include "UtilsLogging.h"
#include "tracing/Logging.h"
#include <sstream>

namespace WPEFramework {
namespace Plugin {

#define FIREBOLT_ENDPOINT_APPACCESS_PORT 3473

RuntimeManagerHandler::RuntimeManagerHandler()
: mRuntimeManager(nullptr), mRuntimeManagerNotification(*this), mFireboltAccessPort(FIREBOLT_ENDPOINT_APPACCESS_PORT), mEventHandler(nullptr)
{
    LOGINFO("Create RuntimeManagerHandler Instance");
}

RuntimeManagerHandler::~RuntimeManagerHandler()
{
    LOGINFO("Delete RuntimeManagerHandler Instance");
}

bool RuntimeManagerHandler::initialize(PluginHost::IShell* service, IEventHandler* eventHandler)
{
    bool ret = false;
    mEventHandler = eventHandler;
    mRuntimeManager = service->QueryInterfaceByCallsign<Exchange::IRuntimeManager>("org.rdk.RuntimeManager");
    if (mRuntimeManager != nullptr)
    {
        ret = true;
        Core::hresult registerResult = mRuntimeManager->Register(&mRuntimeManagerNotification);
        if (Core::ERROR_NONE != registerResult)
        {
            LOGINFO("Unable to register with runtimemanager [%d] \n", registerResult);
        }
    }
    else
    {
        LOGERR("runtimemanager is null \n");
    }
    return ret;
}

void RuntimeManagerHandler::deinitialize()
{
    Core::hresult unregisterResult = mRuntimeManager->Unregister(&mRuntimeManagerNotification);
    if (Core::ERROR_NONE != unregisterResult)
    {
        LOGINFO("Unable to unregister from runtimemanager [%d] \n", unregisterResult);
    }
    uint32_t result = mRuntimeManager->Release();
    LOGINFO("Terminated runtime manager hanlder [%d] \n", result);
    mRuntimeManager = nullptr;
}

bool RuntimeManagerHandler::getRuntimeStats(const string& appInstanceId, string& info)
{
    Core::hresult result = mRuntimeManager->GetInfo(appInstanceId, info);
    if (Core::ERROR_NONE != result)
    {
        LOGINFO("unable to get information about application");
        return false;
    }
    return true;
}

bool RuntimeManagerHandler::run(const string& appId, const string& appInstanceId, const string& launchArgs, Exchange::ILifecycleManager::LifecycleState targetState, WPEFramework::Exchange::RuntimeConfig& runtimeConfigObject, string& errorReason)
{
    JsonArray debugSettingsArray, pathsArray, portsArray;
    // read data from parameters
    // QUESTION HOW TO GET OTHER PARAMS ports, paths, debugsettings?
    // convert launchArgs to meaningful environment variables when launching a container

    // FOR Q1
    // -- debugSettings - [] //for vbn builds
    // -- paths - paths to set in container - path for XDG_RUNTIME_DIR
    
    runtimeConfigObject.dialId = appId;

    uint32_t userId = 0, groupId = 0;
    std::list<string> debugSettingsList, pathsList;
    std::list<uint32_t> portsList;
    JsonArray envNewArray;

    portsList.push_back(mFireboltAccessPort);

    std::stringstream ss;
    ss << "FIREBOLT_ENDPOINT=http://127.0.0.1:" << mFireboltAccessPort << "?session=" << appInstanceId;
    string fireboltEndPoint(ss.str());
    envNewArray.Add(fireboltEndPoint);

    std::stringstream targetAppStateEnvironmentString;
    targetAppStateEnvironmentString << "TARGET_STATE=" << (uint32_t)targetState;
    string targetAppState(targetAppStateEnvironmentString.str());
    envNewArray.Add(targetAppState);

    JsonArray envInputArray, envResultArray;
    envInputArray.FromString(runtimeConfigObject.envVariables);
    for (unsigned int i = 0; i < envInputArray.Length(); ++i)
    {
        envResultArray.Add(envInputArray[i].String());
    }
    for (unsigned int i = 0; i < envNewArray.Length(); ++i)
    {
        envResultArray.Add(envNewArray[i].String());
    }
    envResultArray.ToString(runtimeConfigObject.envVariables);

    // prepare arguments to pass
    RPC::IStringIterator* debugSettingsIterator{};
    RPC::IStringIterator* pathsIterator{};
    RPC::IValueIterator* portsIterator{};

    debugSettingsIterator = Core::Service<RPC::StringIterator>::Create<RPC::IStringIterator>(debugSettingsList);
    pathsIterator = Core::Service<RPC::StringIterator>::Create<RPC::IStringIterator>(pathsList);
    portsIterator = Core::Service<RPC::ValueIterator>::Create<RPC::IValueIterator>(portsList);

    Core::hresult result = mRuntimeManager->Run(appId, appInstanceId, userId, groupId, portsIterator, pathsIterator, debugSettingsIterator, runtimeConfigObject);
    if (Core::ERROR_NONE != result)
    {
        errorReason = "unable to start running application";
        return false;
    }
    return true;
}

bool RuntimeManagerHandler::kill(const string& appInstanceId, string& errorReason)
{
    Core::hresult result = mRuntimeManager->Kill(appInstanceId);
    if (Core::ERROR_NONE != result)
    {
        errorReason = "unable to kill application";
        return false;
    }
    return true;
}

bool RuntimeManagerHandler::terminate(const string& appInstanceId, string& errorReason)
{
    Core::hresult result = mRuntimeManager->Terminate(appInstanceId);
    if (Core::ERROR_NONE != result)
    {
        errorReason = "unable to terminate application";
        return false;
    }
    return true;
}

bool RuntimeManagerHandler::suspend(const string& appInstanceId, string& errorReason)
{
    Core::hresult result = mRuntimeManager->Suspend(appInstanceId);
    if (Core::ERROR_NONE != result)
    {
        errorReason = "unable to suspend application";
        return false;
    }
    return true;
}

bool RuntimeManagerHandler::resume(const string& appInstanceId, string& errorReason)
{
    Core::hresult result = mRuntimeManager->Resume(appInstanceId);
    if (Core::ERROR_NONE != result)
    {
        errorReason = "unable to resume application";
        return false;
    }
    return true;
}

bool RuntimeManagerHandler::hibernate(const string& appInstanceId, string& errorReason)
{
    Core::hresult result = mRuntimeManager->Hibernate(appInstanceId);
    if (Core::ERROR_NONE != result)
    {
        errorReason = "unable to hibernate application";
        return false;
    }
    return true;
}

bool RuntimeManagerHandler::wake(const string& appInstanceId, Exchange::ILifecycleManager::LifecycleState state, string& errorReason)
{
    Exchange::IRuntimeManager::RuntimeState runtimeState = Exchange::IRuntimeManager::RuntimeState::RUNTIME_STATE_UNKNOWN;
    if (state == Exchange::ILifecycleManager::LifecycleState::SUSPENDED)
    {
        runtimeState = Exchange::IRuntimeManager::RuntimeState::RUNTIME_STATE_SUSPENDED;
    }
    else if ((state == Exchange::ILifecycleManager::LifecycleState::PAUSED) || (state == Exchange::ILifecycleManager::LifecycleState::ACTIVE))
    {
        runtimeState = Exchange::IRuntimeManager::RuntimeState::RUNTIME_STATE_RUNNING;
    }
    Core::hresult result = mRuntimeManager->Wake(appInstanceId, runtimeState);
    if (Core::ERROR_NONE != result)
    {
        errorReason = "unable to wakeup application";
        return false;
    }
    return true;
}

void RuntimeManagerHandler::RuntimeManagerNotification::OnStarted(const string& appInstanceId)
{
    JsonObject eventData;
    eventData["appInstanceId"] = appInstanceId; 
    eventData["name"] = "onStarted";
    _parent.onEvent(eventData);
}

void RuntimeManagerHandler::RuntimeManagerNotification::OnTerminated(const string& appInstanceId)
{
    JsonObject eventData;
    eventData["appInstanceId"] = appInstanceId; 
    eventData["name"] = "onTerminated"; 
    _parent.onEvent(eventData);
}

void RuntimeManagerHandler::RuntimeManagerNotification::OnFailure(const string& appInstanceId, const string& error)
{
    JsonObject eventData;
    eventData["appInstanceId"] = appInstanceId; 
    eventData["name"] = "onFailure";
    eventData["errorCode"] = error;
    _parent.onEvent(eventData);
}

void RuntimeManagerHandler::RuntimeManagerNotification::OnStateChanged(const string& appInstanceId, Exchange::IRuntimeManager::RuntimeState state)
{
    JsonObject eventData;
    eventData["appInstanceId"] = appInstanceId; 
    eventData["state"] = (uint32_t)state;
    eventData["name"] = "onStateChanged"; 
    _parent.onEvent(eventData);
}

void RuntimeManagerHandler::onEvent(JsonObject& data)
{
    if (mEventHandler)
    {
        mEventHandler->onRuntimeManagerEvent(data);
    }
}

} // namespace Plugin
} // namespace WPEFramework
