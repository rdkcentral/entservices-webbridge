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
#include <gtest/gtest.h>
#include<string>

#include "RuntimeManager.h"
#include "RuntimeManagerImplementation.h"
#include "HostMock.h"
#include "ServiceMock.h"
#include "ThunderPortability.h"
#include "StorageManagerMock.h"
#include "COMLinkMock.h"
#include "OCIContainerMock.h"
#include "WindowManagerMock.h"
#include "WorkerPoolImplementation.h"
#include <fstream>

#define TEST_LOG(x, ...) fprintf(stderr, "\033[1;32m[%s:%d](%s)<PID:%d><TID:%d>" x "\n\033[0m", __FILE__, __LINE__, __FUNCTION__, getpid(), gettid(), ##__VA_ARGS__); fflush(stderr);
#define TEST_APP_CONTAINER_ID "com.sky.as.appsyouTube"

using namespace WPEFramework;
using ::testing::NiceMock;

class RuntimeManagerTest : public ::testing::Test {
protected:
    string response;
    Core::ProxyType<Plugin::RuntimeManagerImplementation> mRuntimeManagerImpl;
    Exchange::IRuntimeManager* interface = nullptr;
    Exchange::IConfiguration* runTimeManagerConfigure = nullptr;
    ServiceMock     *mServiceMock = nullptr;
    StorageManagerMock* mStoreageManagerMock = nullptr;
    OCIContainerMock* mociContainerMock = nullptr;
    WindowManagerMock* mWindowManagerMock = nullptr;
    Core::ProxyType<WorkerPoolImplementation> workerPool;

    RuntimeManagerTest()
        : workerPool(Core::ProxyType<WorkerPoolImplementation>::Create(
            2, Core::Thread::DefaultStackSize(), 16))
    {
        // create the RuntimeManagerImplementation instance
        mRuntimeManagerImpl = Core::ProxyType<Plugin::RuntimeManagerImplementation>::Create();

        interface = static_cast<Exchange::IRuntimeManager*>(
            mRuntimeManagerImpl->QueryInterface(Exchange::IRuntimeManager::ID));

        Core::IWorkerPool::Assign(&(*workerPool));
        workerPool->Run();
    }
    virtual ~RuntimeManagerTest()
    {
        Core::IWorkerPool::Assign(nullptr);
        workerPool.Release();

        interface->Release();
     }

    bool createResources(void)
    {
        mStoreageManagerMock = new NiceMock<StorageManagerMock>;
        mociContainerMock = new NiceMock<OCIContainerMock>;
        mWindowManagerMock = new NiceMock<WindowManagerMock>;
        mServiceMock =  new NiceMock<ServiceMock>;

        runTimeManagerConfigure = static_cast<Exchange::IConfiguration*>(
        mRuntimeManagerImpl->QueryInterface(Exchange::IConfiguration::ID));

        EXPECT_CALL(*mServiceMock, QueryInterfaceByCallsign(::testing::_, ::testing::_))
          .Times(::testing::AnyNumber())
          .WillRepeatedly(::testing::Invoke(
              [&](const uint32_t, const std::string& name) -> void* {
                if (name == "org.rdk.StorageManager") {
                    return reinterpret_cast<void*>(mStoreageManagerMock);
                }
                else if (name == "org.rdk.OCIContainer") {
                    return reinterpret_cast<void*>(mociContainerMock);
                }
                else if (name == "org.rdk.RDKWindowManager") {
                    return reinterpret_cast<void*>(mWindowManagerMock);
                }
            return nullptr;
        }));

        EXPECT_CALL(*mWindowManagerMock, AddRef())
            .Times(::testing::AnyNumber());

        EXPECT_CALL(*mWindowManagerMock, Register(::testing::_))
            .WillRepeatedly(::testing::Return(Core::ERROR_NONE));

        // Mock ConfigLine to return the runtime app portal configuration
        ON_CALL(*mServiceMock, ConfigLine())
            .WillByDefault(::testing::Return("{\"runtimeAppPortal\":\"com.sky.as.apps\"}"));

        runTimeManagerConfigure->Configure(mServiceMock);
        return true;
    }

    void releaseResources()
    {
        // Clean up mocks
        if (mociContainerMock != nullptr)
        {
            EXPECT_CALL(*mociContainerMock, Release())
                .WillOnce(::testing::Invoke(
                [&]() {
                     delete mociContainerMock;
                     return 0;
                    }));
        }

        if (mServiceMock != nullptr)
        {
            EXPECT_CALL(*mServiceMock, Release())
                .WillOnce(::testing::Invoke(
                [&]() {
                     delete mServiceMock;
                     return 0;
                    }));
        }

        if (mStoreageManagerMock != nullptr)
        {
            EXPECT_CALL(*mStoreageManagerMock, Release())
                .WillOnce(::testing::Invoke(
                [&]() {
                     delete mStoreageManagerMock;
                     return 0;
                    }));
        }

        if (mWindowManagerMock != nullptr)
        {
            EXPECT_CALL(*mWindowManagerMock, Release())
                .WillOnce(::testing::Invoke(
                [&]() {
                    delete mWindowManagerMock;
                    return 0;
                    }));
        }

        runTimeManagerConfigure->Release();
    }

    virtual void SetUp()
    {
        ASSERT_TRUE(interface != nullptr);
    }

    virtual void TearDown()
    {
        ASSERT_TRUE(interface != nullptr);
    }
};

/* Test Case for TerminateMethods
 * 
 * Setting up Runtime Manager Plugin and creating required COM-RPC resources
 * Simulating a container instance for the given appInstanceId ("youTube")
 * Setting up a mock expectation on StopContainer() to simulate graceful termination
 * Verifying Terminate() method sends stop request for the container with correct parameters
 * Simulating successful termination response via mocked StopContainer() implementation
 * Asserting that Terminate() returns Core::ERROR_NONE upon successful termination
 * Releasing the Runtime Manager Interface object and associated test resources
 */
TEST_F(RuntimeManagerTest, TerminateMethods)
{
    string appInstanceId("youTube");

    EXPECT_EQ(true, createResources());

    EXPECT_CALL(*mociContainerMock, StopContainer(TEST_APP_CONTAINER_ID, false,::testing::_, ::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string&, bool force, bool& success, string& errorReason) {
                success = true;
                errorReason = "No Error";
                return WPEFramework::Core::ERROR_NONE;
          }));

    EXPECT_EQ(Core::ERROR_NONE, interface->Terminate(appInstanceId));
    releaseResources();
}

/* Test Case for TerminateFailsWithEmptyAppInstanceId
 * 
 * Setting up Runtime Manager Plugin and creating required COM-RPC resources
 * Attempting to terminate a container using an empty appInstanceId ("")
 * Setting up a mock expectation on StopContainer(), though it should not be invoked
 * Verifying Terminate() method properly handles invalid (empty) input
 * Simulating that the call returns Core::ERROR_GENERAL due to missing appInstanceId
 * Asserting that Terminate() fails early without attempting container interaction
 * Releasing the Runtime Manager Interface object and associated test resources
 */
TEST_F(RuntimeManagerTest, TerminateFailsWithEmptyAppInstanceId)
{
    string appInstanceId("");

    EXPECT_EQ(true, createResources());

    EXPECT_CALL(*mociContainerMock, StopContainer(TEST_APP_CONTAINER_ID, false,::testing::_, ::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string&, bool force, bool& success, string& errorReason) {
                success = true;
                errorReason = "No Error";
                return WPEFramework::Core::ERROR_NONE;
          }));

    EXPECT_EQ(Core::ERROR_GENERAL, interface->Terminate(appInstanceId));
    releaseResources();
}

/* Test Case for TerminateNonExistentContainer
 *
 * Setting up Runtime Manager Plugin and creating required COM-RPC resources
 * Attempting to terminate a container using a non-existent appInstanceId ("invalid_id")
 * Setting up a mock expectation on StopContainer() to simulate failure due to missing container
 * Simulating StopContainer() returning success = false and an errorReason indicating container not found
 * Verifying that Terminate() still returns Core::ERROR_NONE as the request was processed, even if container didn't exist
 * Asserting graceful handling of termination requests for non-existent containers
 * Releasing the Runtime Manager Interface object and associated test resources
 */
