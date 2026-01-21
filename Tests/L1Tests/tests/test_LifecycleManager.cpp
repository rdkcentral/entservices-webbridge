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
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <fstream>
#include <string>
#include <vector>
#include <cstdio>
#include <mutex>
#include <chrono>
#include <condition_variable>

#include "LifecycleManager.h"
#include "LifecycleManagerImplementation.h"
#include "ServiceMock.h"
#include "RuntimeManagerMock.h"
#include "WindowManagerMock.h"
#include "WorkerPoolImplementation.h"

#define TEST_LOG(x, ...) fprintf(stderr, "\033[1;32m[%s:%d](%s)<PID:%d><TID:%d>" x "\n\033[0m", __FILE__, __LINE__, __FUNCTION__, getpid(), gettid(), ##__VA_ARGS__); fflush(stderr);
#define TIMEOUT   (1000)

typedef enum : uint32_t {
    LifecycleManager_invalidEvent = 0,
    LifecycleManager_onStateChangeEvent,
    LifecycleManager_onRuntimeManagerEvent,
    LifecycleManager_onWindowManagerEvent,
    LifecycleManager_onRippleEvent
} LifecycleManagerTest_events_t;

namespace WPEFramework {
namespace Plugin {
class LifecycleManagerImplementationTest : public LifecycleManagerImplementation {
    public:
        ApplicationContext* getContextImpl(const std::string& appInstanceId, const std::string& appId) const 
        {
            return LifecycleManagerImplementation::getContext(appInstanceId, appId);
        }
};
} // namespace Plugin
} // namespace WPEFramework

using ::testing::NiceMock;
using namespace WPEFramework;
using namespace std;

class EventHandlerTest : public Plugin::IEventHandler {
    public:
        string appId;
        string appInstanceId;
        Exchange::ILifecycleManager::LifecycleState oldLifecycleState;
        Exchange::ILifecycleManager::LifecycleState newLifecycleState;
        Exchange::IRuntimeManager::RuntimeState state;
        string navigationIntent;
        string errorReason;
        string name;
        string errorCode;
        string client;
        vector<string> runtimeEventName;
        vector<string> windowEventName;
        double minutes;

        mutex m_mutex;
        condition_variable m_condition_variable;
        uint32_t m_event_signal = LifecycleManager_invalidEvent;

        void onStateChangeEvent(JsonObject& data) override 
        {
            m_event_signal = LifecycleManager_onStateChangeEvent;

            EXPECT_EQ(appId, data["appId"].String());
            EXPECT_EQ(oldLifecycleState, static_cast<Exchange::ILifecycleManager::LifecycleState>(data["oldLifecycleState"].Number()));
            EXPECT_EQ(newLifecycleState, static_cast<Exchange::ILifecycleManager::LifecycleState>(data["newLifecycleState"].Number()));
            EXPECT_EQ(errorReason, data["errorReason"].String());

            m_condition_variable.notify_one();
        }

        void onRuntimeManagerEvent(JsonObject& data) override
        {
            m_event_signal = LifecycleManager_onRuntimeManagerEvent;

            EXPECT_EQ(find(runtimeEventName.begin(), runtimeEventName.end(), data["name"].String()) != runtimeEventName.end(), true);
            EXPECT_EQ(state, static_cast<Exchange::IRuntimeManager::RuntimeState>(data["state"].Number()));
            EXPECT_EQ(errorCode, data["errorCode"].String());

            m_condition_variable.notify_one();
        }

        void onWindowManagerEvent(JsonObject& data) override
        {
            m_event_signal = LifecycleManager_onWindowManagerEvent;

            EXPECT_EQ(find(windowEventName.begin(), windowEventName.end(), data["name"].String()) != windowEventName.end(), true);
            EXPECT_EQ(client, data["client"].String());
            EXPECT_EQ(minutes, data["minutes"].Double());

            m_condition_variable.notify_one();
        }

        void onRippleEvent(string name, JsonObject& data) override
        {
            m_event_signal = LifecycleManager_onRippleEvent;

            m_condition_variable.notify_one();
        }

        uint32_t WaitForEventStatus(uint32_t timeout_ms, LifecycleManagerTest_events_t status)
        {
            uint32_t event_signal = LifecycleManager_invalidEvent;
            std::unique_lock<std::mutex> lock(m_mutex);
            auto now = std::chrono::steady_clock::now();
            auto timeout = std::chrono::milliseconds(timeout_ms);
            if (m_condition_variable.wait_until(lock, now + timeout) == std::cv_status::timeout)
            {
                 TEST_LOG("Timeout waiting for request status event");
                 return m_event_signal;
            }
            event_signal = m_event_signal;
            m_event_signal = LifecycleManager_invalidEvent;
            return event_signal;
        }
};

class NotificationTest : public Exchange::ILifecycleManager::INotification 
{
    private:
        BEGIN_INTERFACE_MAP(NotificationTest)
        INTERFACE_ENTRY(Exchange::ILifecycleManager::INotification)
        END_INTERFACE_MAP
    
    public:
        void OnAppStateChanged(const std::string& appId, Exchange::ILifecycleManager::LifecycleState state, const std::string& errorReason) override {}     
};

class StateNotificationTest : public Exchange::ILifecycleManagerState::INotification 
{
    private:
        BEGIN_INTERFACE_MAP(StateNotificationTest)
        INTERFACE_ENTRY(Exchange::ILifecycleManagerState::INotification)
        END_INTERFACE_MAP

    public:
       void OnAppLifecycleStateChanged(const std::string& appId, const std::string& appInstanceId, const Exchange::ILifecycleManager::LifecycleState oldState, const Exchange::ILifecycleManager::LifecycleState newState, const std::string& navigationIntent) override {}
};

class LifecycleManagerTest : public ::testing::Test {
protected:
    string appId;
    string launchIntent;
    Exchange::ILifecycleManager::LifecycleState targetLifecycleState;
    Exchange::RuntimeConfig runtimeConfigObject;
    string launchArgs;
    string appInstanceId;
    string errorReason;
    bool success;
    vector<string> runtimeEventName;
    vector<string> windowEventName;
    string errorCode;
    Exchange::IRuntimeManager::RuntimeState state;
    string client;
    double minutes;

    Core::ProxyType<Plugin::LifecycleManagerImplementationTest> mLifecycleManagerImpl;
    EventHandlerTest eventHdlTest;
    Exchange::ILifecycleManager* interface = nullptr;
    Exchange::ILifecycleManagerState* stateInterface = nullptr;
    Exchange::IConfiguration* mLifecycleManagerConfigure = nullptr;
    RuntimeManagerMock* mRuntimeManagerMock = nullptr;
    WindowManagerMock* mWindowManagerMock = nullptr;
    ServiceMock* mServiceMock = nullptr;
    Core::ProxyType<WorkerPoolImplementation> workerPool;
    JsonObject eventData;
    uint32_t event_signal;

    LifecycleManagerTest()
	: workerPool(Core::ProxyType<WorkerPoolImplementation>::Create(
            2, Core::Thread::DefaultStackSize(), 16))
    {
        mLifecycleManagerImpl = Core::ProxyType<Plugin::LifecycleManagerImplementationTest>::Create();
        
        interface = static_cast<Exchange::ILifecycleManager*>(mLifecycleManagerImpl->QueryInterface(Exchange::ILifecycleManager::ID));

        stateInterface = static_cast<Exchange::ILifecycleManagerState*>(mLifecycleManagerImpl->QueryInterface(Exchange::ILifecycleManagerState::ID));

		Core::IWorkerPool::Assign(&(*workerPool));
		workerPool->Run();
    }

    virtual ~LifecycleManagerTest() override
    {
		interface->Release();
		stateInterface->Release();

        Core::IWorkerPool::Assign(nullptr);
		workerPool.Release();
    }
	
