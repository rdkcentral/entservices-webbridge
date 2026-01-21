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

#include "RDKWindowManagerImplementation.h"
#include <sys/prctl.h>
#include <mutex>
#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include <rdkwindowmanager/include/compositorcontroller.h>
#include <rdkwindowmanager/include/application.h>
#include <rdkwindowmanager/include/logger.h>
#include "UtilsJsonRpc.h"
#include "UtilsUnused.h"
#include "UtilsString.h"

using namespace std;
using namespace RdkWindowManager;
using namespace Utils;

extern int gCurrentFramerate;
static bool sRunning = true;

#define ANY_KEY                  65536
#define KEYCODE_INVALID          -1
#define TRY_LOCK_WAIT_TIME_IN_MS 250

namespace WPEFramework {
namespace Plugin {

SERVICE_REGISTRATION(RDKWindowManagerImplementation, RDK_WINDOW_MANAGER_API_VERSION_NUMBER_MAJOR, RDK_WINDOW_MANAGER_API_VERSION_NUMBER_MINOR);
RDKWindowManagerImplementation* RDKWindowManagerImplementation::_instance = nullptr;

RDKWindowManagerImplementation::RDKWindowManagerImplementation()
: mAdminLock()
, mService(nullptr)
{
    LOGINFO("Create RDKWindowManagerImplementation Instance");
    RDKWindowManagerImplementation::_instance = this;
}

RDKWindowManagerImplementation::~RDKWindowManagerImplementation()
{
}

uint32_t getKeyFlag(std::string modifier)
{
    uint32_t flag = 0;

    if (0 == modifier.compare("ctrl"))
    {
        flag = RDK_WINDOW_MANAGER_FLAGS_CONTROL;
    }
    else if (0 == modifier.compare("shift"))
    {
        flag = RDK_WINDOW_MANAGER_FLAGS_SHIFT;
    }
    else if (0 == modifier.compare("alt"))
    {
        flag = RDK_WINDOW_MANAGER_FLAGS_ALT;
    }

    return flag;
}

static std::mutex gRdkWindowManagerMutex;
static std::thread shellThread;
static std::vector<std::shared_ptr<CreateDisplayRequest>> gCreateDisplayRequests;

CreateDisplayRequest::CreateDisplayRequest(std::string client, std::string displayName, uint32_t displayWidth, uint32_t displayHeight, bool virtualDisplayEnabled,
                                           uint32_t virtualWidth, uint32_t virtualHeight, bool topmost, bool focus)
: mClient(std::move(client))
, mDisplayName(std::move(displayName))
, mDisplayWidth(displayWidth)
, mDisplayHeight(displayHeight)
, mVirtualDisplayEnabled(virtualDisplayEnabled)
, mVirtualWidth(virtualWidth)
, mVirtualHeight(virtualHeight)
, mTopmost(topmost)
, mFocus(focus)
, mResult(false)
, mAutoDestroy(true)
{
    if (0 != sem_init(&mSemaphore, 0, 0))
    {
        LOGERR("Failed to initialize semaphore: %s", strerror(errno));
    }
}

CreateDisplayRequest::~CreateDisplayRequest()
{
    if (0 != sem_destroy(&mSemaphore))
    {
        LOGERR("Failed to destroy semaphore: %s", strerror(errno));
    }
}

bool lockRdkWindowManagerMutex()
{
    bool lockAcquired = false;
    double startTime = RdkWindowManager::milliseconds();
    while (!lockAcquired && (RdkWindowManager::milliseconds() - startTime) < TRY_LOCK_WAIT_TIME_IN_MS)
    {
        lockAcquired = gRdkWindowManagerMutex.try_lock();
    }
    if (!lockAcquired)
    {
        LOGERR("unable to get lock for defaulting to normal lock");
        gRdkWindowManagerMutex.lock();
        lockAcquired = true;
    }
    return lockAcquired;
}

static bool isClientExists(std::string client)
{
    bool exist = false;
    bool retValue = false;
    bool lockAcquired = false;

    gRdkWindowManagerMutex.lock();
    for (unsigned int i=0; i<gCreateDisplayRequests.size(); i++)
    {
        if (gCreateDisplayRequests[i]->mClient.compare(client) == 0)
        {
            exist = true;
            break;
        }
    }
    gRdkWindowManagerMutex.unlock();

    if (!exist)
    {
        std::vector<std::string> clientList;
        lockAcquired = lockRdkWindowManagerMutex();
        if (lockAcquired)
        {
            retValue = CompositorController::getClients(clientList);
            gRdkWindowManagerMutex.unlock();
            if (true == retValue)
            {
                std::string newClient(std::move(client));
                transform(newClient.begin(), newClient.end(), newClient.begin(), ::tolower);

                if (std::find(clientList.begin(), clientList.end(), newClient) != clientList.end())
                {
                    exist = true;
                }
            }
            else
            {
                LOGERR("getClients Failed");
            }
        }

    }
    return exist;
}

std::string toLower(const std::string& clientName)
{
    std::string displayName = clientName;
    std::transform(displayName.begin(), displayName.end(), displayName.begin(), [](unsigned char c){ return std::tolower(c); });
    return displayName;
}

Core::hresult RDKWindowManagerImplementation::Initialize(PluginHost::IShell* service)
{
    Core::hresult result = Core::ERROR_NONE;

    ASSERT(nullptr != service);

    mService = service;

    if (nullptr != mService)
    {
        mService->AddRef();

        char* waylandDisplay = getenv("WAYLAND_DISPLAY");
        if (NULL != waylandDisplay)
        {
            LOGINFO("RDKWindowManager WAYLAND_DISPLAY is set to: %s unsetting WAYLAND_DISPLAY", waylandDisplay);
            if (0 != unsetenv("WAYLAND_DISPLAY"))
            {
                LOGERR("Failed, unsetenv for WAYLAND_DISPLAY %s ", strerror(errno));
            }
        }
        else
        {
            LOGINFO("RDKWindowManager WAYLAND_DISPLAY is not set");
        }

        mEventListener = std::make_shared<RDKWindowManagerImplementation::RdkWindowManagerListener>(this);

        CompositorController::setEventListener(mEventListener);

        enableInactivityReporting(true);

        static PluginHost::IShell* pluginService = nullptr;
        pluginService = mService;

        shellThread = std::thread([=]() {
            bool isRunning = true;
            gRdkWindowManagerMutex.lock();
            RdkWindowManager::initialize();
            PluginHost::ISubSystem* subSystems(pluginService->SubSystems());
            if (subSystems != nullptr)
            {
                LOGERR("setting platform and graphics");

                subSystems->Set(PluginHost::ISubSystem::PLATFORM, nullptr);
                subSystems->Set(PluginHost::ISubSystem::GRAPHICS, nullptr);
                subSystems->Release();
            }
            isRunning = sRunning;
            gRdkWindowManagerMutex.unlock();
            while(isRunning) {
              const double maxSleepTime = (1000 / gCurrentFramerate) * 1000;
              double startFrameTime = RdkWindowManager::microseconds();
              gRdkWindowManagerMutex.lock();

              while (gCreateDisplayRequests.size() > 0)
              {
                  std::shared_ptr<CreateDisplayRequest> request = gCreateDisplayRequests.front();
                  if (!request)
                  {
                      gCreateDisplayRequests.erase(gCreateDisplayRequests.begin());
                      continue;
                  }
                  request->mResult = CompositorController::createDisplay(request->mClient, request->mDisplayName, request->mDisplayWidth, request->mDisplayHeight, request->mVirtualDisplayEnabled, request->mVirtualWidth, request->mVirtualHeight, request->mTopmost, request->mFocus , request->mAutoDestroy);
                  gCreateDisplayRequests.erase(gCreateDisplayRequests.begin());
                  if (0 != sem_post(&request->mSemaphore))
                  {
                      LOGERR("Failed to release CreateDisplayRequest semaphore: %s", strerror(errno));
                  }
              }

              RdkWindowManager::draw();
              RdkWindowManager::update();
              isRunning = sRunning;
              gRdkWindowManagerMutex.unlock();
              double frameTime = (int)RdkWindowManager::microseconds() - (int)startFrameTime;
              if (frameTime < maxSleepTime)
              {
                  int sleepTime = (int)maxSleepTime-(int)frameTime;
                  usleep(sleepTime);
              }
            }
        });

        LOGINFO("RDKWindowManagerImplementation::Initialized");
    }
    else
    {
        LOGERR("mService is nullptr");
        result = Core::ERROR_GENERAL;
    }
    
    return (result);
}

void RDKWindowManagerImplementation::Deinitialize(PluginHost::IShell* service)
{
    ASSERT(nullptr != service);

    if (nullptr != mService)
    {
        mService->Release();
        mService = nullptr;
    }

    gRdkWindowManagerMutex.lock();
    RdkWindowManager::deinitialize();
    sRunning = false;
    gRdkWindowManagerMutex.unlock();

    shellThread.join();

    std::vector<std::string> clientList;
    if (false == CompositorController::getClients(clientList))
    {
        LOGERR("getClients Failed");
    }

    std::vector<std::string>::iterator ptr;
    for(ptr=clientList.begin();ptr!=clientList.end();ptr++)
    {
        if(false == RdkWindowManager::CompositorController::removeListener((*ptr),mEventListener))
        {
            LOGERR("CompositorController::removeListener Failed");
        }
    }

    RDKWindowManagerImplementation::_instance = nullptr;

    CompositorController::setEventListener(nullptr);
    mEventListener = nullptr;

    gRdkWindowManagerMutex.lock();
    for (unsigned int i=0; i<gCreateDisplayRequests.size(); i++)
    {
        if(gCreateDisplayRequests[i] != nullptr)
        {
            if (0 != sem_destroy(&gCreateDisplayRequests[i]->mSemaphore))
            {
                LOGERR("Failed to destroy semaphore: %s", strerror(errno));
            }
            gCreateDisplayRequests[i] = nullptr;
        }
    }
    gCreateDisplayRequests.clear();

    gRdkWindowManagerMutex.unlock();

    LOGINFO("RDKWindowManagerImplementation::Deinitialized");
}

/**
 * Register a notification callback
 */
Core::hresult RDKWindowManagerImplementation::Register(INotification *notification)
{
    ASSERT (nullptr != notification);

    mAdminLock.Lock();

    /* Make sure we can't register the same notification callback multiple times */
    if (std::find(mRDKWindowManagerNotification.begin(), mRDKWindowManagerNotification.end(), notification) == mRDKWindowManagerNotification.end())
    {
        LOGINFO("Register notification");
        mRDKWindowManagerNotification.push_back(notification);
        notification->AddRef();
    }

    mAdminLock.Unlock();

    return Core::ERROR_NONE;
}

/**
 * Unregister a notification callback
 */
Core::hresult RDKWindowManagerImplementation::Unregister(INotification *notification )
{
    Core::hresult status = Core::ERROR_GENERAL;

    ASSERT (nullptr != notification);

    mAdminLock.Lock();

    /* Make sure we can't unregister the same notification callback multiple times */
    auto itr = std::find(mRDKWindowManagerNotification.begin(), mRDKWindowManagerNotification.end(), notification);
    if (itr != mRDKWindowManagerNotification.end())
    {
        (*itr)->Release();
        LOGINFO("Unregister notification");
        mRDKWindowManagerNotification.erase(itr);
        status = Core::ERROR_NONE;
    }
    else
    {
        LOGERR("notification not found");
    }

    mAdminLock.Unlock();

    return status;
}

void RDKWindowManagerImplementation::RdkWindowManagerListener::onUserInactive(const double minutes)
{
    LOGINFO("RDKWindowManager onUserInactive event received minutes: %f", minutes);

    if ( nullptr == mRDKWindowManagerImpl )
    {
        LOGERR("mRDKWindowManagerImpl is null");
    }
    else
    {
        mRDKWindowManagerImpl->dispatchEvent(RDKWindowManagerImplementation::Event::RDK_WINDOW_MANAGER_EVENT_ON_USER_INACTIVITY, JsonValue(minutes));
    }
}

void RDKWindowManagerImplementation::dispatchEvent(Event event, const JsonValue &params)
{
    Core::IWorkerPool::Instance().Submit(Job::Create(this, event, params));
}

void RDKWindowManagerImplementation::Dispatch(Event event, const JsonValue params)
{
     mAdminLock.Lock();

     std::list<Exchange::IRDKWindowManager::INotification*>::const_iterator index(mRDKWindowManagerNotification.begin());

     switch(event) {
         case RDK_WINDOW_MANAGER_EVENT_ON_USER_INACTIVITY:
             while (index != mRDKWindowManagerNotification.end())
             {
                 LOGINFO("RDKWindowManager Dispatch OnUserInactivity minutes: %f", params.Double());

                 (*index)->OnUserInactivity(params.Double());
                 ++index;
             }
         break;

         default:
             LOGWARN("Event[%u] not handled", event);
             break;
     }

     mAdminLock.Unlock();
}

/***
 * @brief Create the display window.
 * Creates a display for the specified client using the configuration parameters.
 *
 * @displayParams[in] : JSON String format with client/callSign, displayName, displayWidth, displayHeight,
 *                      virtualDisplay,virtualWidth,virtualHeight,topmost,focus
 *                      Ex:{\"callsign\": \"org.rdk.YouTube\",\"displayName\": \"test1\",\"displayWidth\": 1920,\"displayHeight\": 1080,
 *                          \"virtualDisplay\": true,\"virtualWidth\": 1920,\"virtualHeight\": 1080,\"topmost\": false,\"focus\": false}
 * @return            : Core::<StatusCode>
 */
Core::hresult RDKWindowManagerImplementation::CreateDisplay(const string &displayParams)
{
    Core::hresult status = Core::ERROR_GENERAL;
    bool result = true;
    JsonObject parameters;

    if (displayParams.empty())
    {
        LOGERR("displayParams is empty");
    }
    else
    {
        LOGINFO("displayParams :%s", displayParams.c_str());

        parameters.FromString(displayParams);

        if (!parameters.HasLabel("client") && !parameters.HasLabel("callsign"))
        {
            LOGERR("client or callsign");
        }
        else
        {
            string client = "";
            if (parameters.HasLabel("client"))
            {
                client = parameters["client"].String();
            }
            else
            {
                client = parameters["callsign"].String();
            }

            string displayName("");
            if (parameters.HasLabel("displayName"))
            {
                displayName = parameters["displayName"].String();
            }

            uint32_t displayWidth = 0;
            if (parameters.HasLabel("displayWidth"))
            {
                displayWidth = parameters["displayWidth"].Number();
            }

            uint32_t displayHeight = 0;
            if (parameters.HasLabel("displayHeight"))
            {
                displayHeight = parameters["displayHeight"].Number();
            }

            bool virtualDisplay = false;
            if (parameters.HasLabel("virtualDisplay"))
            {
                virtualDisplay = parameters["virtualDisplay"].Boolean();
            }

            uint32_t virtualWidth = 0;
            if (parameters.HasLabel("virtualWidth"))
            {
                virtualWidth = parameters["virtualWidth"].Number();
            }

            uint32_t virtualHeight = 0;
            if (parameters.HasLabel("virtualHeight"))
            {
                virtualHeight = parameters["virtualHeight"].Number();
            }

            bool topmost = false;
            if (parameters.HasLabel("topmost"))
            {
                topmost = parameters["topmost"].Boolean();
            }

            bool focus = false;
            if (parameters.HasLabel("focus"))
            {
                focus = parameters["focus"].Boolean();
            }

            result = createDisplay(client, displayName, displayWidth, displayHeight,
                                   virtualDisplay, virtualWidth, virtualHeight, topmost, focus);

            if (false == result)
            {
                LOGERR("failed to create display : %s, displayName:%s, displayWidth:%u, displayHeight:%u, virtualDisplay:%u, virtualWidth:%u, virtualHeight:%u, topmost:%u, focus:%u",
                       client.c_str(), displayName.c_str(), displayWidth, displayHeight, virtualDisplay, virtualWidth, virtualHeight, topmost, focus);
            }
            else
            {
                status = Core::ERROR_NONE;
            }
        }
    }
    return status;
}

/***
 * @brief Create the display window.
 * Creates a display for the specified client using the configuration parameters.
 *
 * @clients[out]      : get the array of clients as a JSON string format
 *                      Ex: [\"org.rdk.youttube\",\"org.rdk.netflix\"]
 * @return            : Core::<StatusCode>
 */
Core::hresult RDKWindowManagerImplementation::GetClients(string &clients) const
{
    Core::hresult status = Core::ERROR_GENERAL;
    bool retValue = false;
    std::vector<std::string> clientList;
    JsonArray clientsArray;
    bool lockAcquired = false;

    lockAcquired = lockRdkWindowManagerMutex();
    if (lockAcquired)
    {
        retValue = CompositorController::getClients(clientList);
        gRdkWindowManagerMutex.unlock();
    }

    if (true == retValue)
    {
        for (size_t i = 0; i < clientList.size(); i++)
        {
            clientsArray.Add(clientList[i]);
        }

        if (clientsArray.IsSet())
        {
            clientsArray.ToString(clients);
            LOGINFO("clients: %s", clients.c_str());
        }
        else
        {
            LOGWARN("There is no clients");
        }
        status = Core::ERROR_NONE;
    }
    else
    {
        LOGERR("getClients Falied");
    }

    return status;
}

/* @brief AddKeyIntercept for the client.
 * A method to intercept key events before dispatched, allowing blocking of specific keys
 *
 * @intercept[in] : JSON String format with client/callSign, keyCode, modifiers
 * @return ERROR_NONE if success , ERROR_GENERAL if failure.
 */
Core::hresult RDKWindowManagerImplementation::AddKeyIntercept(const string &intercept)
{
    Core::hresult status = Core::ERROR_GENERAL;
    JsonObject parameters;

    if (intercept.empty())
    {
        LOGERR("intercept is empty");
    }
    else
    {
        LOGINFO("intercept :%s", intercept.c_str());
        parameters.FromString(intercept);
        if (!parameters.HasLabel("keyCode"))
        {
            LOGERR("please specify keyCode");
        }
        else if (!parameters.HasLabel("client") && !parameters.HasLabel("callsign"))
        {
            LOGERR("please specify client or callsign");
        }
        else
        {
            /*optional param */
            const JsonArray modifiers = parameters.HasLabel("modifiers") ? parameters["modifiers"].Array() : JsonArray();

            const uint32_t keyCode = parameters["keyCode"].Number();
            string client = "";
            if (parameters.HasLabel("client"))
            {
                client = parameters["client"].String();
            }
            else
            {
                client = parameters["callsign"].String();
            }
            if (false == addKeyIntercept(keyCode, modifiers, client))
            {
                LOGERR("failed to add key intercept");
            }
            else
            {
                status = Core::ERROR_NONE;
            }
        }
    }
    return status;
}

/* @brief AddKeyIntercepts for the clients.
 * A method to add mutiple key interceptors  to enabling blocking of several keys for mutiple client
 * @intercepts[in] : JSON String format with client/callSign, keyCode, modifiers
 * @return ERROR_NONE if success , ERROR_GENERAL if failure.
 */
Core::hresult RDKWindowManagerImplementation::AddKeyIntercepts(const string &intercepts)
{
    Core::hresult status = Core::ERROR_GENERAL;
    JsonObject parameters;

    if (intercepts.empty())
    {
        LOGERR("intercepts is empty");
    }
    else
    {
        LOGINFO("intercepts :%s", intercepts.c_str());
        parameters.FromString(intercepts);
        if (!parameters.HasLabel("intercepts"))
        {
            LOGERR("please specify intercepts");
        }
        else
        {
            const JsonArray keyIntercepts = parameters["intercepts"].Array();
            if (false == addKeyIntercepts(keyIntercepts))
            {
                LOGERR("failed to add some key intercepts due to missing parameters or wrong format");
            }
            else
            {
                status = Core::ERROR_NONE;
            }
        }
    }
    return status;
}

/* @brief RemoveKeyIntercept of the client.
 * Intercept key is removed from that client
 *
 * @intercept[in] : JSON String format with client/callSign, keyCode, modifiers
 * @return ERROR_NONE if success , ERROR_GENERAL if failure.
 */
Core::hresult RDKWindowManagerImplementation::RemoveKeyIntercept(const string &intercept)
{
    Core::hresult status = Core::ERROR_GENERAL;
    JsonObject parameters;

    if (intercept.empty())
    {
        LOGERR("intercept is empty");
    }
    else
    {
        LOGINFO("intercept :%s", intercept.c_str());
        parameters.FromString(intercept);

        if (!parameters.HasLabel("keyCode"))
        {
            LOGERR("please specify keyCode");
        }
        else if (!parameters.HasLabel("client") && !parameters.HasLabel("callsign"))
        {
            LOGERR("please specify client or callsign");
        }
        else
        {
            /* optional param */
            const JsonArray modifiers = parameters.HasLabel("modifiers") ? parameters["modifiers"].Array() : JsonArray();

            uint32_t keyCode = parameters["keyCode"].Number();
            /* check for * parameter */
            JsonValue keyCodeValue = parameters["keyCode"];
            if (keyCodeValue.Content() == JsonValue::type::STRING)
            {
                std::string keyCodeStringValue = parameters["keyCode"].String();
                if (keyCodeStringValue.compare("*") == 0)
                {
                    keyCode = 255;
                }
            }
            string client = "";
            if (parameters.HasLabel("client"))
            {
                client = parameters["client"].String();
            }
            else
            {
                client = parameters["callsign"].String();
            }
            if (false == removeKeyIntercept(keyCode, modifiers, client))
            {
                LOGERR("failed to remove key intercept");
            }
            else
            {
                status = Core::ERROR_NONE;
            }
        }
    }
    return status;
}


/* @brief AddKeyListener for the client.
 * A method to handle key events ,enabling custom actions when specific keys are injected/pressed for that client
 *
 * @keyListeners[in] : JSON String format with client/callSign, keys:keyCode/nativeKeyCode,modifiers,activate.propagate
 * @return ERROR_NONE if success , ERROR_GENERAL if failure.
 */
Core::hresult RDKWindowManagerImplementation::AddKeyListener(const string &keyListeners)
{
    Core::hresult status = Core::ERROR_GENERAL;
    JsonObject parameters;

    if (keyListeners.empty())
    {
        LOGERR("keyListeners is empty");
    }
    else
    {
        LOGINFO("keyListeners :%s", keyListeners.c_str());
        parameters.FromString(keyListeners);
        if (!parameters.HasLabel("keys"))
        {
            LOGERR("please specify keys");
        }
        else if (!parameters.HasLabel("client") && !parameters.HasLabel("callsign"))
        {
            LOGERR("please specify client or callsign");
        }
        else
        {
            const JsonArray keys = parameters["keys"].Array();
            string client = "";
            if (parameters.HasLabel("client"))
            {
                client = parameters["client"].String();
            }
            else
            {
                client = parameters["callsign"].String();
            }
            if (false == addKeyListeners(client, keys))
            {
                LOGERR("failed to add key listeners");
            }
            else
            {
                status = Core::ERROR_NONE;
            }
        }
    }
    return status;
}


/* @brief RemvoeKeyListener for the client.
 * A method to remove previously added keyEvent listeners , stopping them from handling key events
 *
 * @keyListeners[in] : JSON String format with client/callSign, keys:keyCode/nativeKeyCode,modifiers
 * @return ERROR_NONE if success , ERROR_GENERAL if failure.
 */
Core::hresult RDKWindowManagerImplementation::RemoveKeyListener(const string &keyListeners)
{
    Core::hresult status = Core::ERROR_GENERAL;
    JsonObject parameters;

    if (keyListeners.empty())
    {
        LOGERR("keyListeners is empty");
    }
    else
    {
        LOGINFO("keyListeners :%s", keyListeners.c_str());
        parameters.FromString(keyListeners);
        if (!parameters.HasLabel("keys"))
        {
            LOGERR("please specify keys");
        }
        else if (!parameters.HasLabel("client") && !parameters.HasLabel("callsign"))
        {
            LOGERR("please specify client");
        }
        else
        {
            const JsonArray keys = parameters["keys"].Array();
            string client= "";
            if (parameters.HasLabel("client"))
            {
                client = parameters["client"].String();
            }
            else
            {
                client = parameters["callsign"].String();
            }
            if (false == removeKeyListeners(client, keys))
            {
                LOGERR("failed to remove key listeners");
            }
            else
            {
                status = Core::ERROR_NONE;
            }
        }
    }
    return status;
}


/* @brief InjectKey .
 * A method to programmtically simulate key events
 *
 * @keyCode[in] key to simulate , @modifiers[in] like ctrl, alt, shift
 * @return ERROR_NONE if success , ERROR_GENERAL if failure.
 */
Core::hresult RDKWindowManagerImplementation::InjectKey(uint32_t keyCode, const string &modifiers)
{
    Core::hresult status = Core::ERROR_GENERAL;
    JsonObject parameters;
    JsonArray keyModifiers = JsonArray();

    if (modifiers.empty())
    {
        LOGINFO("modifiers is empty");
    }
    else
    {
        LOGINFO("modifiers :%s", modifiers.c_str());
        parameters.FromString(modifiers);
        /* optional param */
        keyModifiers = parameters.HasLabel("modifiers") ? parameters["modifiers"].Array() : JsonArray();
    }

    if (false == injectKey(keyCode, keyModifiers))
    {
        LOGERR("failed to inject key");
    }
    else
    {
        status = Core::ERROR_NONE;
    }
    return status;
}

/* @brief GenerateKey .
 * A method to programmtically simulate mutiple key events for the client
 *
 * @keys[in] keys:keyCode,modifiers,delay , @client[in] for the client
 * @return ERROR_NONE if success , ERROR_GENERAL if failure.
 */
Core::hresult RDKWindowManagerImplementation::GenerateKey(const string& keys, const string& client)
{
    Core::hresult status = Core::ERROR_GENERAL;
    JsonObject parameters;

    if (keys.empty())
    {
        LOGERR("keys is empty");
    }
    else
    {
        LOGINFO("keys :%s", keys.c_str());
        parameters.FromString(keys);
        if (!parameters.HasLabel("keys"))
        {
            LOGERR("please specify keyInputs");
        }
        else
        {
            const JsonArray keyInputs = parameters["keys"].Array();

            if (false == generateKey(client, keyInputs))
            {
                LOGERR("failed to generate keys");
            }
            else
            {
                status = Core::ERROR_NONE;
            }
        }
    }
    return status;
}

/***
 @brief Enables the inactivity reporting feature
*
* @enable[in]         : True/False,to enable/disable the feature
* @return             : Core::<StatusCode>
*/
Core::hresult RDKWindowManagerImplementation::EnableInactivityReporting(const bool enable)
{
    Core::hresult status = Core::ERROR_GENERAL;

    LOGINFO("EnableInactivityReporting Entered");

    if (false == enableInactivityReporting(enable))
    {
        LOGERR("failed to set inactivity notification");
    }
    else
    {
        status = Core::ERROR_NONE;
    }
    return status;
}

/***
* @brief Sets the inactivity time interval
* @interval[in]      :Time interval in minutes
* @return            : Core::<StatusCode>
*/
Core::hresult RDKWindowManagerImplementation::SetInactivityInterval(const uint32_t interval)
{
    Core::hresult status = Core::ERROR_GENERAL;

    LOGINFO("SetInactivityInterval Entered");

    if (false == setInactivityInterval(interval))
    {
        LOGERR("failed to reset inactivity time");
    }
    else
    {
        status = Core::ERROR_NONE;
    }
    return status;
}

/***
* @brief ResetInactivityTime:Resets the inactivity time interval
* @return            : Core::<StatusCode>
*/
Core::hresult RDKWindowManagerImplementation::ResetInactivityTime()
{
    Core::hresult status = Core::ERROR_GENERAL;

    LOGINFO("ResetInactivityTime Entered");

    if (false == resetInactivityTime())
    {
        LOGERR("failed to reset inactivity time");
    }
    else
    {
        status = Core::ERROR_NONE;
    }
    return status;
}

/***
 * @brief Enable or disable key repeats.
 * Set the key repeat functionality for input events.
 *
 * @param[in] enable : Boolean flag to enable (true) or disable (false) key repeats.
 * @return            : Core::<StatusCode>
 */
Core::hresult RDKWindowManagerImplementation::EnableKeyRepeats(bool enable)
{
    Core::hresult status = Core::ERROR_GENERAL;
    bool lockAcquired = false;

    lockAcquired = lockRdkWindowManagerMutex();
    if (lockAcquired)
    {
        if (CompositorController::enableKeyRepeats(enable))
        {
            status = Core::ERROR_NONE;
        }
        else
        {
            LOGERR("Failed to enabled key repeats");
        }
        gRdkWindowManagerMutex.unlock();
    }

    return status;
}