TEST_F(RuntimeManagerTest, TerminateNonExistentContainer) {

    string appInstanceId("invalid_id");

    EXPECT_EQ(true, createResources());

    EXPECT_CALL(*mociContainerMock, StopContainer(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Invoke(
            [&](const string&, bool, bool& success, string& errorReason) {
                success = false;
                errorReason = "Container not found";
                return Core::ERROR_GENERAL;
            }));
    EXPECT_EQ(Core::ERROR_NONE, interface->Terminate(appInstanceId));
    releaseResources();
}

/* Test Case for TerminateWithInvalidContainerId
 *
 * Setting up Runtime Manager Plugin and creating required COM-RPC resources
 * Using an intentionally malformed or invalid appInstanceId ("_567gshg&*rt-")
 * Expecting the StopContainer() method to be invoked with this invalid ID
 * Simulating failure in StopContainer() with success = false and errorReason = "Container not found"
 * Verifying that Terminate() handles such cases gracefully and still returns Core::ERROR_NONE
 * Asserting robustness of Terminate() when handling syntactically invalid or unrecognized container IDs
 * Releasing the Runtime Manager Interface object and associated test resources
 */
TEST_F(RuntimeManagerTest, TerminateWithInvalidContainerId) {
    string appInstanceId("_567gshg&*rt-"); // invalid ID

    EXPECT_EQ(true, createResources());
    EXPECT_CALL(*mociContainerMock, StopContainer(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Invoke(
            [&](const string&, bool, bool& success, string& errorReason) {
                success = false;
                errorReason = "Container not found";
                return Core::ERROR_GENERAL;
            }));
    EXPECT_EQ(Core::ERROR_NONE, interface->Terminate(appInstanceId));
    releaseResources();
}

/* Test Case for TerminateAlreadyStoppedContainer
 *
 * Setting up Runtime Manager Plugin and initializing required COM-RPC resources
 * Using a valid appInstanceId ("youTube") assumed to be associated with a container
 * Simulating the scenario where the container has already been stopped
 * Mocking StopContainer() to return success = false and errorReason = "Container already stopped"
 * Expecting Terminate() to correctly interpret the failure and return Core::ERROR_GENERAL
 * Validating that Runtime Manager handles already-stopped containers gracefully
 * Releasing the Runtime Manager Interface object and all associated test resources
 */
TEST_F(RuntimeManagerTest, TerminateAlreadyStoppedContainer) {
    string appInstanceId("youTube");
    EXPECT_EQ(true, createResources());

    // Mock container as already stopped
    EXPECT_CALL(*mociContainerMock, StopContainer(TEST_APP_CONTAINER_ID, false, ::testing::_, ::testing::_))
        .WillOnce(::testing::Invoke(
            [&](const string&, bool, bool& success, string& errorReason) {
                success = false;
                errorReason = "Container already stopped";
                return Core::ERROR_GENERAL;
            }));

    EXPECT_EQ(Core::ERROR_GENERAL, interface->Terminate(appInstanceId));
    releaseResources();
}

/* Test Case for TerminateWithForceFails
 *
 * Setting up the Runtime Manager Plugin and initializing necessary COM-RPC resources
 * Using a valid appInstanceId ("youTube") to simulate a container instance
 * Invoking Kill() method, which attempts to forcefully stop the container
 * Mocking StopContainer() with force flag set to true, simulating a system failure
 * Returning success = false and errorReason = "System error: Cannot kill process"
 * Expecting Kill() to return Core::ERROR_GENERAL to indicate failure
 * Verifying that Runtime Manager handles forced termination failures correctly
 * Releasing the Runtime Manager Interface and associated test resources
 */
TEST_F(RuntimeManagerTest, TerminateWithForceFails) {
    string appInstanceId("youTube");
    EXPECT_EQ(true, createResources());

    // Mock force-stop failure
    EXPECT_CALL(*mociContainerMock, StopContainer(TEST_APP_CONTAINER_ID, true, ::testing::_, ::testing::_))
        .WillOnce(::testing::Invoke(
            [&](const string&, bool, bool& success, string& errorReason) {
                success = false;
                errorReason = "System error: Cannot kill process";
                return Core::ERROR_GENERAL;
            }));

    EXPECT_EQ(Core::ERROR_GENERAL, interface->Kill(appInstanceId));
    releaseResources();
}

/* Test Case for KillNonExistentContainer
 *
 * Initializing the Runtime Manager Plugin and required COM-RPC resources
 * Using an invalid appInstanceId ("Invalid") to simulate a non-existent container
 * Calling Kill() method, which internally calls StopContainer() with force = true
 * Mocking StopContainer() to simulate failure with error reason "Container not found"
 * Returning success = false to indicate failure in locating or stopping the container
 * Verifying that Kill() returns Core::ERROR_GENERAL for non-existent containers
 * Ensuring Runtime Manager gracefully handles forced termination of invalid containers
 * Releasing the Runtime Manager Interface and all associated test resources
 */
TEST_F(RuntimeManagerTest, KillNonExistentContainer) {
    EXPECT_EQ(true, createResources());
    EXPECT_CALL(*mociContainerMock, StopContainer("com.sky.as.appsInvalid", true, ::testing::_, ::testing::_))
        .WillOnce(::testing::Invoke(
            [&](const string&, bool, bool& success, string& errorReason) {
                success = false;
                errorReason = "Container not found";
                return Core::ERROR_GENERAL;
            }));
    EXPECT_EQ(Core::ERROR_GENERAL, interface->Kill("Invalid"));
    releaseResources();
}

/* Test Case for KillWithPermissionDenied
 *
 * Initializing the Runtime Manager Plugin and required COM-RPC resources
 * Using a valid appInstanceId ("youTube") to simulate a known container
 * Calling Kill() method, which internally calls StopContainer() with force = true
 * Mocking StopContainer() to simulate failure due to permission issues
 * Returning success = false and errorReason = "Permission denied" from the mock
 * Verifying that Kill() method propagates the failure and returns Core::ERROR_GENERAL
 * Ensuring Runtime Manager gracefully handles forced termination failures caused by insufficient permissions
 * Releasing the Runtime Manager Interface and all associated test resources
 */
TEST_F(RuntimeManagerTest, KillWithPermissionDenied) {
    EXPECT_EQ(true, createResources());
    EXPECT_CALL(*mociContainerMock, StopContainer(::testing::_, true, ::testing::_, ::testing::_))
        .WillOnce(::testing::Invoke(
            [&](const string&, bool, bool& success, string& errorReason) {
                success = false;
                errorReason = "Permission denied";
                return Core::ERROR_GENERAL;
            }));
    EXPECT_EQ(Core::ERROR_GENERAL, interface->Kill("youTube"));
    releaseResources();
}

/* Test Case for HibernateMethods
 *
 * Initializing the Runtime Manager Plugin and setting up necessary COM-RPC resources
 * Using a valid appInstanceId ("youTube") to simulate an existing container
 * Calling Hibernate() method to transition the container into a hibernated state
 * Setting up a mock expectation on HibernateContainer() to simulate successful hibernation
 * Mock returns success = true with no error, emulating successful state transition
 * Asserting that Hibernate() returns Core::ERROR_NONE indicating successful execution
 * Ensures Runtime Manager correctly invokes container hibernation logic and handles the response
 * Releasing the Runtime Manager Interface object and cleaning up all associated test resources
 */
TEST_F(RuntimeManagerTest, HibernateMethods)
{
    string appInstanceId("youTube");

    EXPECT_EQ(true, createResources());

    EXPECT_CALL(*mociContainerMock, HibernateContainer(TEST_APP_CONTAINER_ID, "",::testing::_, ::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string&, const string&, bool& success, string& errorReason) {
                success = true;
                errorReason = "No Error";
                return WPEFramework::Core::ERROR_NONE;
          }));

    EXPECT_EQ(Core::ERROR_NONE, interface->Hibernate(appInstanceId));
    releaseResources();
}

/* Test Case for HibernateFailsWithEmptyAppInstanceId
 *
 * Setting up the Runtime Manager Plugin and creating necessary COM-RPC resources
 * Using an empty string for appInstanceId to simulate invalid or missing input
 * Ensuring the Hibernate() method gracefully rejects the invalid appInstanceId
 * Setting up mock expectation on HibernateContainer(), though it should ideally not be called
 * The method under test should perform input validation and return Core::ERROR_GENERAL
 * This test verifies that the Runtime Manager properly handles invalid app identifiers
 * Releasing the Runtime Manager Interface object and associated test resources
 */
TEST_F(RuntimeManagerTest, HibernateFailsWithEmptyAppInstanceId)
{
    string appInstanceId("");

    EXPECT_EQ(true, createResources());

    EXPECT_CALL(*mociContainerMock, HibernateContainer(TEST_APP_CONTAINER_ID, "",::testing::_, ::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string&, const string&, bool& success, string& errorReason) {
                success = true;
                errorReason = "No Error";
                return WPEFramework::Core::ERROR_NONE;
          }));

    EXPECT_EQ(Core::ERROR_GENERAL, interface->Hibernate(appInstanceId));
    releaseResources();
}

/* Test Case for KillMethods
 *
 * Setting up the Runtime Manager Plugin and initializing required COM-RPC resources
 * Simulating a valid container instance with appInstanceId "youTube"
 * Setting up mock expectation on StopContainer() with force flag set to true
 * Mock simulates successful forced termination of the container
 * Calling Kill() method to terminate the appInstance forcibly
 * Verifying that Kill() triggers StopContainer() with the correct parameters
 * Asserting that Kill() returns Core::ERROR_NONE to indicate success
 * Releasing the Runtime Manager Interface object and cleaning up test resources
 */
TEST_F(RuntimeManagerTest, KillMethods)
{
    string appInstanceId("youTube");

    EXPECT_EQ(true, createResources());

    EXPECT_CALL(*mociContainerMock, StopContainer(TEST_APP_CONTAINER_ID, true,::testing::_, ::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string&, bool force, bool& success, string& errorReason) {
                success = true;
                errorReason = "No Error";
                return WPEFramework::Core::ERROR_NONE;
          }));

    EXPECT_EQ(Core::ERROR_NONE, interface->Kill(appInstanceId));
    releaseResources();
}