	void createResources() 
	{
	    // Initialize the parameters with default values
        appId = "com.test.app";
        launchIntent = "test.launch.intent";
        targetLifecycleState = Exchange::ILifecycleManager::LifecycleState::LOADING;
        launchArgs = "test.arguments";
        appInstanceId = "";
        errorReason = "";
        success = true;
        runtimeEventName = {"onTerminated", "onStateChanged", "onFailure", "onStarted"};
        windowEventName = {"onReady", "onDisconnect", "onUserInactivity"};
        errorCode = "1";
        state = Exchange::IRuntimeManager::RuntimeState::RUNTIME_STATE_SUSPENDED;
        client = "test.client";
        minutes = 24;
        
        runtimeConfigObject = {
            true,true,true,1024,512,"test.env.variables",1,1,1024,true,"test.dial.id","test.command","test.app.type","test.app.path","test.runtime.path","test.logfile.path",1024,"test.log.levels",true,"test.fkps.files","test.firebolt.version",true,"test.unpacked.path"
        };

        // Initialize event parameters and event data
        eventHdlTest.appId = appId;
        eventHdlTest.appInstanceId = appInstanceId;
        eventHdlTest.oldLifecycleState = Exchange::ILifecycleManager::LifecycleState::UNLOADED;
        eventHdlTest.newLifecycleState = targetLifecycleState;
        eventHdlTest.errorReason = errorReason;
        eventHdlTest.state = state;
        eventHdlTest.errorCode = errorCode;
        eventHdlTest.client = client;
        eventHdlTest.minutes = minutes;
        eventHdlTest.runtimeEventName = runtimeEventName;
        eventHdlTest.windowEventName = windowEventName;

        eventData["appId"] = appId;
        eventData["appInstanceId"] = appInstanceId;
        eventData["oldLifecycleState"] = static_cast<uint32_t>(Exchange::ILifecycleManager::LifecycleState::UNLOADED);
        eventData["newLifecycleState"] = static_cast<uint32_t>(targetLifecycleState);
        eventData["navigationIntent"] = launchIntent;
        eventData["errorReason"] = errorReason;
        eventData["name"] = "";
        eventData["state"] = static_cast<uint32_t>(state);
        eventData["errorCode"] = errorCode;
        eventData["client"] = client;
        eventData["minutes"] = minutes;

        event_signal = LifecycleManager_invalidEvent;
		
		// Set up mocks and expect calls
        mServiceMock = new NiceMock<ServiceMock>;
        mRuntimeManagerMock = new NiceMock<RuntimeManagerMock>;
        mWindowManagerMock = new NiceMock<WindowManagerMock>;

        mLifecycleManagerConfigure = static_cast<Exchange::IConfiguration*>(mLifecycleManagerImpl->QueryInterface(Exchange::IConfiguration::ID));
		
        EXPECT_CALL(*mServiceMock, QueryInterfaceByCallsign(::testing::_, ::testing::_))
          .Times(::testing::AnyNumber())
          .WillRepeatedly(::testing::Invoke(
              [&](const uint32_t, const std::string& name) -> void* {
                if (name == "org.rdk.RuntimeManager") {
                    return reinterpret_cast<void*>(mRuntimeManagerMock);
                } else if (name == "org.rdk.RDKWindowManager") {
                   return reinterpret_cast<void*>(mWindowManagerMock);
                } 
            return nullptr;
        }));

		EXPECT_CALL(*mServiceMock, AddRef())
          .Times(::testing::AnyNumber());
	
		EXPECT_CALL(*mRuntimeManagerMock, Register(::testing::_))
          .WillRepeatedly(::testing::Return(Core::ERROR_NONE));
    
		EXPECT_CALL(*mWindowManagerMock, Register(::testing::_))
          .WillRepeatedly(::testing::Return(Core::ERROR_NONE));

        // Configure the LifecycleManager
        mLifecycleManagerConfigure->Configure(mServiceMock);
		
		ASSERT_TRUE(interface != nullptr);  
    }

    void releaseResources()
    {
	    // Clean up mocks
		if (mServiceMock != nullptr)
        {
			EXPECT_CALL(*mServiceMock, Release())
              .WillOnce(::testing::Invoke(
              [&]() {
						delete mServiceMock;
						mServiceMock = nullptr;
						return 0;
					}));    
        }

        if (mRuntimeManagerMock != nullptr)
        {
			EXPECT_CALL(*mRuntimeManagerMock, Unregister(::testing::_))
              .WillRepeatedly(::testing::Return(Core::ERROR_NONE));

			EXPECT_CALL(*mRuntimeManagerMock, Release())
              .WillOnce(::testing::Invoke(
              [&]() {
						delete mRuntimeManagerMock;
						mRuntimeManagerMock = nullptr;
						return 0;
					}));
        }

        if (mWindowManagerMock != nullptr)
        {
			EXPECT_CALL(*mWindowManagerMock, Unregister(::testing::_))
              .WillRepeatedly(::testing::Return(Core::ERROR_NONE));

			EXPECT_CALL(*mWindowManagerMock, Release())
              .WillOnce(::testing::Invoke(
              [&]() {
						delete mWindowManagerMock;
						mWindowManagerMock = nullptr;
						return 0;
                    }));
        }

		// Clean up the LifecycleManager
        mLifecycleManagerConfigure->Release();
		
		ASSERT_TRUE(interface != nullptr); 
    }

    void onStateChangeEventSignal() 
    {
        event_signal = LifecycleManager_invalidEvent;

        eventHdlTest.onStateChangeEvent(eventData);

        event_signal = eventHdlTest.WaitForEventStatus(TIMEOUT, LifecycleManager_onStateChangeEvent);

        EXPECT_TRUE(event_signal & LifecycleManager_onStateChangeEvent);
    }

    void onRuntimeManagerEventSignal(JsonObject data) 
    {
        eventData["name"] = data["name"];

        event_signal = LifecycleManager_invalidEvent;

        eventHdlTest.onRuntimeManagerEvent(eventData);

        event_signal = eventHdlTest.WaitForEventStatus(TIMEOUT, LifecycleManager_onRuntimeManagerEvent);

        EXPECT_TRUE(event_signal & LifecycleManager_onRuntimeManagerEvent);
    }

    void onWindowManagerEventSignal(JsonObject data) 
    {
        eventData["name"] = data["name"];

        event_signal = LifecycleManager_invalidEvent;

        eventHdlTest.onWindowManagerEvent(eventData);

        event_signal = eventHdlTest.WaitForEventStatus(TIMEOUT, LifecycleManager_onWindowManagerEvent);

        EXPECT_TRUE(event_signal & LifecycleManager_onWindowManagerEvent);
    }
};

/* Test Case for Registering and Unregistering Notification
 * 
 * Set up Lifecycle Manager interface, configurations, required COM-RPC resources, mocks and expectations
 * Create a notification instance using the NotificationTest class
 * Register the notification with the Lifecycle Manager interface
 * Verify successful registration of notification by asserting that Register() returns Core::ERROR_NONE
 * Unregister the notification from the Lifecycle Manager interface
 * Verify successful unregistration of notification by asserting that Unregister() returns Core::ERROR_NONE
 * Release the Lifecycle Manager interface object and clean-up related test resources
 */

TEST_F(LifecycleManagerTest, unregisterNotification_afterRegister)
{
    createResources();

    Core::Sink<NotificationTest> notification;

    // TC-1: Check if the notification is unregistered after registering
    EXPECT_EQ(Core::ERROR_NONE, interface->Register(&notification));

    EXPECT_EQ(Core::ERROR_NONE, interface->Unregister(&notification));

    releaseResources();
}

/* Test Case for Unregistering Notification without registering
 * 
 * Set up Lifecycle Manager interface, configurations, required COM-RPC resources, mocks and expectations
 * Create a notification instance using the NotificationTest class
 * Unregister the notification from the Lifecycle Manager interface
 * Verify unregistration of notification fails by asserting that Unregister() returns Core::ERROR_GENERAL
 * Release the Lifecycle Manager interface object and clean-up related test resources
 */