 /***
 * @brief Get the status of key repeat functionality.
 * Retrieves whether the key repeat functionality is currently enabled or disabled.
 *
 * @param[out] keyRepeat : the key repeat status(true if enabled, false if disabled).
 * @return            : Core::<StatusCode>
 */
Core::hresult RDKWindowManagerImplementation::GetKeyRepeatsEnabled(bool &keyRepeat) const
{
    Core::hresult status = Core::ERROR_GENERAL;
    bool lockAcquired = false;

    lockAcquired = lockRdkWindowManagerMutex();
    if (lockAcquired)
    {
        if (CompositorController::getKeyRepeatsEnabled(keyRepeat))
        {
            status = Core::ERROR_NONE;
        }
        else
        {
            LOGERR("Failed to retrieve key repeat status");
        }
        gRdkWindowManagerMutex.unlock();
    }

    return status;
}

/***
 * @brief Enable or disable key input handling.
 * Set whether key inputs should be ignored or processed.
 *
 * @param[in] ignore : true to 'ignore' or false to 'not ignore' key inputs.
 * @return            : Core::<StatusCode>
 */
Core::hresult RDKWindowManagerImplementation::IgnoreKeyInputs(bool ignore)
{
    Core::hresult status = Core::ERROR_GENERAL;
    bool lockAcquired = false;

    lockAcquired = lockRdkWindowManagerMutex();
    if (lockAcquired)
    {
        if (CompositorController::ignoreKeyInputs(ignore))
        {
            status = Core::ERROR_NONE;
        }
        else
        {
            LOGERR("Failed to ignore key inputs");
        }
        gRdkWindowManagerMutex.unlock();
    }
    return status;
}