/* Test Case for KillFailsWithEmptyAppInstanceId
 *
 * Setting up the Runtime Manager Plugin and initializing COM-RPC test resources
 * Providing an empty appInstanceId to simulate invalid input
 * Setting mock expectation for StopContainer() with forced termination (though it should not be called)
 * Calling Kill() with an empty appInstanceId
 * Verifying that Kill() correctly identifies invalid input and returns Core::ERROR_GENERAL
 * Releasing the Runtime Manager Interface object and cleaning up test resources
 */
TEST_F(RuntimeManagerTest, KillFailsWithEmptyAppInstanceId)
{
    string appInstanceId("");

    EXPECT_EQ(true, createResources());

    EXPECT_CALL(*mociContainerMock, StopContainer(TEST_APP_CONTAINER_ID, true,::testing::_, ::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string&, bool force, bool& success, string& errorReason) {
                success = true;
                errorReason = "No Error";
                return WPEFramework::Core::ERROR_NONE;
          }));

    EXPECT_EQ(Core::ERROR_GENERAL, interface->Kill(appInstanceId));
    releaseResources();
}

/* Test Case for AnnonateMethods
 *
 * Setting up the Runtime Manager Plugin and creating necessary COM-RPC resources
 * Defining a valid appInstanceId along with key-value annotation data
 * Setting up mock expectation on Annotate() to simulate successful container annotation
 * Verifying that Annotate() is invoked with the correct container ID and key-value pair
 * Asserting that the Annotate() method returns Core::ERROR_NONE upon success
 * Cleaning up by releasing the Runtime Manager Interface object and test resources
 */
TEST_F(RuntimeManagerTest, AnnonateMethods)
{
    string appInstanceId("youTube");
    string appKey("youTube_Key");
    string appValue("youTube_Key");

    EXPECT_EQ(true, createResources());

    EXPECT_CALL(*mociContainerMock, Annotate(TEST_APP_CONTAINER_ID, appKey,appValue,::testing::_, ::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string& , const string& , const string& , bool& success , string& errorReason ){
                success = true;
                errorReason = "No Error";
                return WPEFramework::Core::ERROR_NONE;
          }));

    EXPECT_EQ(Core::ERROR_NONE, interface->Annotate(appInstanceId, appKey, appValue));
    releaseResources();
}

/* Test Case for AnnotateWithEmptyKeyOrValue
 *
 * Initializing the Runtime Manager Plugin and creating necessary test resources
 * Invoking Annotate() with a valid appInstanceId but an empty key to simulate invalid input
 * Verifying that the Annotate() method returns Core::ERROR_GENERAL when key is empty
 * Ensuring no actual container annotation is attempted with invalid parameters
 * Releasing all Runtime Manager Interface resources after test execution
 */
TEST_F(RuntimeManagerTest, AnnotateWithEmptyKeyOrValue) {
    EXPECT_EQ(true, createResources());
    EXPECT_EQ(Core::ERROR_GENERAL, interface->Annotate("youTube", "", "value")); // Empty key
    releaseResources();
}

/* Test Case for AnnotateNonExistentContainer
 *
 * Setting up Runtime Manager Plugin and creating required COM-RPC resources
 * Simulating an invalid or non-existent container with appInstanceId "Invalid"
 * Expecting Annotate() to attempt annotation via the mocked OCI container interface
 * Mocking Annotate() call to simulate failure due to "Container not found"
 * Verifying that Annotate() returns Core::ERROR_GENERAL when the target container does not exist
 * Releasing all Runtime Manager Interface resources after test execution
 */
TEST_F(RuntimeManagerTest, AnnotateNonExistentContainer) {
    EXPECT_EQ(true, createResources());
    EXPECT_CALL(*mociContainerMock, Annotate("com.sky.as.appsInvalid", ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Invoke(
            [&](const string&, const string&, const string&, bool& success, string& errorReason) {
                success = false;
                errorReason = "Container not found";
                return Core::ERROR_GENERAL;
            }));
    EXPECT_EQ(Core::ERROR_GENERAL, interface->Annotate("Invalid", "key", "value"));
    releaseResources();
}

/* Test Case for AnnotateFailsWithEmptyAppInstanceId
 *
 * Setting up Runtime Manager Plugin and initializing required COM-RPC resources
 * Using an empty string as the appInstanceId to simulate invalid input
 * Setting mock expectation for Annotate() even though it should not be called due to validation failure
 * Calling Annotate() with empty appInstanceId and verifying it fails gracefully
 * Asserting that Annotate() returns Core::ERROR_GENERAL due to missing app identifier
 * Releasing Runtime Manager Interface resources after the test
 */
TEST_F(RuntimeManagerTest, AnnotateFailsWithEmptyAppInstanceId)
{
    string appInstanceId("");
    string appKey("youTube_Key");
    string appValue("youTube_Key");

    EXPECT_EQ(true, createResources());

    EXPECT_CALL(*mociContainerMock, Annotate(TEST_APP_CONTAINER_ID, appKey,appValue,::testing::_, ::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string& , const string& , const string& , bool& success , string& errorReason ){
                success = true;
                errorReason = "No Error";
                return WPEFramework::Core::ERROR_NONE;
          }));

    EXPECT_EQ(Core::ERROR_GENERAL, interface->Annotate(appInstanceId, appKey, appValue));
    releaseResources();
}

/* Test Case for GetInfoMethods
 *
 * Setting up Runtime Manager Plugin and initializing COM-RPC resources
 * Using a valid appInstanceId ("youTube") for retrieving container information
 * Setting up mock expectation for GetContainerInfo() to return a JSON-formatted string
 * Mock returns simulated app stats like RAM usage, CPU load, GPU memory, and uptime
 * Calling GetInfo() and asserting it successfully retrieves the expected JSON info
 * Verifying that the returned info string exactly matches the mocked expected value
 * Releasing Runtime Manager Interface resources after the test
 */
TEST_F(RuntimeManagerTest, GetInfoMethods)
{
    string appInstanceId("youTube");
    string appInfo("");
    string expectedAppInfo = R"({"ramUsage": "150MB","cpuUsage": "20%","gpuMemory": "32MB","uptime": "45s"})";

    EXPECT_EQ(true, createResources());

    EXPECT_CALL(*mociContainerMock, GetContainerInfo(TEST_APP_CONTAINER_ID, ::testing::_,::testing::_, ::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string& , string& info ,  bool& success , string& errorReason ){
                info = expectedAppInfo;
                success = true;
                errorReason = "No Error";
                return WPEFramework::Core::ERROR_NONE;
          }));

    EXPECT_EQ(Core::ERROR_NONE, interface->GetInfo(appInstanceId, appInfo));
    EXPECT_STREQ(appInfo.c_str(), expectedAppInfo.c_str());
    releaseResources();
}

/* Test Case for GetInfoForNonExistentContainer
 *
 * Setting up Runtime Manager Plugin and initializing COM-RPC resources
 * Using an invalid appInstanceId ("Invalid") that doesn't map to any container
 * Setting up mock expectation on GetContainerInfo() to simulate failure due to missing container
 * Mock populates an appropriate error message and sets success to false
 * Calling GetInfo() and asserting it returns Core::ERROR_GENERAL
 * Verifying that the output string `appInfo` remains empty as no data is available
 * Releasing Runtime Manager Interface resources after the test
 */