TEST_F(LifecycleManagerTest, unregisterNotification_withoutRegister)
{
    createResources();

    Core::Sink<NotificationTest> notification;
	
	// TC-2: Check if the notification is unregistered without registering
    EXPECT_EQ(Core::ERROR_GENERAL, interface->Unregister(&notification));

    releaseResources();
}

/* Test Case for Registering and Unregistering State Notification 
 * 
 * Set up Lifecycle Manager state interface, configurations, required COM-RPC resources, mocks and expectations
 * Create a state notification instance using the StateNotificationTest class
 * Register the state notification with the Lifecycle Manager state interface
 * Verify successful registration of state notification by asserting that Register() returns Core::ERROR_NONE
 * Unregister the state notification from the Lifecycle Manager state interface
 * Verify successful unregistration of state notification by asserting that Unregister() returns Core::ERROR_NONE
 * Release the Lifecycle Manager state interface object and clean-up related test resources
 */

TEST_F(LifecycleManagerTest, unregisterStateNotification_afterRegister)
{
    createResources();

    Core::Sink<StateNotificationTest> stateNotification;

    // TC-3: Check if the state notification is registered after unregistering
    EXPECT_EQ(Core::ERROR_NONE, stateInterface->Register(&stateNotification));

    EXPECT_EQ(Core::ERROR_NONE, stateInterface->Unregister(&stateNotification));
    
    releaseResources();
}

/* Test Case for Unregistering State Notification without registering
 * 
 * Set up Lifecycle Manager state interface, configurations, required COM-RPC resources, mocks and expectations
 * Create a state notification instance using the StateNotificationTest class
 * Unregister the state notification from the Lifecycle Manager state interface
 * Verify unregistration of state notification fails by asserting that Unregister() returns Core::ERROR_GENERAL
 * Release the Lifecycle Manager state interface object and clean-up related test resources
 */

TEST_F(LifecycleManagerTest, unregisterStateNotification_withoutRegister)
{
    createResources();

    Core::Sink<StateNotificationTest> stateNotification;
	
	// TC-4: Check if the state notification is registered without unregistering
    EXPECT_EQ(Core::ERROR_GENERAL, stateInterface->Unregister(&stateNotification));

    releaseResources();
}

/* Test Case for Spawning an App
 * 
 * Set up Lifecycle Manager interface, configurations, required COM-RPC resources, mocks and expectations
 * Spawn an app with valid parameters with target state as LOADING
 * Verify successful spawn by asserting that SpawnApp() returns Core::ERROR_NONE
 * Handle event signals by calling the onStateChangeonStateChangeEventSignal() method
 * Release the Lifecycle Manager objects and clean-up related test resources
 */

TEST_F(LifecycleManagerTest, spawnApp_withValidParams)
{    
    createResources();

    // TC-5: Spawn an app with all parameters valid
    EXPECT_EQ(Core::ERROR_NONE, interface->SpawnApp(appId, launchIntent, targetLifecycleState, runtimeConfigObject, launchArgs, appInstanceId, errorReason, success));

    onStateChangeEventSignal();

    releaseResources();
}

/* Test Case for App Ready after Spawning
 * 
 * Set up Lifecycle Manager interface, state interface, configurations, required COM-RPC resources, mocks and expectations
 * Spawn an app with valid parameters with target state as LOADING.
 * Verify successful spawn by asserting that SpawnApp() returns Core::ERROR_NONE
 * Handle event signals by calling the onStateChangeEventSignal() method
 * Check if the app is ready after spawning with the appId 
 * Verify that the app is ready by asserting that AppReady() returns Core::ERROR_NONE
 * Obtain the loaded app context using getContextImpl() and wait for the app ready semaphore
 * Release the Lifecycle Manager objects and clean-up related test resources
 */

TEST_F(LifecycleManagerTest, appready_onSpawnAppSuccess) 
{
    createResources();

    EXPECT_EQ(Core::ERROR_NONE, interface->SpawnApp(appId, launchIntent, targetLifecycleState, runtimeConfigObject, launchArgs, appInstanceId, errorReason, success));

    onStateChangeEventSignal();
    
	// TC-6: Check if app is ready after spawning
    EXPECT_EQ(Core::ERROR_NONE, stateInterface->AppReady(appId));

    Plugin::ApplicationContext* context = mLifecycleManagerImpl->getContextImpl("", appId);

    sem_wait(&context->mAppReadySemaphore);
    
    releaseResources();
}

/* Test Case for App Ready with invalid AppId after Spawning 
 * 
 * Set up Lifecycle Manager interface, state interface, configurations, required COM-RPC resources, mocks and expectations
 * Spawn an app with valid parameters with target state as LOADING
 * Verify successful spawn by asserting that SpawnApp() returns Core::ERROR_NONE
 * Handle event signals by calling the onStateChangeEventSignal() method
 * Check failure of app ready due to invalid appId by asserting that AppReady() returns Core::ERROR_GENERAL
 * Release the Lifecycle Manager objects and clean-up related test resources
 */

TEST_F(LifecycleManagerTest, appready_oninvalidAppId) 
{
    createResources();

    EXPECT_EQ(Core::ERROR_NONE, interface->SpawnApp(appId, launchIntent, targetLifecycleState, runtimeConfigObject, launchArgs, appInstanceId, errorReason, success));

    onStateChangeEventSignal();
	
	// TC-7: Verify error on passing an invalid appId
    EXPECT_EQ(Core::ERROR_GENERAL, stateInterface->AppReady(""));

    releaseResources();
}

/* Test Case for querying if App is Loaded after Spawning
 * 
 * Set up Lifecycle Manager interface, configurations, required COM-RPC resources, mocks and expectations
 * Spawn an app with valid parameters  with target state as LOADING
 * Verify successful spawn by asserting that SpawnApp() returns Core::ERROR_NONE
 * Handle event signals by calling the onStateChangeEventSignal() method
 * Check if the app is loaded after spawning with the appId
 * Verify that the app is loaded by asserting that IsAppLoaded() returns Core::ERROR_NONE
 * Check that the loaded flag is set to true, confirming that the app is loaded
 * Release the Lifecycle Manager object and clean-up related test resources
 */

TEST_F(LifecycleManagerTest, isAppLoaded_onSpawnAppSuccess) 
{
    createResources();

    bool loaded = false;
    
    EXPECT_EQ(Core::ERROR_NONE, interface->SpawnApp(appId, launchIntent, targetLifecycleState, runtimeConfigObject, launchArgs, appInstanceId, errorReason, success));

    onStateChangeEventSignal();

	// TC-8: Check if app is loaded after spawning
    EXPECT_EQ(Core::ERROR_NONE, interface->IsAppLoaded(appId, loaded));

    EXPECT_EQ(loaded, true);    

    releaseResources();
}

/* Test Case for querying if App is Loaded with invalid AppId after Spawning
 * 
 * Set up Lifecycle Manager interface, configurations, required COM-RPC resources, mocks and expectations
 * Spawn an app with valid parameters with target state as LOADING
 * Verify successful spawn by asserting that SpawnApp() returns Core::ERROR_NONE
 * Handle event signals by calling the onStateChangeEventSignal() method
 * Check failure of app loaded due to invalid appId by asserting that IsAppLoaded() returns Core::ERROR_GENERAL
 * Check that the loaded flag is set to false, confirming that the app is not loaded
 * Release the Lifecycle Manager object and clean-up related test resources
 */