 /***
 * @brief Enable or disable input events for specified clients.
 * Controls whether input events are processed or ignored for specified clients.
 *
 * @param[in] clients : JSON String format specifying the clients for which input events should be enabled or disabled.
 *                      Example: {"clients": ["testapp"]}
 * @param[in] enable  : flag to enable (true) or disable (false) input events for the specified clients.
 * @return            : Core::<StatusCode>
 */
Core::hresult RDKWindowManagerImplementation::EnableInputEvents(const string &clients, bool enable)
{
    Core::hresult status = Core::ERROR_GENERAL;
    bool result = true;
    JsonObject parameters;

    if (clients.empty())
    {
        LOGERR("Clients parameter is empty");
        result = false;
    }

    else if (!parameters.FromString(clients))
    {
        LOGERR("Invalid JSON string in clients parameter");
        result = false;
    }

    else
    {
        const JsonArray clientArray = parameters.HasLabel("clients") ? parameters["clients"].Array() : JsonArray();
        gRdkWindowManagerMutex.lock();
        if (clientArray.Length() > 0)
        {
            for (int i = 0; i < clientArray.Length(); i++)
            {
                if (clientArray[i].Content() != JsonValue::type::STRING)
                {
                    LOGWARN("Skipping client at index %d due to invalid format", i);
                    result = false;
                    continue;
                }

                const string clientName = clientArray[i].String();

                if (clientName == "*")
                {
                    std::vector<std::string> clientList;
                    CompositorController::getClients(clientList);
                    for (const auto& client : clientList)
                    {
                        result = result && CompositorController::enableInputEvents(client, enable);
                    }
                    break;
                }
                else
                {
                    result = result && CompositorController::enableInputEvents(clientName, enable);
                }
            }
        }
        else
        {
            LOGERR("'clientArray is missing");
            result = false;
        }

        gRdkWindowManagerMutex.unlock();

        if (!result)
        {
            LOGERR("Failed to Enable/Disable input events");
        }
        else
        {
            status = Core::ERROR_NONE;
        }
    }
    return status;
}

/***
 * @brief Configure key repeat settings.
 * Allows enabling or disabling key repeat functionality and setting the initial delay and repeat interval.
 *
 * @param[in] input     : Specifies the type of input device for the configuration.
 * @param[in] keyConfig : JSON String format specifying the key repeat configuration.
 *                        Example: {"input": "keyboard","keyConfig": "{\"enabled\":true,\"initialDelay\":500,\"repeatInterval\":100}}
 *
 * @return            : Core::<StatusCode>
 */
Core::hresult RDKWindowManagerImplementation::KeyRepeatConfig(const string &input, const string &keyConfig)
{
    Core::hresult status = Core::ERROR_GENERAL;
    bool enabled = false;
    int32_t initialDelay = 0;
    int32_t repeatInterval = 0;

    JsonObject config;
    config.FromString(keyConfig);

    if (!input.empty() && input != "default" && input != "keyboard")
    {
        LOGERR("Unsupported input type: %s", input.c_str());
    }
    else if (!config.HasLabel("enabled"))
    {
        LOGERR("Missing 'enabled' parameter in keyConfig");
    }
    else if (!config.HasLabel("initialDelay"))
    {
        LOGERR("Missing 'initialDelay' parameter in keyConfig");
    }
    else if (!config.HasLabel("repeatInterval"))
    {
        LOGERR("Missing 'repeatInterval' parameter in keyConfig");
    }
    else
    {
        enabled = config["enabled"].Boolean();
        initialDelay = config["initialDelay"].Number();
        repeatInterval = config["repeatInterval"].Number();

        gRdkWindowManagerMutex.lock();
        CompositorController::setKeyRepeatConfig(enabled, initialDelay, repeatInterval);
        gRdkWindowManagerMutex.unlock();

        status = Core::ERROR_NONE;
    }

    return status;
}

/***
 * @brief Create the display window.
 * Creates a display for the specified client using the configuration parameters.
 *
 * @client[in]        : client/callsign, Ex: westerostest, org.rdk.YouTube
 * @displayName[in]   : Optional - Name of the display
 * @displayWidth[in]  : Optional - width of the creating display
 * @displayWidth[in]  : Optional - width of the creating display
 * @displayHeight[in] : Optional - height of the creating display
 * @virtualDisplay[in]: Optional - virtual display is required or not
 * @virtualWidth[in]  : Optional - width of the virtual display
 * @virtualHeight[in] : Optional - height of the virtual display
 * @topmost[in]       : Optional - topmost is required or not true/false
 * @focus[in]         : Optional - focus is required or not
 * @return            : Optional - true/false
 */
bool RDKWindowManagerImplementation::createDisplay(const string& client, const string& displayName, const uint32_t displayWidth, const uint32_t displayHeight,
                                                   const bool virtualDisplay, const uint32_t virtualWidth, const uint32_t virtualHeight,
                                                   const bool topmost, const bool focus)
{
    bool ret = false;

    if (!isClientExists(client))
    {
        bool lockAcquired = false;
        gRdkWindowManagerMutex.lock();
        std::shared_ptr<CreateDisplayRequest> request = std::make_shared<CreateDisplayRequest>(client, displayName, displayWidth, displayHeight, virtualDisplay, virtualWidth, virtualHeight, topmost, focus);
        gCreateDisplayRequests.push_back(request);
        gRdkWindowManagerMutex.unlock();

        if (-1 == sem_wait(&request->mSemaphore))
        {
            LOGERR("CreateDisplayRequest: sem_wait failed: %s", strerror(errno));
        }
        ret = request->mResult;

        if (ret)
        {
            lockAcquired = lockRdkWindowManagerMutex();
            if (lockAcquired)
            {
                if (false == RdkWindowManager::CompositorController::addListener(client, mEventListener))
                {
                    LOGERR("CompositorController::addListener Failed");
                }
                gRdkWindowManagerMutex.unlock();
            }
        }
    }
    else
    {
        LOGERR("Client: %s already exist", client.c_str());
    }

    return ret;
}

bool RDKWindowManagerImplementation::enableInactivityReporting(const bool enable)
{
    bool lockAcquired = false;

    lockAcquired = lockRdkWindowManagerMutex();
    if (lockAcquired)
    {
       RdkWindowManager::CompositorController::enableInactivityReporting(enable);
       gRdkWindowManagerMutex.unlock();
    }
    return true;
}

bool RDKWindowManagerImplementation::addKeyIntercept(const uint32_t& keyCode, const JsonArray& modifiers, const string& client)
{
    uint32_t flags = 0;
    for (unsigned int i=0; i<modifiers.Length(); i++) {
        flags |= getKeyFlag(modifiers[i].String());
    }
    bool ret = false;
    gRdkWindowManagerMutex.lock();
    ret = RdkWindowManager::CompositorController::addKeyIntercept(client, keyCode, flags);
    gRdkWindowManagerMutex.unlock();
    return ret;
}

bool RDKWindowManagerImplementation::addKeyIntercepts(const JsonArray& intercepts)
{
    bool ret = false;
    for (unsigned int i=0; i<intercepts.Length(); i++)
    {
        if (!(intercepts[i].Content() == JsonValue::type::OBJECT))
        {
            LOGWARN("ignoring entry %d due to wrong format",i+1);
            continue;
        }
        const JsonObject& interceptEntry = intercepts[i].Object();
        if (!interceptEntry.HasLabel("keys") || !interceptEntry.HasLabel("client"))
        {
            LOGWARN("ignoring entry %d due to missing client or keys parameter",i+1);
            continue;
        }
        const JsonArray& keys = interceptEntry["keys"].Array();
        std::string client = interceptEntry["client"].String();
        for (unsigned int k=0; k<keys.Length(); k++)
        {
            if (!(keys[k].Content() == JsonValue::type::OBJECT))
            {
                LOGWARN("ignoring key  %d in entry  %d due to wrong format", k+1, i+1);
                continue;
            }
            const JsonObject& keyEntry = keys[k].Object();
            if (!keyEntry.HasLabel("keyCode"))
            {
                LOGWARN("ignoring key  %d in entry  %d due to missing key code parameter ", k+1, i+1);
                continue;
            }
            const JsonArray modifiers = keyEntry.HasLabel("modifiers") ? keyEntry["modifiers"].Array() : JsonArray();
            const uint32_t keyCode = keyEntry["keyCode"].Number();
            ret = addKeyIntercept(keyCode, modifiers, client);
        }
    }
    return ret;
}

bool RDKWindowManagerImplementation::removeKeyIntercept(const uint32_t& keyCode, const JsonArray& modifiers, const string& client)
{
    uint32_t flags = 0;
    for (unsigned int i=0; i<modifiers.Length(); i++)
    {
        flags |= getKeyFlag(modifiers[i].String());
    }
    bool ret = false;
    gRdkWindowManagerMutex.lock();
    ret = RdkWindowManager::CompositorController::removeKeyIntercept(client, keyCode, flags);
    gRdkWindowManagerMutex.unlock();
    return ret;
}

bool RDKWindowManagerImplementation::addKeyListeners(const string& client, const JsonArray& keys)
{
    gRdkWindowManagerMutex.lock();

    bool result = true;

    for (unsigned int i=0; i<keys.Length(); i++)
    {
        result = false;
        const JsonObject& keyInfo = keys[i].Object();

        if (keyInfo.HasLabel("keyCode") && keyInfo.HasLabel("nativeKeyCode"))
        {
            LOGERR("keyCode and nativeKeyCode can't be set both at the same time");
        }
        else if (keyInfo.HasLabel("keyCode") || keyInfo.HasLabel("nativeKeyCode"))
        {
            uint32_t keyCode = 0;

            if (keyInfo.HasLabel("keyCode"))
            {
                std::string keystring = keyInfo["keyCode"].String();
                if (keystring.compare("*") == 0)
                {
                    keyCode = ANY_KEY;
                }
                else
                {
                    keyCode = keyInfo["keyCode"].Number();
                }
            }
            else
            {
                std::string keystring = keyInfo["nativeKeyCode"].String();
                if (keystring.compare("*") == 0)
                {
                    keyCode = ANY_KEY;
                }
                else
                {
                    keyCode = keyInfo["nativeKeyCode"].Number();
                }
            }
            const JsonArray modifiers = keyInfo.HasLabel("modifiers") ? keyInfo["modifiers"].Array() : JsonArray();
            uint32_t flags = 0;
            for (unsigned int i=0; i<modifiers.Length(); i++)
            {
                flags |= getKeyFlag(modifiers[i].String());
            }
            std::map<std::string, RdkWindowManagerData> properties;
            if (keyInfo.HasLabel("activate"))
            {
                bool activate = keyInfo["activate"].Boolean();
                properties["activate"] = activate;
            }
            if (keyInfo.HasLabel("propagate"))
            {
                bool propagate = keyInfo["propagate"].Boolean();
                properties["propagate"] = propagate;
            }

            if (keyInfo.HasLabel("keyCode"))
            {
                result = RdkWindowManager::CompositorController::addKeyListener(client, keyCode, flags, properties);
            }
            else
            {
                result = RdkWindowManager::CompositorController::addNativeKeyListener(client, keyCode, flags, properties);
            }
        }
        else
        {
            LOGERR("Neither keyCode nor nativeKeyCode provided");
        }

        if (result == false)
        {
            break;
        }
    }
    gRdkWindowManagerMutex.unlock();
    return result;
}

bool RDKWindowManagerImplementation::removeKeyListeners(const string& client, const JsonArray& keys)
{
    gRdkWindowManagerMutex.lock();

    bool result = true;

    for (unsigned int i=0; i<keys.Length(); i++)
    {
        result = false;
        const JsonObject& keyInfo = keys[i].Object();

        if (keyInfo.HasLabel("keyCode") && keyInfo.HasLabel("nativeKeyCode"))
        {
            LOGERR("keyCode and nativeKeyCode can't be set both at the same time");
        }
        else if (keyInfo.HasLabel("keyCode") || keyInfo.HasLabel("nativeKeyCode"))
        {
            uint32_t keyCode = 0;
            if (keyInfo.HasLabel("keyCode"))
            {
                std::string keystring = keyInfo["keyCode"].String();
                if (keystring.compare("*") == 0)
                {
                    keyCode = ANY_KEY;
                }
                else
                {
                    keyCode = keyInfo["keyCode"].Number();
                }
            }
            else
            {
                std::string keystring = keyInfo["nativeKeyCode"].String();
                if (keystring.compare("*") == 0)
                {
                    keyCode = ANY_KEY;
                }
                else
                {
                    keyCode = keyInfo["nativeKeyCode"].Number();
                }
            }

            const JsonArray modifiers = keyInfo.HasLabel("modifiers") ? keyInfo["modifiers"].Array() : JsonArray();
            uint32_t flags = 0;
            for (unsigned int i=0; i<modifiers.Length(); i++) {
                flags |= getKeyFlag(modifiers[i].String());
            }

            if (keyInfo.HasLabel("keyCode"))
            {
                result = RdkWindowManager::CompositorController::removeKeyListener(client, keyCode, flags);
            }
            else
            {
                result = RdkWindowManager::CompositorController::removeNativeKeyListener(client, keyCode, flags);
            }
        }
        else
        {
            LOGERR("Neither keyCode nor nativeKeyCode provided");
        }

        if (result == false)
        {
            break;
        }
    }
    gRdkWindowManagerMutex.unlock();
    return result;
}

bool RDKWindowManagerImplementation::injectKey(const uint32_t& keyCode, const JsonArray& modifiers)
{
    bool ret = false;
    uint32_t flags = 0;
    for (unsigned int i=0; i<modifiers.Length(); i++)
    {
        flags |= getKeyFlag(modifiers[i].String());
    }
    gRdkWindowManagerMutex.lock();
    ret = RdkWindowManager::CompositorController::injectKey(keyCode, flags);
    gRdkWindowManagerMutex.unlock();
    return ret;
}

bool RDKWindowManagerImplementation::generateKey(const string& client, const JsonArray& keyInputs)
{
    bool ret = false;
    for (unsigned int i=0; i<keyInputs.Length(); i++)
    {
        const JsonObject& keyInputInfo = keyInputs[i].Object();
        uint32_t keyCode = 0, flags = 0;
        std::string virtualKey("");
        bool lockAcquired = false;

        if (keyInputInfo.HasLabel("key"))
        {
            virtualKey = keyInputInfo["key"].String();
        }
        else if (keyInputInfo.HasLabel("keyCode"))
        {
            keyCode = keyInputInfo["keyCode"].Number();
            const JsonArray modifiers = keyInputInfo.HasLabel("modifiers") ? keyInputInfo["modifiers"].Array() : JsonArray();
            for (unsigned int k=0; k<modifiers.Length(); k++) {
            flags |= getKeyFlag(modifiers[k].String());
            }
        }
        else
        {
            continue;
        }
        const uint32_t delay = keyInputInfo["delay"].Number();
        if(delay)
        {
            sleep(delay);
        }
        std::string keyClient = keyInputInfo.HasLabel("client")? keyInputInfo["client"].String(): client;
        if (keyClient.empty())
        {
            keyClient = keyInputInfo.HasLabel("callsign")? keyInputInfo["callsign"].String(): "";
        }
        lockAcquired = lockRdkWindowManagerMutex();
        if (lockAcquired)
        {
            bool targetFound = false;
            if (keyClient != "")
            {
                std::vector<std::string> clientList;
                RdkWindowManager::CompositorController::getClients(clientList);
                std::transform(keyClient.begin(), keyClient.end(), keyClient.begin(), ::tolower);
                if (std::find(clientList.begin(), clientList.end(), keyClient) != clientList.end())
                {
                    targetFound = true;
                }
            }
            if (targetFound || keyClient == "")
            {
                ret = RdkWindowManager::CompositorController::generateKey(keyClient, keyCode, flags, std::move(virtualKey));
            }
            gRdkWindowManagerMutex.unlock();
        }
    }
    return ret;
}

bool RDKWindowManagerImplementation::setInactivityInterval(const uint32_t interval)
{
    bool lockAcquired = false;

    lockAcquired = lockRdkWindowManagerMutex();
    try
    {
        RdkWindowManager::CompositorController::setInactivityInterval((double)interval);
        LOGINFO("RDKWindowManager setInactivity for Interval:%d",interval);
    }
    catch (...)
    {
        LOGERR("RDKWindowManager unable to set inactivity interval  ");
    }
    if (lockAcquired)
    {
        gRdkWindowManagerMutex.unlock();
    }
    return true;
}

bool RDKWindowManagerImplementation::resetInactivityTime()
{
    bool lockAcquired = false;

    lockAcquired = lockRdkWindowManagerMutex();
    try
    {
        RdkWindowManager::CompositorController::resetInactivityTime();
        LOGINFO("RDKWindowManager inactivity time reset");
    }
    catch (...)
    {
        LOGERR("RDKWindowManager unable to reset inactivity time  ");
    }
    if (lockAcquired)
    {
        gRdkWindowManagerMutex.unlock();
    }
    return true;
}
} /* namespace Plugin */
} /* namespace WPEFramework */