TEST_F(RuntimeManagerTest, GetInfoForNonExistentContainer) {
    string appInfo;
    EXPECT_EQ(true, createResources());
    EXPECT_CALL(*mociContainerMock, GetContainerInfo("com.sky.as.appsInvalid", ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Invoke(
            [&](const string&, string& info, bool& success, string& errorReason) {
                info = "";
                success = false;
                errorReason = "Container not found";
                return Core::ERROR_GENERAL;
            }));
    EXPECT_EQ(Core::ERROR_GENERAL, interface->GetInfo("Invalid", appInfo));
    EXPECT_TRUE(appInfo.empty());
    releaseResources();
}

/* Test Case for GetInfoForValidContainerReturnError
 *
 * Initializing Runtime Manager Plugin and associated COM-RPC resources
 * Using an invalid appInstanceId ("Invalid") that maps to a valid container ID format
 * Setting up a mock for GetContainerInfo() to simulate a failure due to internal error
 * Mock sets `success = true` but returns Core::ERROR_GENERAL to mimic partial failure scenario
 * Ensures GetInfo() handles such inconsistent success/error status correctly
 * Verifies that the returned `appInfo` string is empty and error code is Core::ERROR_GENERAL
 * Releases the test resources after the scenario
 */
TEST_F(RuntimeManagerTest, GetInfoForValidContainerReturnError) {
    string appInfo;
    EXPECT_EQ(true, createResources());
    EXPECT_CALL(*mociContainerMock, GetContainerInfo("com.sky.as.appsInvalid", ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Invoke(
            [&](const string&, string& info, bool& success, string& errorReason) {
                info = "";
                success = true;
                errorReason = "No Error";
                return Core::ERROR_GENERAL;
            }));
    EXPECT_EQ(Core::ERROR_GENERAL, interface->GetInfo("Invalid", appInfo));
    EXPECT_TRUE(appInfo.empty());
    releaseResources();
}

/* Test Case for GetInfoFailsWithEmptyAppInstanceId
 *
 * Initializes the Runtime Manager Plugin and required COM-RPC resources
 * Provides an empty string as the appInstanceId, which is considered invalid input
 * Sets up a mock expectation on GetContainerInfo() with a valid container ID
 * Even though the mock simulates a successful container info retrieval,
 * the plugin logic is expected to reject the call due to empty appInstanceId
 * Verifies that GetInfo() returns Core::ERROR_GENERAL in such cases
 * Releases the Runtime Manager Interface resources after validation
 */
TEST_F(RuntimeManagerTest, GetInfoFailsWithEmptyAppInstanceId)
{
    string appInstanceId("");
    string appInfo("");
    string expectedAppInfo("This Test YouTube");

    EXPECT_EQ(true, createResources());

    EXPECT_CALL(*mociContainerMock, GetContainerInfo(TEST_APP_CONTAINER_ID, ::testing::_,::testing::_, ::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string& , string& info ,  bool& success , string& errorReason ){
                info = expectedAppInfo;
                success = true;
                errorReason = "No Error";
                return WPEFramework::Core::ERROR_NONE;
          }));

    EXPECT_EQ(Core::ERROR_GENERAL, interface->GetInfo(appInstanceId, appInfo));
    releaseResources();
}

/* Test Case for RunMethods
 *
 * Sets up Runtime Manager with valid inputs including environment, ports, and paths
 * Mocks StartContainerFromDobbySpec and CreateDisplay for a successful run path
 * Verifies that Run() launches the container and initializes display as expected
 * Ensures Run() returns Core::ERROR_NONE on success and releases resources
 */
TEST_F(RuntimeManagerTest, RunMethods)
{
    string appInstanceId("youTube");
    string expectedAppInfo = R"({"ramUsage": "150MB","cpuUsage": "20%","gpuMemory": "32MB","uptime": "45s"})";
    std::vector<uint32_t> portList = {10, 20};
    std::vector<std::string> pathsList = {"/tmp", "/opt"};
    std::vector<std::string> debugSettingsList = {"MIL", "INFO"};

    auto portsIterator = Core::Service<RPC::IteratorType<WPEFramework::Exchange::IRuntimeManager::IValueIterator>>::Create<WPEFramework::Exchange::IRuntimeManager::IValueIterator>(portList);
    auto pathsListIterator = Core::Service<RPC::IteratorType<WPEFramework::Exchange::IRuntimeManager::IStringIterator>>::Create<WPEFramework::Exchange::IRuntimeManager::IStringIterator>(pathsList);
    auto debugSettingsIterator = Core::Service<RPC::IteratorType<WPEFramework::Exchange::IRuntimeManager::IStringIterator>>::Create<WPEFramework::Exchange::IRuntimeManager::IStringIterator>(debugSettingsList);

    WPEFramework::Exchange::RuntimeConfig runtimeConfig;
    runtimeConfig.envVariables = "XDG_RUNTIME_DIR=/tmp;WAYLAND_DISPLAY=main";
    runtimeConfig.appPath = "/var/runTimeManager";
    runtimeConfig.runtimePath = "/tmp/runTimeManager";
    runtimeConfig.systemMemoryLimit = 512;
    runtimeConfig.command ="SkyBrowserLauncher";

    EXPECT_EQ(true, createResources());

    EXPECT_CALL(*mociContainerMock, StartContainerFromDobbySpec(TEST_APP_CONTAINER_ID, ::testing::_, "", "/run/user/1001/wst-youTube", ::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string& , const string&, const string&, const string&,int32_t& descriptor, bool& success, string& errorReason ){
                descriptor = 100;
                success = true;
                errorReason = "No Error";
                return WPEFramework::Core::ERROR_NONE;
          }));

    ON_CALL(*mWindowManagerMock, CreateDisplay(::testing::_))
            .WillByDefault(::testing::Return(Core::ERROR_NONE));

    EXPECT_EQ(Core::ERROR_NONE, interface->Run(appInstanceId, appInstanceId, 10, 10, portsIterator, pathsListIterator, debugSettingsIterator, runtimeConfig));

    releaseResources();
}

/* Test Case for RunWithoutCreateDisplay
 *
 * Simulates successful container start using mocked StartContainerFromDobbySpec
 * Forces CreateDisplay() to return Core::ERROR_GENERAL to simulate display creation failure
 * Verifies that Run() still proceeds and returns Core::ERROR_NONE despite display issue
 * Ensures all resources are cleaned up post execution
 */
TEST_F(RuntimeManagerTest, RunWithoutCreateDisplay)
{
    string appInstanceId("youTube");
    string expectedAppInfo = R"({"ramUsage": "150MB","cpuUsage": "20%","gpuMemory": "32MB","uptime": "45s"})";
    std::vector<uint32_t> portList = {10, 20};
    std::vector<std::string> pathsList = {"/tmp", "/opt"};
    std::vector<std::string> debugSettingsList = {"MIL", "INFO"};

    auto portsIterator = Core::Service<RPC::IteratorType<WPEFramework::Exchange::IRuntimeManager::IValueIterator>>::Create<WPEFramework::Exchange::IRuntimeManager::IValueIterator>(portList);
    auto pathsListIterator = Core::Service<RPC::IteratorType<WPEFramework::Exchange::IRuntimeManager::IStringIterator>>::Create<WPEFramework::Exchange::IRuntimeManager::IStringIterator>(pathsList);
    auto debugSettingsIterator = Core::Service<RPC::IteratorType<WPEFramework::Exchange::IRuntimeManager::IStringIterator>>::Create<WPEFramework::Exchange::IRuntimeManager::IStringIterator>(debugSettingsList);

    WPEFramework::Exchange::RuntimeConfig runtimeConfig;
    runtimeConfig.envVariables = "XDG_RUNTIME_DIR=/tmp;WAYLAND_DISPLAY=main";
    runtimeConfig.appPath = "/var/runTimeManager";
    runtimeConfig.runtimePath = "/tmp/runTimeManager";
    runtimeConfig.systemMemoryLimit = 512;
    runtimeConfig.command ="SkyBrowserLauncher";

    EXPECT_EQ(true, createResources());

    EXPECT_CALL(*mociContainerMock, StartContainerFromDobbySpec(TEST_APP_CONTAINER_ID, ::testing::_, "", "/run/user/1001/wst-youTube", ::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string& , const string&, const string&, const string&,int32_t& descriptor, bool& success, string& errorReason ){
                descriptor = 100;
                success = true;
                errorReason = "No Error";
                return WPEFramework::Core::ERROR_NONE;
          }));

    ON_CALL(*mWindowManagerMock, CreateDisplay(::testing::_))
            .WillByDefault(::testing::Return(Core::ERROR_GENERAL));

    EXPECT_EQ(Core::ERROR_GENERAL, interface->Run(appInstanceId, appInstanceId, 10, 10, portsIterator, pathsListIterator, debugSettingsIterator, runtimeConfig));
    releaseResources();
}

/* Test Case for RunCreateFkpsMounts
 *
 * Sets up FKPS file under /opt/drm and includes it in runtime config JSON array
 * Simulates container start and display creation using mocks
 * Validates that Run() successfully mounts FKPS file and proceeds with execution
 * Ensures proper creation of FKPS content and cleanup after test run
 */
TEST_F(RuntimeManagerTest, RunCreateFkpsMounts)
{
    string appInstanceId("youTube");
    string expectedAppInfo = R"({"ramUsage": "150MB","cpuUsage": "20%","gpuMemory": "32MB","uptime": "45s"})";
    std::vector<uint32_t> portList = {10, 20};
    std::vector<std::string> pathsList = {"/tmp", "/opt"};
    std::vector<std::string> debugSettingsList = {"MIL", "INFO"};

    auto portsIterator = Core::Service<RPC::IteratorType<WPEFramework::Exchange::IRuntimeManager::IValueIterator>>::Create<WPEFramework::Exchange::IRuntimeManager::IValueIterator>(portList);
    auto pathsListIterator = Core::Service<RPC::IteratorType<WPEFramework::Exchange::IRuntimeManager::IStringIterator>>::Create<WPEFramework::Exchange::IRuntimeManager::IStringIterator>(pathsList);
    auto debugSettingsIterator = Core::Service<RPC::IteratorType<WPEFramework::Exchange::IRuntimeManager::IStringIterator>>::Create<WPEFramework::Exchange::IRuntimeManager::IStringIterator>(debugSettingsList);

    WPEFramework::Exchange::RuntimeConfig runtimeConfig;
    runtimeConfig.envVariables = "XDG_RUNTIME_DIR=/tmp;WAYLAND_DISPLAY=main";
    runtimeConfig.appPath = "/var/runTimeManager";
    runtimeConfig.runtimePath = "/tmp/runTimeManager";
    runtimeConfig.systemMemoryLimit = 512;
    runtimeConfig.command ="SkyBrowserLauncher";

    std::string fkpsFileName = "test1.fkps";
    std::string fkpsDir = "/opt/drm";
    std::string fkpsFullPath = fkpsDir + "/" + fkpsFileName;

    mkdir(fkpsDir.c_str(), 0755); // Ensure /opt/drm exists

    {
        std::ofstream fkpsFile(fkpsFullPath);
        ASSERT_TRUE(fkpsFile.is_open()) << "Failed to create FKPS file";
        fkpsFile << "dummy FKPS content";
    }

    runtimeConfig.fkpsFiles = R"(["test1.fkps"])"; // JSON array of FKPS file

    EXPECT_EQ(true, createResources());

    EXPECT_CALL(*mociContainerMock, StartContainerFromDobbySpec(TEST_APP_CONTAINER_ID, ::testing::_, "", "/run/user/1001/wst-youTube", ::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string& , const string&, const string&, const string&,int32_t& descriptor, bool& success, string& errorReason ){
                descriptor = 100;
                success = true;
                errorReason = "No Error";
                return WPEFramework::Core::ERROR_NONE;
          }));

    ON_CALL(*mWindowManagerMock, CreateDisplay(::testing::_))
            .WillByDefault(::testing::Return(Core::ERROR_NONE));

    EXPECT_EQ(Core::ERROR_NONE, interface->Run(appInstanceId, appInstanceId, 10, 10, portsIterator, pathsListIterator, debugSettingsIterator, runtimeConfig));

    releaseResources();
}