TEST_F(LifecycleManagerTest, isAppLoaded_oninvalidAppId)
{
    createResources();

    bool loaded = true;

    EXPECT_EQ(Core::ERROR_NONE, interface->SpawnApp(appId, launchIntent, targetLifecycleState, runtimeConfigObject, launchArgs, appInstanceId, errorReason, success));

    onStateChangeEventSignal();
	
	// TC-9: Verify error on passing an invalid appId
    EXPECT_EQ(Core::ERROR_NONE, interface->IsAppLoaded("", loaded));

    EXPECT_EQ(loaded, false);

    releaseResources();
}

/* Test Case for getLoadedApps with verbose enabled after Spawning
 * 
 * Set up Lifecycle Manager interface, configurations, required COM-RPC resources, mocks and expectations
 * Enable the verbose flag by setting it to true
 * Spawn an app with valid parameters with target state as LOADING
 * Verify successful spawn by asserting that SpawnApp() returns Core::ERROR_NONE
 * Handle event signals by calling the onStateChangeEventSignal() method
 * Obtain the loaded apps and assert that GetLoadedApps() returns Core::ERROR_NONE
 * Verify the app list parameters by comparing the obtained and expected appId
 * Release the Lifecycle Manager object and clean-up related test resources
 */

TEST_F(LifecycleManagerTest, getLoadedApps_verboseEnabled)
{
    createResources();

    bool verbose = true;
    string apps = "";

    EXPECT_EQ(Core::ERROR_NONE, interface->SpawnApp(appId, launchIntent, targetLifecycleState, runtimeConfigObject, launchArgs, appInstanceId, errorReason, success));

    onStateChangeEventSignal();
	
	// TC-10: Get loaded apps with verbose enabled
    EXPECT_EQ(Core::ERROR_NONE, interface->GetLoadedApps(verbose, apps)); 
    
    EXPECT_THAT(apps, ::testing::HasSubstr("\"appId\":\"com.test.app\""));

    releaseResources();
}

/* Test Case for getLoadedApps with verbose disabled after Spawning
 * 
 * Set up Lifecycle Manager interface, configurations, required COM-RPC resources, mocks and expectations
 * Disable the verbose flag by setting it to false
 * Spawn an app with valid parameters with target state as LOADING
 * Verify successful spawn by asserting that SpawnApp() returns Core::ERROR_NONE
 * Handle event signals by calling the onStateChangeEventSignal() method
 * Obtain the loaded apps and assert that GetLoadedApps() returns Core::ERROR_NONE
 * Verify the app list parameters by comparing the obtained and expected app list
 * Release the Lifecycle Manager object and clean-up related test resources
 */

TEST_F(LifecycleManagerTest, getLoadedApps_verboseDisabled)
{
    createResources();

    bool verbose = false;
    string apps = "";

    EXPECT_EQ(Core::ERROR_NONE, interface->SpawnApp(appId, launchIntent, targetLifecycleState, runtimeConfigObject, launchArgs, appInstanceId, errorReason, success));

    onStateChangeEventSignal();
	
	// TC-11: Get loaded apps with verbose disabled
    EXPECT_EQ(Core::ERROR_NONE, interface->GetLoadedApps(verbose, apps)); 

    EXPECT_THAT(apps, ::testing::HasSubstr("\"appId\":\"com.test.app\""));
    
    releaseResources();
}

/* Test Case for getLoadedApps with verbose enabled without Spawning
 * 
 * Set up Lifecycle Manager interface, configurations, required COM-RPC resources, mocks and expectations
 * Enable the verbose flag by setting it to true
 * Obtain the loaded apps and assert that GetLoadedApps() returns Core::ERROR_NONE
 * Verify the app list parameters is empty indicating no apps are loaded
 * Release the Lifecycle Manager object and clean-up related test resources
 */

TEST_F(LifecycleManagerTest, getLoadedApps_noAppsLoaded)
{
    createResources();

    bool verbose = true;
    string apps = "";

    // TC-12: Check that no apps are loaded
    EXPECT_EQ(Core::ERROR_NONE, interface->GetLoadedApps(verbose, apps));

    EXPECT_EQ(apps, "[]");

    releaseResources();
}

/* Test Case for setTargetAppState with valid parameters
 * 
 * Set up Lifecycle Manager interface, configurations, required COM-RPC resources, mocks and expectations
 * Spawn an app with valid parameters with target state as LOADING
 * Verify successful spawn by asserting that SpawnApp() returns Core::ERROR_NONE
 * Handle event signals by calling the onStateChangeEventSignal() method
 * Set the target state of the app from LOADING to ACTIVE with valid parameters
 * Verify successful state change by asserting that SetTargetAppState() returns Core::ERROR_NONE
 * Handle event signals by calling the onStateChangeEventSignal() method
 * Repeat the same process with only required parameters valid
 * Release the Lifecycle Manager objects and clean-up related test resources
 */

TEST_F(LifecycleManagerTest, setTargetAppState_withValidParams)
{
    createResources();

    EXPECT_CALL(*mRuntimeManagerMock, Run(appId, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string& appId, const string& appInstanceId, const uint32_t userId, const uint32_t groupId, Exchange::IRuntimeManager::IValueIterator* const& ports, Exchange::IRuntimeManager::IStringIterator* const& paths, Exchange::IRuntimeManager::IStringIterator* const& debugSettings, const Exchange::RuntimeConfig& runtimeConfigObject) {
                return Core::ERROR_NONE;
          }));

    EXPECT_CALL(*mWindowManagerMock, RenderReady(::testing::_, ::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string& client, bool &status) {
                return Core::ERROR_NONE;
          }));

    EXPECT_EQ(Core::ERROR_NONE, interface->SpawnApp(appId, launchIntent, targetLifecycleState, runtimeConfigObject, launchArgs, appInstanceId, errorReason, success));

    onStateChangeEventSignal();

    targetLifecycleState = Exchange::ILifecycleManager::LifecycleState::ACTIVE;

    // TC-13: Set the target state of a loaded app with all parameters valid
    EXPECT_EQ(Core::ERROR_NONE, interface->SetTargetAppState(appInstanceId, targetLifecycleState, launchIntent));

    onStateChangeEventSignal();

    // TC-14: Set the target state of a loaded app with only required parameters valid
    EXPECT_EQ(Core::ERROR_NONE, interface->SetTargetAppState(appInstanceId, targetLifecycleState, ""));
    
    onStateChangeEventSignal();

    releaseResources();
}

/* Test Case for setTargetAppState with invalid parameters
 * 
 * Set up Lifecycle Manager interface, configurations, required COM-RPC resources, mocks and expectations
 * Spawn an app with valid parameters with target state as LOADING
 * Verify successful spawn by asserting that SpawnApp() returns Core::ERROR_NONE
 * Handle event signals by calling the onStateChangeEventSignal() method
 * Set the target state of the app to LOADING with invalid appInstanceId
 * Verify state change fails by asserting that SetTargetAppState() returns Core::ERROR_GENERAL
 * Release the Lifecycle Manager object and clean-up related test resources
 */

TEST_F(LifecycleManagerTest, setTargetAppState_withinvalidParams)
{
    createResources();

    EXPECT_EQ(Core::ERROR_NONE, interface->SpawnApp(appId, launchIntent, targetLifecycleState, runtimeConfigObject, launchArgs, appInstanceId, errorReason, success));

    onStateChangeEventSignal();

    // TC-15: Set the target state of a loaded app with invalid appInstanceId
    EXPECT_EQ(Core::ERROR_GENERAL, interface->SetTargetAppState("", targetLifecycleState, launchIntent));    

    releaseResources();
}

/* Test Case for Unload App after Spawning
 * 
 * Set up Lifecycle Manager interface, configurations, required COM-RPC resources, mocks and expectations
 * Spawn an app with valid parameters with target state as LOADING
 * Verify successful spawn by asserting that SpawnApp() returns Core::ERROR_NONE
 * Handle event signals by calling the onStateChangeEventSignal() method
 * Unload the app using the appInstanceId 
 * Verify that app is successfully unloaded by asserting that UnloadApp() returns Core::ERROR_NONE
 * Handle event signals by calling the onStateChangeEventSignal() method
 * Release the Lifecycle Manager objects and clean-up related test resources
 */