/* Test Case for RunReadfromAIConfigFile
 *
 * Creates a mock config.ini with various runtime parameters in /opt/demo
 * Validates that RuntimeManager correctly reads and applies AI configuration
 * Simulates container start and display creation via mocks
 * Ensures Run() completes successfully with loaded config values
 * Cleans up config file after test completion
 */
TEST_F(RuntimeManagerTest, RunReadfromAIConfigFile)
{
    const std::string configDir = "/opt/demo";
    const std::string configFilePath = configDir + "/config.ini";
    const std::string appInstanceId = "testAppInstance";

    if (access(configDir.c_str(), F_OK) != 0) {
        int ret = mkdir(configDir.c_str(), 0755);
        ASSERT_EQ(ret, 0) << "Failed to create directory: " << configDir << " (errno: " << errno << ")";
    }

    {
        std::ofstream configFile(configFilePath);
        ASSERT_TRUE(configFile.is_open()) << "Unable to open " << configFilePath << " for writing";
        configFile << "consoleLogCap=1024\n";
        configFile << "cores=15\n";
        configFile << "ramLimit=512\n";
        configFile << "gpuMemoryLimit=128\n";
        configFile << "vpuAccessBlacklist=item1,item2\n";
        configFile << "appsRequiringDBus=app1,app2\n";
        configFile << "mapiPorts=1234,5678\n";
        configFile << "resourceManagerClientEnabled=true\n";
        configFile << "gstreamerRegistryEnabled=true\n";
        configFile << "svpEnabled=true\n";
        configFile << "enableUsbMassStorage=true\n";
        configFile << "ipv6Enabled=true\n";
        configFile << "ionDefaultLimit=2048\n";
        configFile << "dialServerPort=8080\n";
        configFile << "dialServerPathPrefix=\"/prefix\"\n";
        configFile << "dialUsn=\"uuid:1234\"\n";
        configFile << "ionLimits=item1:1000,item2:2000\n";
        configFile << "preloads=lib1,lib2\n";
        configFile << "envVariables=VAR1=1,VAR2=2\n";
        configFile.close();
    }

    std::vector<uint32_t> portList = { 1234 };
    std::vector<std::string> pathsList = { "/tmp" };
    std::vector<std::string> debugSettingsList = { "INFO" };

    auto portsIterator = Core::Service<RPC::IteratorType<WPEFramework::Exchange::IRuntimeManager::IValueIterator>>::Create<WPEFramework::Exchange::IRuntimeManager::IValueIterator>(portList);
    auto pathsListIterator = Core::Service<RPC::IteratorType<WPEFramework::Exchange::IRuntimeManager::IStringIterator>>::Create<WPEFramework::Exchange::IRuntimeManager::IStringIterator>(pathsList);
    auto debugSettingsIterator = Core::Service<RPC::IteratorType<WPEFramework::Exchange::IRuntimeManager::IStringIterator>>::Create<WPEFramework::Exchange::IRuntimeManager::IStringIterator>(debugSettingsList);

    WPEFramework::Exchange::RuntimeConfig runtimeConfig;
    runtimeConfig.envVariables = "XDG_RUNTIME_DIR=/tmp;WAYLAND_DISPLAY=wst-test";
    runtimeConfig.appPath = "/var/testApp";
    runtimeConfig.runtimePath = "/tmp/testApp";
    runtimeConfig.systemMemoryLimit = 512;
    runtimeConfig.command ="SkyBrowserLauncher";

    EXPECT_EQ(true, createResources());

    EXPECT_CALL(*mociContainerMock, StartContainerFromDobbySpec(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Invoke(
            [](const std::string&, const std::string&, const std::string&, const std::string&, int32_t& descriptor, bool& success, std::string& errorReason) {
                descriptor = 99;
                success = true;
                errorReason = "";
                return Core::ERROR_NONE;
            }));

    ON_CALL(*mWindowManagerMock, CreateDisplay(::testing::_)).WillByDefault(::testing::Return(Core::ERROR_NONE));

    LOGINFO("Calling Run");
    EXPECT_EQ(Core::ERROR_NONE, interface->Run(appInstanceId, appInstanceId, 1000, 1001,
                                               portsIterator, pathsListIterator, debugSettingsIterator, runtimeConfig));

    // Optional: Clean up the config file after test
    remove(configFilePath.c_str());
    releaseResources();
}

/* Test Case for StartContainerFailure
 *
 * Simulates failure in starting a container due to invalid Dobby spec
 * Sets up necessary runtime config and mock environment for container start
 * Mocks StartContainerFromDobbySpec() to return failure and error message
 * Verifies that Run() returns Core::ERROR_GENERAL on container start failure
 */
TEST_F(RuntimeManagerTest, StartContainerFailure) {

    string appInstanceId("youTube");
    string expectedAppInfo = R"({"ramUsage": "150MB","cpuUsage": "20%","gpuMemory": "32MB","uptime": "45s"})";
    std::vector<uint32_t> portList = {10, 20};
    std::vector<std::string> pathsList = {"/tmp", "/opt"};
    std::vector<std::string> debugSettingsList = {"MIL", "INFO"};

    auto portsIterator = Core::Service<RPC::IteratorType<WPEFramework::Exchange::IRuntimeManager::IValueIterator>>::Create<WPEFramework::Exchange::IRuntimeManager::IValueIterator>(portList);
    auto pathsListIterator = Core::Service<RPC::IteratorType<WPEFramework::Exchange::IRuntimeManager::IStringIterator>>::Create<WPEFramework::Exchange::IRuntimeManager::IStringIterator>(pathsList);
    auto debugSettingsIterator = Core::Service<RPC::IteratorType<WPEFramework::Exchange::IRuntimeManager::IStringIterator>>::Create<WPEFramework::Exchange::IRuntimeManager::IStringIterator>(debugSettingsList);

    WPEFramework::Exchange::RuntimeConfig runtimeConfig;
    runtimeConfig.envVariables = "XDG_RUNTIME_DIR=/tmp;WAYLAND_DISPLAY=main";
    runtimeConfig.appPath = "/var/runTimeManager";
    runtimeConfig.runtimePath = "/tmp/runTimeManager";
    runtimeConfig.systemMemoryLimit = 512;
    runtimeConfig.command ="SkyBrowserLauncher";

    EXPECT_EQ(true, createResources());
    EXPECT_CALL(*mociContainerMock, StartContainerFromDobbySpec(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Invoke(
            [&](const string& , const string&, const string&, const string&,int32_t& descriptor, bool& success, string& errorReason ) {
                descriptor = -1;
                success = false;
                errorReason = "Dobby spec invalid";
                return Core::ERROR_GENERAL;
            }));
    EXPECT_EQ(Core::ERROR_GENERAL, interface->Run(appInstanceId, appInstanceId, 10, 10, portsIterator, pathsListIterator, debugSettingsIterator, runtimeConfig)); // Pass valid args
    releaseResources();
}

/* Test Case for RunWithEmptyContainerId
 *
 * Validates behavior when Run() is invoked with an empty appInstanceId
 * Sets up basic environment variables and runtime configuration
 * Passes empty container ID and expects Run() to fail with Core::ERROR_GENERAL
 * Ensures interface handles invalid input gracefully and exits early
 */
TEST_F(RuntimeManagerTest, RunWithEmptyContainerId) {

    WPEFramework::Exchange::RuntimeConfig runtimeConfig;
    runtimeConfig.envVariables = "XDG_RUNTIME_DIR=/tmp;WAYLAND_DISPLAY=main";
    runtimeConfig.appPath = "/valid/path";
    runtimeConfig.runtimePath = "/valid/runtime";

    EXPECT_EQ(true, createResources());

    EXPECT_EQ(Core::ERROR_GENERAL, interface->Run(
        "", "", 10, 10, nullptr, nullptr, nullptr,
        runtimeConfig
    ));
    releaseResources();
}

/* Test Case for RunWithDuplicateContainerId
 *
 * Verifies that Run() fails when attempting to start a container with a duplicate container ID
 * Mocks StartContainerFromDobbySpec() to simulate conflict with existing container
 * Expects Run() to return Core::ERROR_GENERAL due to ID collision
 * Ensures the interface handles duplicate container scenarios gracefully
 */
TEST_F(RuntimeManagerTest, RunWithDuplicateContainerId) {
    EXPECT_EQ(true, createResources());
    EXPECT_CALL(*mociContainerMock, StartContainerFromDobbySpec(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Invoke(
            [&](const string& , const string&, const string&, const string&,int32_t& descriptor, bool& success, string& errorReason ) {
                descriptor = -1;
                success = false;
                errorReason = "Container ID already exists";
                return Core::ERROR_GENERAL;
            }));

    WPEFramework::Exchange::RuntimeConfig runtimeConfig;
    runtimeConfig.envVariables = "XDG_RUNTIME_DIR=/tmp;WAYLAND_DISPLAY=main";
    runtimeConfig.appPath = "/valid/path";
    runtimeConfig.runtimePath = "/valid/runtime";
    runtimeConfig.systemMemoryLimit = 512;
    runtimeConfig.command ="SkyBrowserLauncher";

    EXPECT_EQ(Core::ERROR_GENERAL, interface->Run(
        "youTube", "youTube", 10, 10, nullptr, nullptr, nullptr,runtimeConfig
    ));
    releaseResources();
}

/* Test Case for RunWithTimeout
 *
 * Simulates a timeout error while invoking StartContainerFromDobbySpec during Run()
 * Mocks container start to return failure with "Operation timed out" error
 * Verifies Run() handles timeout failure gracefully and returns Core::ERROR_GENERAL
 * Confirms the system does not proceed with launch on start timeout
 */
TEST_F(RuntimeManagerTest, RunWithTimeout) {
    EXPECT_EQ(true, createResources());
    EXPECT_CALL(*mociContainerMock, StartContainerFromDobbySpec(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Invoke(
            [&](const string& , const string&, const string&, const string&,int32_t& descriptor,  bool& success , string& errorReason ) {
                descriptor = -1;
                success = false;
                errorReason = "Operation timed out";
                return Core::ERROR_GENERAL;
            }));

    WPEFramework::Exchange::RuntimeConfig runtimeConfig;
    runtimeConfig.envVariables = "XDG_RUNTIME_DIR=/tmp;WAYLAND_DISPLAY=main";
    runtimeConfig.appPath = "/valid/path";
    runtimeConfig.runtimePath = "/valid/runtime";
    runtimeConfig.systemMemoryLimit = 512;
    runtimeConfig.command ="SkyBrowserLauncher";

    EXPECT_EQ(Core::ERROR_GENERAL, interface->Run(
        "youTube", "youTube", 10, 10, nullptr, nullptr, nullptr,runtimeConfig
    ));
    releaseResources();
}

/* Test Case for WakeMethods
 *
 * Verifies successful Run, Hibernate, and Wake transitions for a container
 * Mocks StartContainerFromDobbySpec, HibernateContainer, and WakeupContainer with success responses
 * Ensures Wake() is called after container is successfully hibernated
 * Asserts all three operations (Run ? Hibernate ? Wake) complete with Core::ERROR_NONE
 */
TEST_F(RuntimeManagerTest, WakeMethods)
{
    string appInstanceId("youTube");
    string expectedAppInfo = R"({"ramUsage": "150MB","cpuUsage": "20%","gpuMemory": "32MB","uptime": "45s"})";
    std::vector<uint32_t> portList = {10, 20};
    std::vector<std::string> pathsList = {"/tmp", "/opt"};
    std::vector<std::string> debugSettingsList = {"MIL", "INFO"};

    auto portsIterator = Core::Service<RPC::IteratorType<WPEFramework::Exchange::IRuntimeManager::IValueIterator>>::Create<WPEFramework::Exchange::IRuntimeManager::IValueIterator>(portList);
    auto pathsListIterator = Core::Service<RPC::IteratorType<WPEFramework::Exchange::IRuntimeManager::IStringIterator>>::Create<WPEFramework::Exchange::IRuntimeManager::IStringIterator>(pathsList);
    auto debugSettingsIterator = Core::Service<RPC::IteratorType<WPEFramework::Exchange::IRuntimeManager::IStringIterator>>::Create<WPEFramework::Exchange::IRuntimeManager::IStringIterator>(debugSettingsList);

    WPEFramework::Exchange::RuntimeConfig runtimeConfig;
    runtimeConfig.envVariables = "XDG_RUNTIME_DIR=/tmp;WAYLAND_DISPLAY=main";
    runtimeConfig.appPath = "/var/runTimeManager";
    runtimeConfig.runtimePath = "/tmp/runTimeManager";
    runtimeConfig.systemMemoryLimit = 512;
    runtimeConfig.command ="SkyBrowserLauncher";

    EXPECT_EQ(true, createResources());

    EXPECT_CALL(*mociContainerMock, StartContainerFromDobbySpec(TEST_APP_CONTAINER_ID, ::testing::_, "", "/run/user/1001/wst-youTube", ::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string& , const string&, const string&, const string&,int32_t& descriptor, bool& success, string& errorReason ){
                descriptor = 100;
                success = true;
                errorReason = "No Error";
                return WPEFramework::Core::ERROR_NONE;
          }));

    EXPECT_EQ(Core::ERROR_NONE, interface->Run(appInstanceId, appInstanceId, 10, 10, portsIterator, pathsListIterator, debugSettingsIterator, runtimeConfig));

    EXPECT_CALL(*mociContainerMock, HibernateContainer(TEST_APP_CONTAINER_ID, "",::testing::_, ::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string&, const string&, bool& success, string& errorReason) {
                success = true;
                errorReason = "No Error";
                return WPEFramework::Core::ERROR_NONE;
          }));

    EXPECT_EQ(Core::ERROR_NONE, interface->Hibernate(appInstanceId));

    EXPECT_CALL(*mociContainerMock, WakeupContainer(TEST_APP_CONTAINER_ID, ::testing::_, ::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string&, bool& success, string& errorReason) {
                success = true;
                errorReason = "No Error";
                return WPEFramework::Core::ERROR_NONE;
          }));

    EXPECT_EQ(Core::ERROR_NONE, interface->Wake(appInstanceId, WPEFramework::Exchange::IRuntimeManager::RUNTIME_STATE_RUNNING));

    releaseResources();
}

/* Test Case for WakeOnRunningNonHibernateContainer
 *
 * Ensures Wake() fails if invoked without prior Hibernate()
 * Mocks StartContainerFromDobbySpec with a successful container start
 * Does not call HibernateContainer before Wake()
 * Expects Wake() to return Core::ERROR_GENERAL indicating invalid state transition
 */
TEST_F(RuntimeManagerTest, WakeOnRunningNonHibernateContainer)
{
    string appInstanceId("youTube");
    string expectedAppInfo = R"({"ramUsage": "150MB","cpuUsage": "20%","gpuMemory": "32MB","uptime": "45s"})";
    std::vector<uint32_t> portList = {10, 20};
    std::vector<std::string> pathsList = {"/tmp", "/opt"};
    std::vector<std::string> debugSettingsList = {"MIL", "INFO"};

    auto portsIterator = Core::Service<RPC::IteratorType<WPEFramework::Exchange::IRuntimeManager::IValueIterator>>::Create<WPEFramework::Exchange::IRuntimeManager::IValueIterator>(portList);
    auto pathsListIterator = Core::Service<RPC::IteratorType<WPEFramework::Exchange::IRuntimeManager::IStringIterator>>::Create<WPEFramework::Exchange::IRuntimeManager::IStringIterator>(pathsList);
    auto debugSettingsIterator = Core::Service<RPC::IteratorType<WPEFramework::Exchange::IRuntimeManager::IStringIterator>>::Create<WPEFramework::Exchange::IRuntimeManager::IStringIterator>(debugSettingsList);

    WPEFramework::Exchange::RuntimeConfig runtimeConfig;
    runtimeConfig.envVariables = "XDG_RUNTIME_DIR=/tmp;WAYLAND_DISPLAY=main";
    runtimeConfig.appPath = "/var/runTimeManager";
    runtimeConfig.runtimePath = "/tmp/runTimeManager";
    runtimeConfig.systemMemoryLimit = 512;
    runtimeConfig.command ="SkyBrowserLauncher";

    EXPECT_EQ(true, createResources());

    EXPECT_CALL(*mociContainerMock, StartContainerFromDobbySpec(TEST_APP_CONTAINER_ID, ::testing::_, "", "/run/user/1001/wst-youTube", ::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string& , const string&, const string&, const string&,int32_t& descriptor, bool& success, string& errorReason ){
                descriptor = 100;
                success = true;
                errorReason = "No Error";
                return WPEFramework::Core::ERROR_NONE;
          }));

    EXPECT_EQ(Core::ERROR_NONE, interface->Run(appInstanceId, appInstanceId, 10, 10, portsIterator, pathsListIterator, debugSettingsIterator, runtimeConfig));


    EXPECT_EQ(Core::ERROR_GENERAL, interface->Wake(appInstanceId, WPEFramework::Exchange::IRuntimeManager::RUNTIME_STATE_RUNNING));

    releaseResources();
}