TEST_F(LifecycleManagerTest, unloadApp_onSpawnAppSuccess)
{
    createResources();

    EXPECT_CALL(*mRuntimeManagerMock, Run(appId, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string& appId, const string& appInstanceId, const uint32_t userId, const uint32_t groupId, Exchange::IRuntimeManager::IValueIterator* const& ports, Exchange::IRuntimeManager::IStringIterator* const& paths, Exchange::IRuntimeManager::IStringIterator* const& debugSettings, const Exchange::RuntimeConfig& runtimeConfigObject) {
                return Core::ERROR_NONE;
          }));

    EXPECT_CALL(*mRuntimeManagerMock, Terminate(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string& appInstanceId) {
                return Core::ERROR_NONE;
          }));

    EXPECT_EQ(Core::ERROR_NONE, interface->SpawnApp(appId, launchIntent, targetLifecycleState, runtimeConfigObject, launchArgs, appInstanceId, errorReason, success));

    onStateChangeEventSignal();
    
	// TC-16: Unload the app after spawning
    EXPECT_EQ(Core::ERROR_NONE, interface->UnloadApp(appInstanceId, errorReason, success));
    
    onStateChangeEventSignal();

    releaseResources();
}

/* Test Case for Unload App without Spawning
 * 
 * Set up Lifecycle Manager interface, configurations, required COM-RPC resources, mocks and expectations
 * Set the appInstanceId to a random test value
 * Unload the app using the appInstanceId
 * Verify failure of app unload by asserting that UnloadApp() returns Core::ERROR_GENERAL
 * Release the Lifecycle Manager object and clean-up related test resources
 */

TEST_F(LifecycleManagerTest, unloadApp_withoutSpawning)
{
    createResources();

    appInstanceId = "test.app.instance";

    // TC-17: Unload the app after spawn fails
    EXPECT_EQ(Core::ERROR_GENERAL, interface->UnloadApp(appInstanceId, errorReason, success));

    releaseResources();
}

/* Test Case for Kill App after Spawning
 * 
 * Set up Lifecycle Manager interface, configurations, required COM-RPC resources, mocks and expectations
 * Spawn an app with valid parameters with target state as LOADING
 * Verify successful spawn by asserting that SpawnApp() returns Core::ERROR_NONE
 * Handle event signals by calling the onStateChangeEventSignal() method
 * Kill the app using the appInstanceId
 * Verify that app is successfully killed by asserting that KillApp() returns Core::ERROR_NONE
 * Handle event signals by calling the onStateChangeEventSignal() method
 * Release the Lifecycle Manager objects and clean-up related test resources
 */

TEST_F(LifecycleManagerTest, killApp_onSpawnAppSuccess)
{
    createResources();

    EXPECT_CALL(*mRuntimeManagerMock, Run(appId, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string& appId, const string& appInstanceId, const uint32_t userId, const uint32_t groupId, Exchange::IRuntimeManager::IValueIterator* const& ports, Exchange::IRuntimeManager::IStringIterator* const& paths, Exchange::IRuntimeManager::IStringIterator* const& debugSettings, const Exchange::RuntimeConfig& runtimeConfigObject) {
                return Core::ERROR_NONE;
          }));

    EXPECT_CALL(*mRuntimeManagerMock, Kill(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string& appInstanceId) {
                return Core::ERROR_NONE;
          }));

    EXPECT_EQ(Core::ERROR_NONE, interface->SpawnApp(appId, launchIntent, targetLifecycleState, runtimeConfigObject, launchArgs, appInstanceId, errorReason, success));

    onStateChangeEventSignal();
    
	// TC-18: Kill the app after spawning
    EXPECT_EQ(Core::ERROR_NONE, interface->KillApp(appInstanceId, errorReason, success));
    
    onStateChangeEventSignal();
    
    releaseResources();
}

/* Test Case for Kill App without Spawning
 * 
 * Set up Lifecycle Manager interface, configurations, required COM-RPC resources, mocks and expectations
 * Set the appInstanceId to a random test value
 * Kill the app using the appInstanceId 
 * Verify failure of app kill by asserting that KillApp() returns Core::ERROR_GENERAL
 * Release the Lifecycle Manager interface object and clean-up related test resources
 */

TEST_F(LifecycleManagerTest, killApp_withoutSpawning)
{
    createResources();

    appInstanceId = "test.app.instance";

    // TC-19: Kill the app after spawn fails
    EXPECT_EQ(Core::ERROR_GENERAL, interface->KillApp(appInstanceId, errorReason, success)); 
    
    releaseResources();
}

/* Test Case for Close App on User Exit
 * 
 * Set up Lifecycle Manager interface, state interface, configurations, required COM-RPC resources, mocks and expectations
 * Spawn an app with valid parameters with target state as LOADING
 * Verify successful spawn by asserting that SpawnApp() returns Core::ERROR_NONE
 * Handle event signals by calling the onStateChangeEventSignal() method
 * Close the app using the appId and setting the reason for close as USER EXIT 
 * Verify that app is successfully closed by asserting that CloseApp() returns Core::ERROR_NONE
 * Handle event signals by calling the onStateChangeEventSignal() method
 * Release the Lifecycle Manager objects and clean-up related test resources
 */

TEST_F(LifecycleManagerTest, closeApp_onUserExit) 
{
    createResources();

    EXPECT_CALL(*mRuntimeManagerMock, Run(appId, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string& appId, const string& appInstanceId, const uint32_t userId, const uint32_t groupId, Exchange::IRuntimeManager::IValueIterator* const& ports, Exchange::IRuntimeManager::IStringIterator* const& paths, Exchange::IRuntimeManager::IStringIterator* const& debugSettings, const Exchange::RuntimeConfig& runtimeConfigObject) {
                return Core::ERROR_NONE;
          }));

    EXPECT_CALL(*mRuntimeManagerMock, Kill(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string& appInstanceId) {
                return Core::ERROR_NONE;
          }));

    EXPECT_EQ(Core::ERROR_NONE, interface->SpawnApp(appId, launchIntent, targetLifecycleState, runtimeConfigObject, launchArgs, appInstanceId, errorReason, success));

    onStateChangeEventSignal();
    
	// TC-20: User exits the app after spawning 
    EXPECT_EQ(Core::ERROR_NONE, stateInterface->CloseApp(appId, Exchange::ILifecycleManagerState::AppCloseReason::USER_EXIT));

    onStateChangeEventSignal();

    releaseResources();
}

/* Test Case for Close App on Error
 * 
 * Set up Lifecycle Manager interface, state interface, configurations, required COM-RPC resources, mocks and expectations
 * Spawn an app with valid parameters with target state as LOADING
 * Verify successful spawn by asserting that SpawnApp() returns Core::ERROR_NONE
 * Handle event signals by calling the onStateChangeEventSignal() method
 * Close the app using the appId and setting the reason for close as ERROR
 * Verify that app is successfully closed by asserting that CloseApp() returns Core::ERROR_NONE
 * Handle event signals by calling the onStateChangeEventSignal() method
 * Release the Lifecycle Manager objects and clean-up related test resources
 */

TEST_F(LifecycleManagerTest, closeApp_onError) 
{
    createResources();

    EXPECT_CALL(*mRuntimeManagerMock, Run(appId, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string& appId, const string& appInstanceId, const uint32_t userId, const uint32_t groupId, Exchange::IRuntimeManager::IValueIterator* const& ports, Exchange::IRuntimeManager::IStringIterator* const& paths, Exchange::IRuntimeManager::IStringIterator* const& debugSettings, const Exchange::RuntimeConfig& runtimeConfigObject) {
                return Core::ERROR_NONE;
          }));

    EXPECT_CALL(*mRuntimeManagerMock, Kill(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string& appInstanceId) {
                return Core::ERROR_NONE;
          }));

    EXPECT_EQ(Core::ERROR_NONE, interface->SpawnApp(appId, launchIntent, targetLifecycleState, runtimeConfigObject, launchArgs, appInstanceId, errorReason, success));

    onStateChangeEventSignal();
    
	// TC-21: Error after spawning the app
    EXPECT_EQ(Core::ERROR_NONE, stateInterface->CloseApp(appId, Exchange::ILifecycleManagerState::AppCloseReason::ERROR));

    onStateChangeEventSignal();

    releaseResources();
}

/* Test Case for Close App on Kill and Run
 * 
 * Set up Lifecycle Manager interface, state interface, configurations, required COM-RPC resources, mocks and expectations
 * Spawn an app with valid parameters with target state as LOADING
 * Verify successful spawn by asserting that SpawnApp() returns Core::ERROR_NONE
 * Handle event signals by calling the onStateChangeEventSignal() method
 * Close the app using the appId and setting the reason for close as KILL AND RUN
 * Verify that app is successfully closed by asserting that CloseApp() returns Core::ERROR_NONE
 * Handle event signals by calling the onStateChangeEventSignal() method
 * Release the Lifecycle Manager objects and clean-up related test resources
 */

TEST_F(LifecycleManagerTest, closeApp_onKillandRun) 
{
    createResources();

    EXPECT_CALL(*mRuntimeManagerMock, Run(appId, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string& appId, const string& appInstanceId, const uint32_t userId, const uint32_t groupId, Exchange::IRuntimeManager::IValueIterator* const& ports, Exchange::IRuntimeManager::IStringIterator* const& paths, Exchange::IRuntimeManager::IStringIterator* const& debugSettings, const Exchange::RuntimeConfig& runtimeConfigObject) {
                return Core::ERROR_NONE;
          }));

    EXPECT_CALL(*mRuntimeManagerMock, Kill(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string& appInstanceId) {
                return Core::ERROR_NONE;
          }));

    EXPECT_EQ(Core::ERROR_NONE, interface->SpawnApp(appId, launchIntent, targetLifecycleState, runtimeConfigObject, launchArgs, appInstanceId, errorReason, success));

    onStateChangeEventSignal();
    
	// TC-22: Kill and run after spawning the app
    EXPECT_EQ(Core::ERROR_NONE, stateInterface->CloseApp(appId, Exchange::ILifecycleManagerState::AppCloseReason::KILL_AND_RUN));

    event_signal = eventHdlTest.WaitForEventStatus(TIMEOUT, LifecycleManager_onStateChangeEvent);

    EXPECT_TRUE(event_signal & LifecycleManager_onStateChangeEvent);

    onStateChangeEventSignal();

    event_signal = eventHdlTest.WaitForEventStatus(TIMEOUT, LifecycleManager_onStateChangeEvent);

    EXPECT_TRUE(event_signal & LifecycleManager_onStateChangeEvent);

    onStateChangeEventSignal();

    releaseResources();
}

/* Test Case for Close App on Kill and Activate
 * 
 * Set up Lifecycle Manager interface, state interface, configurations, required COM-RPC resources, mocks and expectations
 * Spawn an app with valid parameters with target state as LOADING
 * Verify successful spawn by asserting that SpawnApp() returns Core::ERROR_NONE
 * Handle event signals by calling the onStateChangeEventSignal() method
 * Close the app using the appId and setting the reason for close as KILL AND ACTIVATE
 * Verify that app is successfully closed by asserting that CloseApp() returns Core::ERROR_NONE
 * Handle event signals by calling the onStateChangeEventSignal() method
 * Release the Lifecycle Manager objects and clean-up related test resources
 */

TEST_F(LifecycleManagerTest, closeApp_onKillandActivate) 
{
    createResources();

    EXPECT_CALL(*mRuntimeManagerMock, Run(appId, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string& appId, const string& appInstanceId, const uint32_t userId, const uint32_t groupId, Exchange::IRuntimeManager::IValueIterator* const& ports, Exchange::IRuntimeManager::IStringIterator* const& paths, Exchange::IRuntimeManager::IStringIterator* const& debugSettings, const Exchange::RuntimeConfig& runtimeConfigObject) {
                return Core::ERROR_NONE;
          }));

    EXPECT_CALL(*mRuntimeManagerMock, Kill(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string& appInstanceId) {
                return Core::ERROR_NONE;
          }));

    EXPECT_CALL(*mWindowManagerMock, RenderReady(::testing::_, ::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string& client, bool &status) {
                return Core::ERROR_NONE;
          }));

    EXPECT_EQ(Core::ERROR_NONE, interface->SpawnApp(appId, launchIntent, targetLifecycleState, runtimeConfigObject, launchArgs, appInstanceId, errorReason, success));

    onStateChangeEventSignal();
    
	// TC-23: Kill and activate after spawning the app
    EXPECT_EQ(Core::ERROR_NONE, stateInterface->CloseApp(appId, Exchange::ILifecycleManagerState::AppCloseReason::KILL_AND_ACTIVATE));

    event_signal = eventHdlTest.WaitForEventStatus(TIMEOUT, LifecycleManager_onStateChangeEvent);

    EXPECT_TRUE(event_signal & LifecycleManager_onStateChangeEvent);

    onStateChangeEventSignal();

    event_signal = eventHdlTest.WaitForEventStatus(TIMEOUT, LifecycleManager_onStateChangeEvent);

    EXPECT_TRUE(event_signal & LifecycleManager_onStateChangeEvent);

    onStateChangeEventSignal();

    releaseResources();
}

/* Test Case for State Change Complete with valid parameters
 * 
 * Set up Lifecycle Manager state interface, configurations, required COM-RPC resources, mocks and expectations
 * Set the stateChangedId to a random test value
 * Signal that the state change is complete
 * Verify successful state change by asserting that StateChangeComplete() returns Core::ERROR_NONE
 * Release the Lifecycle Manager state object and clean-up related test resources
 */

TEST_F(LifecycleManagerTest, stateChangeComplete_withValidParams) 
{
    createResources();

    uint32_t stateChangedId = 1;
    
	// TC-24: Check if state change is complete
    EXPECT_EQ(Core::ERROR_NONE, stateInterface->StateChangeComplete(appId, stateChangedId, success));

    releaseResources();
}

/* Test Case for Send Intent to Active App after Spawning
 * 
 * Set up Lifecycle Manager interface, configurations, required COM-RPC resources, mocks and expectations
 * Set the intent to a random test value
 * Spawn an app with valid parameters with target state as ACTIVE
 * Verify successful spawn by asserting that SpawnApp() returns Core::ERROR_NONE
 * Handle event signals by calling the onStateChangeEventSignal() method
 * Send an intent to the active app using the appInstanceId 
 * Verify failure of sent intent (due to failure in websocket) by asserting that SendIntentToActiveApp() returns Core::ERROR_GENERAL
 * Release the Lifecycle Manager objects and clean-up related test resources
 */

TEST_F(LifecycleManagerTest, sendIntenttoActiveApp_onSpawnAppSuccess)
{
    createResources();

    EXPECT_CALL(*mRuntimeManagerMock, Run(appId, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string& appId, const string& appInstanceId, const uint32_t userId, const uint32_t groupId, Exchange::IRuntimeManager::IValueIterator* const& ports, Exchange::IRuntimeManager::IStringIterator* const& paths, Exchange::IRuntimeManager::IStringIterator* const& debugSettings, const Exchange::RuntimeConfig& runtimeConfigObject) {
                return Core::ERROR_NONE;
          }));

    EXPECT_CALL(*mWindowManagerMock, RenderReady(::testing::_, ::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string& client, bool &status) {
                return Core::ERROR_NONE;
          }));

    string intent = "test.intent";

    targetLifecycleState = Exchange::ILifecycleManager::LifecycleState::ACTIVE;

    EXPECT_EQ(Core::ERROR_NONE, interface->SpawnApp(appId, launchIntent, targetLifecycleState, runtimeConfigObject, launchArgs, appInstanceId, errorReason, success));

    onStateChangeEventSignal();

    // TC-25: Send intent to the app after spawning
    EXPECT_EQ(Core::ERROR_GENERAL, interface->SendIntentToActiveApp(appInstanceId, intent, errorReason, success));   

    onStateChangeEventSignal();

    releaseResources();
}