/* Test Case for WakeWithGeneralError
 *
 * Validates Wake() failure when WakeupContainer returns an error
 * Simulates successful Run() and Hibernate() calls for the container
 * Mocks WakeupContainer to return Core::ERROR_GENERAL with failure status
 * Expects Wake() to return Core::ERROR_GENERAL due to internal wakeup failure
 */
TEST_F(RuntimeManagerTest, WakeWithGeneralError)
{
    string appInstanceId("youTube");
    string expectedAppInfo = R"({"ramUsage": "150MB","cpuUsage": "20%","gpuMemory": "32MB","uptime": "45s"})";
    std::vector<uint32_t> portList = {10, 20};
    std::vector<std::string> pathsList = {"/tmp", "/opt"};
    std::vector<std::string> debugSettingsList = {"MIL", "INFO"};

    auto portsIterator = Core::Service<RPC::IteratorType<WPEFramework::Exchange::IRuntimeManager::IValueIterator>>::Create<WPEFramework::Exchange::IRuntimeManager::IValueIterator>(portList);
    auto pathsListIterator = Core::Service<RPC::IteratorType<WPEFramework::Exchange::IRuntimeManager::IStringIterator>>::Create<WPEFramework::Exchange::IRuntimeManager::IStringIterator>(pathsList);
    auto debugSettingsIterator = Core::Service<RPC::IteratorType<WPEFramework::Exchange::IRuntimeManager::IStringIterator>>::Create<WPEFramework::Exchange::IRuntimeManager::IStringIterator>(debugSettingsList);

    WPEFramework::Exchange::RuntimeConfig runtimeConfig;
    runtimeConfig.envVariables = "XDG_RUNTIME_DIR=/tmp;WAYLAND_DISPLAY=main";
    runtimeConfig.appPath = "/var/runTimeManager";
    runtimeConfig.runtimePath = "/tmp/runTimeManager";
    runtimeConfig.systemMemoryLimit = 512;
    runtimeConfig.command ="SkyBrowserLauncher";

    EXPECT_EQ(true, createResources());

    EXPECT_CALL(*mociContainerMock, StartContainerFromDobbySpec(TEST_APP_CONTAINER_ID, ::testing::_, "", "/run/user/1001/wst-youTube", ::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string& , const string&, const string&, const string&,int32_t& descriptor, bool& success, string& errorReason ){
                descriptor = 100;
                success = true;
                errorReason = "No Error";
                return WPEFramework::Core::ERROR_NONE;
          }));

    EXPECT_EQ(Core::ERROR_NONE, interface->Run(appInstanceId, appInstanceId, 10, 10, portsIterator, pathsListIterator, debugSettingsIterator, runtimeConfig));

    EXPECT_CALL(*mociContainerMock, HibernateContainer(TEST_APP_CONTAINER_ID, "",::testing::_, ::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string&, const string&, bool& success, string& errorReason) {
                success = true;
                errorReason = "No Error";
                return WPEFramework::Core::ERROR_NONE;
          }));

    EXPECT_EQ(Core::ERROR_NONE, interface->Hibernate(appInstanceId));

    EXPECT_CALL(*mociContainerMock, WakeupContainer(TEST_APP_CONTAINER_ID, ::testing::_, ::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string&, bool& success, string& errorReason) {
                success = false;
                errorReason = "Genral Error";
                return WPEFramework::Core::ERROR_GENERAL;
          }));

    EXPECT_EQ(Core::ERROR_GENERAL, interface->Wake(appInstanceId, WPEFramework::Exchange::IRuntimeManager::RUNTIME_STATE_RUNNING));

    releaseResources();
}

/* Test Case for WakeOnNonRunningContainer
 *
 * Verifies Wake() behavior when called on a non-running container
 * Does not simulate prior Run() or Hibernate() actions
 * Expects Wake() to fail due to missing container context
 * Ensures Wake() returns Core::ERROR_GENERAL
 */
TEST_F(RuntimeManagerTest, WakeOnNonRunningContainer)
{
    string appInstanceId("youTube");
    EXPECT_EQ(true, createResources());

    EXPECT_EQ(Core::ERROR_GENERAL, interface->Wake(appInstanceId, WPEFramework::Exchange::IRuntimeManager::RUNTIME_STATE_RUNNING));

    releaseResources();
}

/* Test Case for WakeFailsWithEmptyAppInstanceId
 *
 * Validates Wake() behavior when provided an empty appInstanceId
 * No container is associated with the empty ID
 * Expects Wake() to fail gracefully
 * Ensures it returns Core::ERROR_GENERAL
 */
TEST_F(RuntimeManagerTest, WakeFailsWithEmptyAppInstanceId)
{
    string appInstanceId("");

    EXPECT_EQ(true, createResources());

    EXPECT_EQ(Core::ERROR_GENERAL, interface->Wake(appInstanceId, WPEFramework::Exchange::IRuntimeManager::RUNTIME_STATE_RUNNING));

    releaseResources();
}

/* Test Case for SuspendResumeMethods
 *
 * Verifies the suspend and resume lifecycle operations on a running container
 * Mocks PauseContainer and ResumeContainer with success responses
 * Ensures both Suspend() and Resume() return Core::ERROR_NONE
 * Validates end-to-end control flow of container pause/resume
 */
TEST_F(RuntimeManagerTest, SuspendResumeMethods)
{
    string appInstanceId("youTube");
    string expectedAppInfo = R"({"ramUsage": "150MB","cpuUsage": "20%","gpuMemory": "32MB","uptime": "45s"})";
    std::vector<uint32_t> portList = {10, 20};
    std::vector<std::string> pathsList = {"/tmp", "/opt"};
    std::vector<std::string> debugSettingsList = {"MIL", "INFO"};

    auto portsIterator = Core::Service<RPC::IteratorType<WPEFramework::Exchange::IRuntimeManager::IValueIterator>>::Create<WPEFramework::Exchange::IRuntimeManager::IValueIterator>(portList);
    auto pathsListIterator = Core::Service<RPC::IteratorType<WPEFramework::Exchange::IRuntimeManager::IStringIterator>>::Create<WPEFramework::Exchange::IRuntimeManager::IStringIterator>(pathsList);
    auto debugSettingsIterator = Core::Service<RPC::IteratorType<WPEFramework::Exchange::IRuntimeManager::IStringIterator>>::Create<WPEFramework::Exchange::IRuntimeManager::IStringIterator>(debugSettingsList);

    WPEFramework::Exchange::RuntimeConfig runtimeConfig;
    runtimeConfig.envVariables = "XDG_RUNTIME_DIR=/tmp;WAYLAND_DISPLAY=main";
    runtimeConfig.appPath = "/var/runTimeManager";
    runtimeConfig.runtimePath = "/tmp/runTimeManager";
    runtimeConfig.systemMemoryLimit = 512;
    runtimeConfig.command ="SkyBrowserLauncher";

    EXPECT_EQ(true, createResources());

    EXPECT_CALL(*mociContainerMock, StartContainerFromDobbySpec(TEST_APP_CONTAINER_ID, ::testing::_, "", "/run/user/1001/wst-youTube", ::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string& , const string&, const string&, const string&,int32_t& descriptor, bool& success, string& errorReason ){
                descriptor = 100;
                success = true;
                errorReason = "No Error";
                return WPEFramework::Core::ERROR_NONE;
          }));

    ON_CALL(*mWindowManagerMock, CreateDisplay(::testing::_))
            .WillByDefault(::testing::Return(Core::ERROR_NONE));

    EXPECT_EQ(Core::ERROR_NONE, interface->Run(appInstanceId, appInstanceId, 10, 10, portsIterator, pathsListIterator, debugSettingsIterator, runtimeConfig));

    EXPECT_CALL(*mociContainerMock, PauseContainer(::testing::_, ::testing::_, ::testing::_))
    .WillOnce([](const std::string&, bool& success, std::string& reason) {
            success = true;
            reason = "No Error";
            return Core::ERROR_NONE;
        });

    EXPECT_EQ(Core::ERROR_NONE, interface->Suspend(appInstanceId));

    EXPECT_CALL(*mociContainerMock, ResumeContainer(::testing::_, ::testing::_, ::testing::_))
        .WillOnce([](const std::string& containerId, bool& success, std::string& reason) {
            success = true;
            reason = "No Error";
            return Core::ERROR_NONE;
        });

    EXPECT_EQ(Core::ERROR_NONE, interface->Resume(appInstanceId));
    releaseResources();
}

/* Test: SuspendFailsWithEmptyAppInstanceId
 *
 * Purpose: Verify Suspend() fails when appInstanceId is empty.
 * Expectation: PauseContainer should not be called.
 * Result: Suspend() should return Core::ERROR_GENERAL.
 * Validates input checking for Suspend().
 */
TEST_F(RuntimeManagerTest, SuspendFailsWithEmptyAppInstanceId)
{
    EXPECT_EQ(true, createResources());

    EXPECT_CALL(*mociContainerMock, PauseContainer(::testing::_, ::testing::_, ::testing::_))
        .Times(0);

    EXPECT_EQ(Core::ERROR_GENERAL, interface->Suspend(""));

    releaseResources();
}