/* Test Case for Runtime Manager Event - onTerminated after Spawning
 * 
 * Set up Lifecycle Manager interface, configurations, required COM-RPC resources, mocks and expectations
 * Spawn an app with valid parameters with target state as ACTIVE
 * Verify successful spawn by asserting that SpawnApp() returns Core::ERROR_NONE
 * Handle event signals by calling the onStateChangeEventSignal() method
 * Terminate the app with the appInstanceId
 * Verify successful termination by asserting that UnloadApp() returns Core::ERROR_NONE
 * Handle event signals by calling the onStateChangeEventSignal() method
 * Populate the data by setting the event name as onTerminated along with the appInstanceId obtained
 * Signal the Runtime Manager Event using onRuntimeManagerEvent() with the data
 * Handle event signals by calling the onStateChangeEventSignal() method
 * Release the Lifecycle Manager objects and clean-up related test resources
 */

TEST_F(LifecycleManagerTest, runtimeManagerEvent_onTerminated) 
{
    createResources();

    EXPECT_CALL(*mRuntimeManagerMock, Run(appId, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string& appId, const string& appInstanceId, const uint32_t userId, const uint32_t groupId, Exchange::IRuntimeManager::IValueIterator* const& ports, Exchange::IRuntimeManager::IStringIterator* const& paths, Exchange::IRuntimeManager::IStringIterator* const& debugSettings, const Exchange::RuntimeConfig& runtimeConfigObject) {
                return Core::ERROR_NONE;
          }));

    EXPECT_CALL(*mWindowManagerMock, RenderReady(::testing::_, ::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string& client, bool &status) {
                return Core::ERROR_NONE;
          }));

    EXPECT_CALL(*mRuntimeManagerMock, Terminate(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string& appInstanceId) {
                return Core::ERROR_NONE;
          }));

    targetLifecycleState = Exchange::ILifecycleManager::LifecycleState::ACTIVE;

    EXPECT_EQ(Core::ERROR_NONE, interface->SpawnApp(appId, launchIntent, targetLifecycleState, runtimeConfigObject, launchArgs, appInstanceId, errorReason, success));

    onStateChangeEventSignal();
    
    EXPECT_EQ(Core::ERROR_NONE, interface->UnloadApp(appInstanceId, errorReason, success));

    onStateChangeEventSignal();

    JsonObject data;

    data["name"] = "onTerminated";
    data["appInstanceId"] = appInstanceId;

	// TC-26: Signal the Runtime Manager Event - onTerminated 
    mLifecycleManagerImpl->onRuntimeManagerEvent(data);

    onRuntimeManagerEventSignal(data);

    event_signal = eventHdlTest.WaitForEventStatus(TIMEOUT, LifecycleManager_onRuntimeManagerEvent);

    EXPECT_TRUE(event_signal & LifecycleManager_onRuntimeManagerEvent);
   
    releaseResources();
} 

/* Test Case for Runtime Manager Event - onStateChanged after Spawning
 * 
 * Set up Lifecycle Manager interface, configurations, required COM-RPC resources, mocks and expectations
 * Spawn an app with valid parameters with target state as INITIALIZING
 * Verify successful spawn by asserting that SpawnApp() returns Core::ERROR_NONE
 * Handle event signals by calling the onStateChangeEventSignal() method
 * Populate the data by setting the event name as onStateChanged along with the state as SUSPENDED and appInstanceId obtained 
 * Signal the Runtime Manager Event using onRuntimeManagerEvent() with the data 
 * Handle event signals by calling the onStateChangeEventSignal() method
 * Release the Lifecycle Manager objects and clean-up related test resources
 */

TEST_F(LifecycleManagerTest, runtimeManagerEvent_onStateChanged) 
{
    createResources();

    EXPECT_CALL(*mRuntimeManagerMock, Run(appId, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string& appId, const string& appInstanceId, const uint32_t userId, const uint32_t groupId, Exchange::IRuntimeManager::IValueIterator* const& ports, Exchange::IRuntimeManager::IStringIterator* const& paths, Exchange::IRuntimeManager::IStringIterator* const& debugSettings, const Exchange::RuntimeConfig& runtimeConfigObject) {
                return Core::ERROR_NONE;
          }));

    targetLifecycleState = Exchange::ILifecycleManager::LifecycleState::INITIALIZING;

    EXPECT_EQ(Core::ERROR_NONE, interface->SpawnApp(appId, launchIntent, targetLifecycleState, runtimeConfigObject, launchArgs, appInstanceId, errorReason, success));

    onStateChangeEventSignal();
 
    JsonObject data;

    data["name"] = "onStateChanged";
    data["appInstanceId"] = appInstanceId;
    data["state"] = 3;

	// TC-27: Signal the Runtime Manager Event - onStateChanged
    mLifecycleManagerImpl->onRuntimeManagerEvent(data);

    onRuntimeManagerEventSignal(data);

    event_signal = eventHdlTest.WaitForEventStatus(TIMEOUT, LifecycleManager_onRuntimeManagerEvent);

    EXPECT_TRUE(event_signal & LifecycleManager_onRuntimeManagerEvent);
    
    releaseResources();
} 

/* Test Case for Runtime Manager Event - onFailure after Spawning
 * 
 * Set up Lifecycle Manager interface, configurations, required COM-RPC resources, mocks and expectations
 * Spawn an app with valid parameters with target state as INITIALIZING
 * Verify successful spawn by asserting that SpawnApp() returns Core::ERROR_NONE
 * Handle event signals by calling the onStateChangeEventSignal() method
 * Populate the data by setting the event name as onFailure along with the error code and appInstanceId obtained 
 * Signal the Runtime Manager Event using onRuntimeManagerEvent() with the data 
 * Release the Lifecycle Manager objects and clean-up related test resources
 */

TEST_F(LifecycleManagerTest, runtimeManagerEvent_onFailure) 
{
    createResources();

    EXPECT_CALL(*mRuntimeManagerMock, Run(appId, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string& appId, const string& appInstanceId, const uint32_t userId, const uint32_t groupId, Exchange::IRuntimeManager::IValueIterator* const& ports, Exchange::IRuntimeManager::IStringIterator* const& paths, Exchange::IRuntimeManager::IStringIterator* const& debugSettings, const Exchange::RuntimeConfig& runtimeConfigObject) {
                return Core::ERROR_NONE;
          }));

    targetLifecycleState = Exchange::ILifecycleManager::LifecycleState::INITIALIZING;

    EXPECT_EQ(Core::ERROR_NONE, interface->SpawnApp(appId, launchIntent, targetLifecycleState, runtimeConfigObject, launchArgs, appInstanceId, errorReason, success));

    onStateChangeEventSignal();

    JsonObject data;

    data["name"] = "onFailure";
    data["appInstanceId"] = appInstanceId;
    data["errorCode"] = 1;

	// TC-28: Signal the Runtime Manager Event - onFailure
    mLifecycleManagerImpl->onRuntimeManagerEvent(data);

    onRuntimeManagerEventSignal(data);

    releaseResources();
} 