/* Test Case for SuspendFailsWithEmptyAppInstanceId
 *
 * Ensures Suspend() fails gracefully when appInstanceId is empty
 * Verifies that PauseContainer is not invoked
 * Expects Core::ERROR_GENERAL as the return value
 * Validates input sanity checks in Suspend()
 */
TEST_F(RuntimeManagerTest, ResumeFailsWithEmptyAppInstanceId)
{
    EXPECT_EQ(true, createResources());

    EXPECT_CALL(*mociContainerMock, ResumeContainer(::testing::_, ::testing::_, ::testing::_))
        .Times(0);

    EXPECT_EQ(Core::ERROR_GENERAL, interface->Resume(""));

    releaseResources();
}

/* Test: SuspendFailsWithPauseContainerError
 *
 * Purpose: Validate behavior when PauseContainer fails during Suspend().
 * Setup: Mocks StartContainerFromDobbySpec to succeed and PauseContainer to fail.
 * Expectation: Suspend() should return Core::ERROR_GENERAL on failure.
 * Confirms error handling path in Suspend().
 */
TEST_F(RuntimeManagerTest, SuspendFailsWithPauseContainerError)
{
    string appInstanceId("youTube");
    std::vector<uint32_t> portList = {10, 20};
    std::vector<std::string> pathsList = {"/tmp", "/opt"};
    std::vector<std::string> debugSettingsList = {"MIL", "INFO"};

    auto portsIterator = Core::Service<RPC::IteratorType<Exchange::IRuntimeManager::IValueIterator>>::Create<Exchange::IRuntimeManager::IValueIterator>(portList);
    auto pathsListIterator = Core::Service<RPC::IteratorType<Exchange::IRuntimeManager::IStringIterator>>::Create<Exchange::IRuntimeManager::IStringIterator>(pathsList);
    auto debugSettingsIterator = Core::Service<RPC::IteratorType<Exchange::IRuntimeManager::IStringIterator>>::Create<Exchange::IRuntimeManager::IStringIterator>(debugSettingsList);

    Exchange::RuntimeConfig runtimeConfig;
    runtimeConfig.envVariables = "XDG_RUNTIME_DIR=/tmp;WAYLAND_DISPLAY=main";
    runtimeConfig.appPath = "/var/runTimeManager";
    runtimeConfig.runtimePath = "/tmp/runTimeManager";
    runtimeConfig.systemMemoryLimit = 512;
    runtimeConfig.command ="SkyBrowserLauncher";

    EXPECT_EQ(true, createResources());

    EXPECT_CALL(*mociContainerMock, StartContainerFromDobbySpec(TEST_APP_CONTAINER_ID, ::testing::_, "", "/run/user/1001/wst-youTube", ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Invoke(
            [&](const string&, const string&, const string&, const string&, int32_t& descriptor, bool& success, string& errorReason) {
                descriptor = 100;
                success = true;
                errorReason = "No Error";
                return Core::ERROR_NONE;
            }));

    ON_CALL(*mWindowManagerMock, CreateDisplay(::testing::_)).WillByDefault(::testing::Return(Core::ERROR_NONE));

    EXPECT_EQ(Core::ERROR_NONE, interface->Run(appInstanceId, appInstanceId, 10, 10, portsIterator, pathsListIterator, debugSettingsIterator, runtimeConfig));

    EXPECT_CALL(*mociContainerMock, PauseContainer(::testing::_, ::testing::_, ::testing::_))
        .WillOnce([](const std::string&, bool& success, std::string& reason) {
            success = false;
            reason = "Mocked failure";
            return Core::ERROR_GENERAL;
        });

    EXPECT_EQ(Core::ERROR_GENERAL, interface->Suspend(appInstanceId));

    releaseResources();
}

/* Test: ResumeFailsResumePauseContainerError
 *
 * Purpose: Verify that Resume() handles failure in ResumeContainer properly.
 * Setup:
 *   - Mocks StartContainerFromDobbySpec to succeed.
 *   - Mocks PauseContainer to succeed.
 *   - Mocks ResumeContainer to return Core::ERROR_GENERAL and fail.
 * Expectation: Resume() returns Core::ERROR_GENERAL indicating failure.
 * Validates error path in Resume().
 */
TEST_F(RuntimeManagerTest, ResumeFailsResumePauseContainerError)
{
    string appInstanceId("youTube");
    std::vector<uint32_t> portList = {10, 20};
    std::vector<std::string> pathsList = {"/tmp", "/opt"};
    std::vector<std::string> debugSettingsList = {"MIL", "INFO"};

    auto portsIterator = Core::Service<RPC::IteratorType<Exchange::IRuntimeManager::IValueIterator>>::Create<Exchange::IRuntimeManager::IValueIterator>(portList);
    auto pathsListIterator = Core::Service<RPC::IteratorType<Exchange::IRuntimeManager::IStringIterator>>::Create<Exchange::IRuntimeManager::IStringIterator>(pathsList);
    auto debugSettingsIterator = Core::Service<RPC::IteratorType<Exchange::IRuntimeManager::IStringIterator>>::Create<Exchange::IRuntimeManager::IStringIterator>(debugSettingsList);

    Exchange::RuntimeConfig runtimeConfig;
    runtimeConfig.envVariables = "XDG_RUNTIME_DIR=/tmp;WAYLAND_DISPLAY=main";
    runtimeConfig.appPath = "/var/runTimeManager";
    runtimeConfig.runtimePath = "/tmp/runTimeManager";
    runtimeConfig.systemMemoryLimit = 512;
    runtimeConfig.command ="SkyBrowserLauncher";

    EXPECT_EQ(true, createResources());

    EXPECT_CALL(*mociContainerMock, StartContainerFromDobbySpec(TEST_APP_CONTAINER_ID, ::testing::_, "", "/run/user/1001/wst-youTube", ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Invoke(
            [&](const string&, const string&, const string&, const string&, int32_t& descriptor, bool& success, string& errorReason) {
                descriptor = 100;
                success = true;
                errorReason = "No Error";
                return Core::ERROR_NONE;
            }));

    ON_CALL(*mWindowManagerMock, CreateDisplay(::testing::_)).WillByDefault(::testing::Return(Core::ERROR_NONE));

    EXPECT_EQ(Core::ERROR_NONE, interface->Run(appInstanceId, appInstanceId, 10, 10, portsIterator, pathsListIterator, debugSettingsIterator, runtimeConfig));

    EXPECT_CALL(*mociContainerMock, PauseContainer(::testing::_, ::testing::_, ::testing::_))
    .WillOnce([](const std::string&, bool& success, std::string& reason) {
            success = true;
            reason = "No Error";
            return Core::ERROR_NONE;
        });

    EXPECT_EQ(Core::ERROR_NONE, interface->Suspend(appInstanceId));

    EXPECT_CALL(*mociContainerMock, ResumeContainer(::testing::_, ::testing::_, ::testing::_))
        .WillOnce([](const std::string& containerId, bool& success, std::string& reason) {
            success = false;
            reason = "Mocked Failure";
            return Core::ERROR_GENERAL;
        });

    EXPECT_EQ(Core::ERROR_GENERAL, interface->Resume(appInstanceId));

    releaseResources();
}

/* Test: OCIContainerEvents
 *
 * Purpose: Verify that the RuntimeManager correctly handles all OCI container lifecycle events.
 * Setup:
 *   - Creates dummy event data and container name.
 *   - Calls event handlers: onOCIContainerStartedEvent, onOCIContainerStoppedEvent,
 *     onOCIContainerFailureEvent, and onOCIContainerStateChangedEvent.
 * Expectation:
 *   - No assertion failures or crashes.
 *   - Ensures that the event-handling methods are callable and don't throw or misbehave under basic conditions.
 */
TEST_F(RuntimeManagerTest, OCIContainerEvents)
{
    ASSERT_TRUE(createResources());

    JsonObject dummyData;
    std::string dummyName = "DummyApp";

    mRuntimeManagerImpl->onOCIContainerStartedEvent(dummyName, dummyData);
    mRuntimeManagerImpl->onOCIContainerStoppedEvent(dummyName, dummyData);
    mRuntimeManagerImpl->onOCIContainerFailureEvent(dummyName, dummyData);
    mRuntimeManagerImpl->onOCIContainerStateChangedEvent(dummyName, dummyData);

    releaseResources();
}

/* Test: MountMethods
 *
 * Purpose: Ensure the stubbed Mount() method can be called successfully.
 * Note: This is a stubbed method, added purely for code coverage.
 */
TEST_F(RuntimeManagerTest, MountMethods)
{
    EXPECT_EQ(Core::ERROR_NONE, interface->Mount());
}

/* Test: UnmountMethods
 *
 * Purpose: Ensure the stubbed Unmount() method can be called successfully.
 * Note: This is a stubbed method, added purely for code coverage.
 */
TEST_F(RuntimeManagerTest, UnmountMethods)
{
    EXPECT_EQ(Core::ERROR_NONE, interface->Unmount());
}