/* Test Case for Runtime Manager Event - onStarted after Spawning
 * 
 * Set up Lifecycle Manager interface, configurations, required COM-RPC resources, mocks and expectations
 * Spawn an app with valid parameters with target state as INITIALIZING
 * Verify successful spawn by asserting that SpawnApp() returns Core::ERROR_NONE
 * Handle event signals by calling the onStateChangeEventSignal() method
 * Populate the data by setting the event name as onStarted along with the appInstanceId obtained 
 * Signal the Runtime Manager Event using onRuntimeManagerEvent() with the data 
 * Release the Lifecycle Manager objects and clean-up related test resources
 */

TEST_F(LifecycleManagerTest, runtimeManagerEvent_onStarted) 
{
    createResources();

    EXPECT_CALL(*mRuntimeManagerMock, Run(appId, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string& appId, const string& appInstanceId, const uint32_t userId, const uint32_t groupId, Exchange::IRuntimeManager::IValueIterator* const& ports, Exchange::IRuntimeManager::IStringIterator* const& paths, Exchange::IRuntimeManager::IStringIterator* const& debugSettings, const Exchange::RuntimeConfig& runtimeConfigObject) {
                return Core::ERROR_NONE;
          }));

    targetLifecycleState = Exchange::ILifecycleManager::LifecycleState::INITIALIZING;

    EXPECT_EQ(Core::ERROR_NONE, interface->SpawnApp(appId, launchIntent, targetLifecycleState, runtimeConfigObject, launchArgs, appInstanceId, errorReason, success));

    onStateChangeEventSignal();

    JsonObject data;

    data["name"] = "onStarted";
    data["appInstanceId"] = appInstanceId;

	// TC-29: Signal the Runtime Manager Event - onStarted
    mLifecycleManagerImpl->onRuntimeManagerEvent(data);

    onRuntimeManagerEventSignal(data);

    releaseResources();
} 

/* Test Case for Window Manager Event - onUserInactivity after Spawning
 * 
 * Set up Lifecycle Manager interface, configurations, required COM-RPC resources, mocks and expectations
 * Spawn an app with valid parameters with target state as ACTIVE
 * Verify successful spawn by asserting that SpawnApp() returns Core::ERROR_NONE
 * Handle event signals by calling the onStateChangeEventSignal() method
 * Populate the data by setting the event name as onUserInactivity
 * Signal the Window Manager Event using onWindowManagerEvent() with the data 
 * Release the Lifecycle Manager objects and clean-up related test resources
 */

TEST_F(LifecycleManagerTest, windowManagerEvent_onUserInactivity) 
{
    createResources();

    EXPECT_CALL(*mRuntimeManagerMock, Run(appId, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string& appId, const string& appInstanceId, const uint32_t userId, const uint32_t groupId, Exchange::IRuntimeManager::IValueIterator* const& ports, Exchange::IRuntimeManager::IStringIterator* const& paths, Exchange::IRuntimeManager::IStringIterator* const& debugSettings, const Exchange::RuntimeConfig& runtimeConfigObject) {
                return Core::ERROR_NONE;
          }));

    EXPECT_CALL(*mWindowManagerMock, RenderReady(::testing::_, ::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string& client, bool &status) {
                return Core::ERROR_NONE;
          }));  

    targetLifecycleState = Exchange::ILifecycleManager::LifecycleState::ACTIVE;

    EXPECT_EQ(Core::ERROR_NONE, interface->SpawnApp(appId, launchIntent, targetLifecycleState, runtimeConfigObject, launchArgs, appInstanceId, errorReason, success));

    onStateChangeEventSignal();

    JsonObject data;

    data["name"] = "onUserInactivity";
    data["minutes"] = 24;

	// TC-30: Signal the Window Manager Event - onUserInactivity
    mLifecycleManagerImpl->onWindowManagerEvent(data);

    onWindowManagerEventSignal(data);

    releaseResources();
} 

/* Test Case for Window Manager Event - onDisconnect after Spawning
 * 
 * Set up Lifecycle Manager interface, configurations, required COM-RPC resources, mocks and expectations
 * Spawn an app with valid parameters with target state as ACTIVE
 * Verify successful spawn by asserting that SpawnApp() returns Core::ERROR_NONE
 * Handle event signals by calling the onStateChangeEventSignal() method
 * Populate the data by setting the event name as onDisconnect
 * Signal the Window Manager Event using onWindowManagerEvent() with the data 
 * Release the Lifecycle Manager objects and clean-up related test resources
 */

TEST_F(LifecycleManagerTest, windowManagerEvent_onDisconnect) 
{
    createResources();

    EXPECT_CALL(*mRuntimeManagerMock, Run(appId, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string& appId, const string& appInstanceId, const uint32_t userId, const uint32_t groupId, Exchange::IRuntimeManager::IValueIterator* const& ports, Exchange::IRuntimeManager::IStringIterator* const& paths, Exchange::IRuntimeManager::IStringIterator* const& debugSettings, const Exchange::RuntimeConfig& runtimeConfigObject) {
                return Core::ERROR_NONE;
          }));

    EXPECT_CALL(*mWindowManagerMock, RenderReady(::testing::_, ::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string& client, bool &status) {
                return Core::ERROR_NONE;
          }));  

    targetLifecycleState = Exchange::ILifecycleManager::LifecycleState::ACTIVE;

    EXPECT_EQ(Core::ERROR_NONE, interface->SpawnApp(appId, launchIntent, targetLifecycleState, runtimeConfigObject, launchArgs, appInstanceId, errorReason, success));

    onStateChangeEventSignal();

    JsonObject data;

    data["name"] = "onDisconnect";
    data["client"] = "test.client";

	// TC-31: Signal the Window Manager Event - onDisconnect
    mLifecycleManagerImpl->onWindowManagerEvent(data);

    onWindowManagerEventSignal(data);

    releaseResources();
} 

/* Test Case for Window Manager Event - onReady after Spawning
 * 
 * Set up Lifecycle Manager interface, configurations, required COM-RPC resources, mocks and expectations
 * Spawn an app with valid parameters with target state as ACTIVE
 * Verify successful spawn by asserting that SpawnApp() returns Core::ERROR_NONE
 * Handle event signals by calling the onStateChangeEventSignal() method
 * Populate the data by setting the event name as onReady along with the appInstanceId obtained
 * Signal the Window Manager Event using onWindowManagerEvent() with the data
 * Handle event signals by calling the onStateChangeEventSignal() method
 * Release the Lifecycle Manager objects and clean-up related test resources
 */

TEST_F(LifecycleManagerTest, windowManagerEvent_onReady) 
{
    createResources();

    EXPECT_CALL(*mRuntimeManagerMock, Run(appId, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string& appId, const string& appInstanceId, const uint32_t userId, const uint32_t groupId, Exchange::IRuntimeManager::IValueIterator* const& ports, Exchange::IRuntimeManager::IStringIterator* const& paths, Exchange::IRuntimeManager::IStringIterator* const& debugSettings, const Exchange::RuntimeConfig& runtimeConfigObject) {
                return Core::ERROR_NONE;
          }));

    EXPECT_CALL(*mWindowManagerMock, RenderReady(::testing::_, ::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string& client, bool &status) {
                return Core::ERROR_NONE;
          }));  

    targetLifecycleState = Exchange::ILifecycleManager::LifecycleState::ACTIVE;

    EXPECT_EQ(Core::ERROR_NONE, interface->SpawnApp(appId, launchIntent, targetLifecycleState, runtimeConfigObject, launchArgs, appInstanceId, errorReason, success));

    onStateChangeEventSignal();

    JsonObject data;

    data["name"] = "onReady";
    data["appInstanceId"] = appInstanceId;

	// TC-32: Signal the Window Manager Event - onReady
    mLifecycleManagerImpl->onWindowManagerEvent(data);

    onWindowManagerEventSignal(data);

    event_signal = eventHdlTest.WaitForEventStatus(TIMEOUT, LifecycleManager_onWindowManagerEvent);

    EXPECT_TRUE(event_signal & LifecycleManager_onWindowManagerEvent);

    releaseResources();
} 
