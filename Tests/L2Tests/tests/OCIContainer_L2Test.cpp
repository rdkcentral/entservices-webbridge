/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2025 RDK Management
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
#include "L2Tests.h"
#include "L2TestsMock.h"
#include <condition_variable>
#include <fstream>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <interfaces/IOCIContainer.h>

#define EVNT_TIMEOUT (5000)
#define OCICONTAINER_CALLSIGN _T("org.rdk.OCIContainer.1")
#define OCICONTAINERTEST_CALLSIGN _T("L2tests.1")

#define TEST_LOG(x, ...)                                                                                                                         \
    fprintf(stderr, "\033[1;32m[%s:%d](%s)<PID:%d><TID:%d>" x "\n\033[0m", __FILE__, __LINE__, __FUNCTION__, getpid(), gettid(), ##__VA_ARGS__); \
    fflush(stderr);

using ::testing::NiceMock;
using namespace WPEFramework;
using testing::StrictMock;
using ContainerState = WPEFramework::Exchange::IOCIContainer::ContainerState;
using OmiErrorListener = omi::IOmiProxy::OmiErrorListener;

typedef enum : uint32_t {

    ON_CONTAINER_STARTED = 0x00000001,
    ON_CONTAINER_STOPPED = 0x00000002,
    ON_CONTAINER_FAILED = 0x00000003,
    ON_CONTAINER_STATECHANGED = 0x00000004,
    OCICONTAINER_STATUS_INVALID = 0x00000000

} OCIContainerL2test_async_events_t;

class AsyncHandlerMock_OCIContainer {
public:
    AsyncHandlerMock_OCIContainer()
    {
    }
    MOCK_METHOD(void, onContainerStarted, (const JsonObject& message));
    MOCK_METHOD(void, onContainerStopped, (const JsonObject& message));
    MOCK_METHOD(void, onContainerFailed, (const JsonObject& message));
    MOCK_METHOD(void, onContainerStateChanged, (const JsonObject& message));
};

// Notification Handler Class for OCIContainer
class OCIContainerNotificationHandler : public Exchange::IOCIContainer::INotification {
private:
    /** @brief Mutex */
    std::mutex m_mutex;

    /** @brief Condition variable */
    std::condition_variable m_condition_variable;

    /** @brief Event signalled flag */
    uint32_t m_event_signalled;

    BEGIN_INTERFACE_MAP(Notification)
    INTERFACE_ENTRY(Exchange::IOCIContainer::INotification)
    END_INTERFACE_MAP

public:
    OCIContainerNotificationHandler() {}
    ~OCIContainerNotificationHandler() {}

    // OCIContainer notifications
    void OnContainerStarted(const string& containerId, const string& name) override
    {
        TEST_LOG("OnContainerStarted event triggered ***\n");
        std::unique_lock<std::mutex> lock(m_mutex);

        TEST_LOG("OnContainerStarted received (CONTAINER ID): %s\n", containerId.c_str());
        TEST_LOG("OnContainerStarted received (NAME): %s\n", name.c_str());
        /* Notify the requester thread. */
        m_event_signalled |= ON_CONTAINER_STARTED;
        m_condition_variable.notify_one();
    }

    void OnContainerStopped(const string& containerId, const string& name) override
    {
        TEST_LOG("OnContainerStopped event triggered ***\n");
        std::unique_lock<std::mutex> lock(m_mutex);

        TEST_LOG("OnContainerStopped received (CONTAINER ID): %s\n", containerId.c_str());
        TEST_LOG("OnContainerStopped received (NAME): %s\n", name.c_str());
        /* Notify the requester thread. */
        m_event_signalled |= ON_CONTAINER_STOPPED;
        m_condition_variable.notify_one();
    }

    void OnContainerFailed(const string& containerId, const string& name, uint32_t error) override
    {
        TEST_LOG("OnContainerFailed event triggered ***\n");
        std::unique_lock<std::mutex> lock(m_mutex);

        TEST_LOG("OnContainerFailed received (CONTAINER ID): %s\n", containerId.c_str());
        TEST_LOG("OnContainerFailed received (NAME): %s\n", name.c_str());
        TEST_LOG("OnContainerFailed received (ERROR): %u\n", error);
        /* Notify the requester thread. */
        m_event_signalled |= ON_CONTAINER_FAILED;
        m_condition_variable.notify_one();
    }

    void OnContainerStateChanged(const string& containerId, ContainerState state) override
    {
        TEST_LOG("OnContainerStateChanged event triggered ***\n");
        std::unique_lock<std::mutex> lock(m_mutex);

        TEST_LOG("OnContainerStateChanged received (CONTAINER ID): %s\n", containerId.c_str());
        TEST_LOG("OnContainerStateChanged received (STATE): %d\n", state);
        /* Notify the requester thread. */
        m_event_signalled |= ON_CONTAINER_STATECHANGED; // Use a specific flag for state change
        m_condition_variable.notify_one();
    }

    uint32_t WaitForRequestStatus(uint32_t timeout_ms, OCIContainerL2test_async_events_t expected_status)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        auto now = std::chrono::system_clock::now();
        std::chrono::milliseconds timeout(timeout_ms);
        uint32_t signalled = OCICONTAINER_STATUS_INVALID;

        while (!(expected_status & m_event_signalled)) {
            if (m_condition_variable.wait_until(lock, now + timeout) == std::cv_status::timeout) {
                TEST_LOG("Timeout waiting for request status event");
                break;
            }
        }

        signalled = m_event_signalled;
        return signalled;
    }
};

class OCIContainer_L2Test : public L2TestMocks {
protected:
    OCIContainer_L2Test();
    virtual ~OCIContainer_L2Test() override;

public:
    void onContainerStarted(const JsonObject& data);
    void onContainerStopped(const JsonObject& data);
    void onContainerFailed(const JsonObject& data);
    void onContainerStateChanged(const JsonObject& data);
    uint32_t WaitForRequestStatus(uint32_t timeout_ms, OCIContainerL2test_async_events_t expected_status);
    uint32_t CreateOCIContainerInterfaceObject();

    StateChangeListener storedListener = nullptr;
    const void* storedCbParams = nullptr;
    OmiErrorListener storedOmiListener = nullptr;
    const void* storedOmiCbParams = nullptr;

    void triggerStateChangeEvent(int32_t id, const std::string& name, IDobbyProxyEvents::ContainerState state)
    {
        if (storedListener) {
            storedListener(id, name, state, storedCbParams);
        }
    }

    void triggerOmiErrorEvent(const std::string& id, omi::IOmiProxy::ErrorType err)
    {
        if (storedOmiListener) {
            storedOmiListener(id, err, storedOmiCbParams);
        }
    }

private:
    /** @brief Mutex */
    std::mutex m_mutex;

    /** @brief Condition variable */
    std::condition_variable m_condition_variable;

    /** @brief Event signalled flag */
    uint32_t m_event_signalled;

protected:
    Core::ProxyType<RPC::InvokeServerType<1, 0, 4>> OCIcontainer_Engine;
    Core::ProxyType<RPC::CommunicatorClient> OCIcontainer_Client;

    /** @brief Pointer to the IShell interface */
    PluginHost::IShell* m_controller_OCIcontainer;

    /** @brief Pointer to the IOCIContainerinterface */
    Exchange::IOCIContainer* m_OCIContainerplugin;
};

OCIContainer_L2Test::OCIContainer_L2Test()
    : L2TestMocks()
{
    uint32_t status = Core::ERROR_GENERAL;

    EXPECT_CALL(*p_ipcservicemock, start())
        .WillOnce(::testing::Return(true));

    EXPECT_CALL(*p_dobbyProxyMock, registerListener(::testing::_, ::testing::_))
        .WillOnce(::testing::Invoke([&](const StateChangeListener& listener, const void* cbParams) {
            storedListener = listener;
            storedCbParams = cbParams;
            return 5; // Return a mock listener ID
        }));

    EXPECT_CALL(*p_mockOmiProxy, registerListener(::testing::_, ::testing::_))
        .WillOnce(::testing::Invoke([&](const OmiErrorListener& listener, const void* cbParams) {
            storedOmiListener = listener;
            storedOmiCbParams = cbParams;
            return 6; // Return a mock listener ID
        }));

    /* Activate plugin in constructor */
    status = ActivateService("org.rdk.OCIContainer");
    EXPECT_EQ(Core::ERROR_NONE, status);
}

OCIContainer_L2Test::~OCIContainer_L2Test()
{
    uint32_t status = Core::ERROR_GENERAL;

    /* Deactivate plugin in destructor */
    status = DeactivateService("org.rdk.OCIContainer");
    EXPECT_EQ(Core::ERROR_NONE, status);

    sleep(2); // Added sleep due to failures while activating/deactivating the plugin
}

uint32_t OCIContainer_L2Test::CreateOCIContainerInterfaceObject()
{
    uint32_t return_value = Core::ERROR_GENERAL;

    TEST_LOG("Creating OCIcontainer_Engine");
    OCIcontainer_Engine = Core::ProxyType<RPC::InvokeServerType<1, 0, 4>>::Create();
    OCIcontainer_Client = Core::ProxyType<RPC::CommunicatorClient>::Create(Core::NodeId("/tmp/communicator"), Core::ProxyType<Core::IIPCServer>(OCIcontainer_Engine));

    TEST_LOG("Creating OCIcontainer_Engine Announcements");
#if ((THUNDER_VERSION == 2) || ((THUNDER_VERSION == 4) && (THUNDER_VERSION_MINOR == 2)))
    OCIcontainer_Engine->Announcements(mOCIcontainer_Client->Announcement());
#endif
    if (!OCIcontainer_Client.IsValid()) {
        TEST_LOG("Invalid OCIcontainer_Client");
    } else {
        m_controller_OCIcontainer = OCIcontainer_Client->Open<PluginHost::IShell>(_T("org.rdk.OCIContainer"), ~0, 3000);
        if (m_controller_OCIcontainer) {
            m_OCIContainerplugin = m_controller_OCIcontainer->QueryInterface<Exchange::IOCIContainer>();
            return_value = Core::ERROR_NONE;
        }
    }
    return return_value;
}

uint32_t OCIContainer_L2Test::WaitForRequestStatus(uint32_t timeout_ms, OCIContainerL2test_async_events_t expected_status)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    auto now = std::chrono::system_clock::now();
    std::chrono::milliseconds timeout(timeout_ms);
    uint32_t signalled = OCICONTAINER_STATUS_INVALID;

    while (!(expected_status & m_event_signalled)) {
        if (m_condition_variable.wait_until(lock, now + timeout) == std::cv_status::timeout) {
            TEST_LOG("Timeout waiting for request status event");
            break;
        }
    }

    signalled = m_event_signalled;
    return signalled;
}

void OCIContainer_L2Test::onContainerStarted(const JsonObject& data)
{
    TEST_LOG("OnContainerStarted event triggered ***\n");
    std::unique_lock<std::mutex> lock(m_mutex);

    std::string str;
    data.ToString(str);

    TEST_LOG("OnContainerStarted received: %s\n", str.c_str());

    /* Notify the requester thread. */
    m_event_signalled |= ON_CONTAINER_STARTED;
    m_condition_variable.notify_one();
}

void OCIContainer_L2Test::onContainerStopped(const JsonObject& data)
{
    TEST_LOG("OnContainerStopped event triggered ***\n");
    std::unique_lock<std::mutex> lock(m_mutex);

    std::string str;
    data.ToString(str);

    TEST_LOG("OnContainerStopped received: %s\n", str.c_str());

    /* Notify the requester thread. */
    m_event_signalled |= ON_CONTAINER_STOPPED;
    m_condition_variable.notify_one();
}

void OCIContainer_L2Test::onContainerFailed(const JsonObject& data)
{
    TEST_LOG("OnContainerFailed event triggered ***\n");
    std::unique_lock<std::mutex> lock(m_mutex);

    std::string str;
    data.ToString(str);

    TEST_LOG("OnContainerFailed received: %s\n", str.c_str());

    /* Notify the requester thread. */
    m_event_signalled |= ON_CONTAINER_FAILED;
    m_condition_variable.notify_one();
}

void OCIContainer_L2Test::onContainerStateChanged(const JsonObject& data)
{
    TEST_LOG("OnContainerStateChanged event triggered ***\n");
    std::unique_lock<std::mutex> lock(m_mutex);

    std::string str;
    data.ToString(str);

    TEST_LOG("OnContainerStateChanged received: %s\n", str.c_str());

    /* Notify the requester thread. */
    m_event_signalled |= ON_CONTAINER_STATECHANGED;
    m_condition_variable.notify_one();
}

MATCHER_P(MatchRequestStatus, data, "")
{
    bool match = true;
    std::string expected;
    std::string actual;

    data.ToString(expected);
    arg.ToString(actual);
    TEST_LOG(" rec = %s, arg = %s", expected.c_str(), actual.c_str());
    EXPECT_STREQ(expected.c_str(), actual.c_str());

    return match;
}

/*
 * Test case to list containers using COMRPC
 * This test case verifies the ListContainers method of the OCIContainer plugin.
 */
TEST_F(OCIContainer_L2Test, ListContainer_COMRPC)
{
    uint32_t status = Core::ERROR_GENERAL;
    string containers, errorReason;
    bool success = false;
    if (CreateOCIContainerInterfaceObject() != Core::ERROR_NONE) {
        TEST_LOG("Invalid OCIContainer_Client");
    } else {
        EXPECT_TRUE(m_controller_OCIcontainer != nullptr);
        if (m_controller_OCIcontainer) {
            EXPECT_TRUE(m_OCIContainerplugin != nullptr);
            if (m_OCIContainerplugin) {
                m_OCIContainerplugin->AddRef();

                std::list<std::pair<int32_t, std::string>> containerslist = { { 91, "com.bskyb.epgui" }, { 94, "Netflix" } };
                EXPECT_CALL(*p_dobbyProxyMock, listContainers())
                    .WillOnce(::testing::Return(containerslist));

                status = m_OCIContainerplugin->ListContainers(containers, success, errorReason);
                EXPECT_EQ(status, Core::ERROR_NONE);
                EXPECT_TRUE(success);
                TEST_LOG("ListContainers returned containers: %s, success: %d, errorReason: %s", containers.c_str(), success, errorReason.c_str());

                m_OCIContainerplugin->Release();
            } else {
                TEST_LOG("m_OCIContainerplugin is NULL");
            }
            m_controller_OCIcontainer->Release();
        } else {
            TEST_LOG("m_controller_OCIcontainer is NULL");
        }
    }
}

/*
 * Test case to test the GetContainerInfo method of OCIContainer plugin using COMRPC.
 * This test case verifies that the GetContainerInfo method returns the correct information
 * for a given container ID.
 */
TEST_F(OCIContainer_L2Test, GetContainerInfo_COMRPC)
{
    uint32_t status = Core::ERROR_GENERAL;
    string containerID = "com.bskyb.epgui";
    string info, errorReason;
    bool success = false;

    if (CreateOCIContainerInterfaceObject() != Core::ERROR_NONE) {
        TEST_LOG("Invalid OCIContainer_Client");
    } else {
        EXPECT_TRUE(m_controller_OCIcontainer != nullptr);
        if (m_controller_OCIcontainer) {
            EXPECT_TRUE(m_OCIContainerplugin != nullptr);
            if (m_OCIContainerplugin) {
                m_OCIContainerplugin->AddRef();
                std::list<std::pair<int32_t, std::string>> containerslist = { { 91, "com.bskyb.epgui" }, { 94, "Netflix" } };
                EXPECT_CALL(*p_dobbyProxyMock, listContainers())
                    .WillOnce(::testing::Return(containerslist));

                EXPECT_CALL(*p_dobbyProxyMock, getContainerInfo(91))
                    .WillOnce(::testing::Return("{\"cpu\":{\"usage\" :{\"percpu\" :[3661526845,3773518079,4484546066,4700379608],"
                                                "\"total\" : 16619970598}},\"gpu\":{\"memory\" :{\"failcnt\" : 0,\"limit\" : 209715200,\"max\" : 3911680,\"usage\" : 0}},"
                                                "\"id\":\"Cobalt-0\",\"ion\":{\"heaps\" :{\"ion.\" :{\"failcnt\" : null,\"limit\" : null,\"max\" : null,\"usage\" : null}}},"
                                                "\"memory\":{\"user\" :{\"failcnt\" : 0,\"limit\" : 419430400,\"max\" : 73375744,\"usage\" : 53297152}},\"pids\":[13132,13418],"
                                                "\"processes\":[{\"cmdline\" : \"/usr/libexec/DobbyInit /usr/bin/WPEProcess -l libWPEFrameworkCobaltImpl.so -c"
                                                " CobaltImplementation -C Cobalt-0 -r /tmp/communicator -i 64 -x 48 -p \"/opt/persistent/rdkservices/Cobalt-0/\" -s"
                                                " \"/usr/lib/wpeframework/plugins/\" -d \"/usr/share/WPEFramework/Cobalt/\" -a \"/usr/bin/\" -v \"/tmp/Cobalt-0/\" -m"
                                                " \"/usr/lib/wpeframework/proxystubs/\" -P \"/opt/minidumps/\" \",\"executable\" : \"/usr/libexec/DobbyInit\",\"nsPid\" : 1,"
                                                "\"pid\" : 13132},{\"cmdline\" : \"WPEProcess -l libWPEFrameworkCobaltImpl.so -c CobaltImplementation -C Cobalt-0 -r /tmp/communicator"
                                                " -i 64 -x 48 -p \"/opt/persistent/rdkservices/Cobalt-0/\" -s \"/usr/lib/wpeframework/plugins/\" -d \"/usr/share/WPEFramework/Cobalt/\""
                                                " -a \"/usr/bin/\" -v \"/tmp/Cobalt-0/\" -m \"/usr/lib/wpeframework/proxystubs/\" -P \"/opt/minidumps/\" \",\"executable\" : "
                                                "\"/usr/bin/WPEProcess\",\"nsPid\" : 6,\"pid\" : 13418}],\"state\":\"running\",\"timestamp\":1298657054196}"));

                status = m_OCIContainerplugin->GetContainerInfo(containerID, info, success, errorReason);
                EXPECT_EQ(status, Core::ERROR_NONE);
                EXPECT_TRUE(success);

                TEST_LOG("GetContainerInfo returned info: %s, success: %d, errorReason: %s", info.c_str(), success, errorReason.c_str());
                m_OCIContainerplugin->Release();
            } else {
                TEST_LOG("m_OCIContainerplugin is NULL");
            }
            m_controller_OCIcontainer->Release();
        } else {
            TEST_LOG("m_controller_OCIcontainer is NULL");
        }
    }
}

/*
 * Test case to get the STARTING state of a container using COMRPC
 * This test case verifies the GetContainerState method of the OCIContainer plugin.
 */
TEST_F(OCIContainer_L2Test, ListContainer_STARTING_COMRPC)
{
    uint32_t status = Core::ERROR_GENERAL;
    string containerID = "com.bskyb.epgui";
    ContainerState state;
    string errorReason;
    bool success = false;
    if (CreateOCIContainerInterfaceObject() != Core::ERROR_NONE) {
        TEST_LOG("Invalid OCIContainer_Client");
    } else {
        EXPECT_TRUE(m_controller_OCIcontainer != nullptr);
        if (m_controller_OCIcontainer) {
            EXPECT_TRUE(m_OCIContainerplugin != nullptr);
            if (m_OCIContainerplugin) {
                m_OCIContainerplugin->AddRef();

                std::list<std::pair<int32_t, std::string>> containerslist = { { 91, "com.bskyb.epgui" }, { 94, "Netflix" } };
                EXPECT_CALL(*p_dobbyProxyMock, listContainers())
                    .WillOnce(::testing::Return(containerslist));

                EXPECT_CALL(*p_dobbyProxyMock, getContainerState(91))
                    .WillOnce(::testing::Return(IDobbyProxyEvents::ContainerState::Starting));

                status = m_OCIContainerplugin->GetContainerState(containerID, state, success, errorReason);
                EXPECT_EQ(status, Core::ERROR_NONE);
                EXPECT_EQ(state, ContainerState::STARTING);
                EXPECT_TRUE(success);
                TEST_LOG("GetContainerState returned state: %d, success: %d, errorReason: %s", state, success, errorReason.c_str());

                m_OCIContainerplugin->Release();
            } else {
                TEST_LOG("m_OCIContainerplugin is NULL");
            }
            m_controller_OCIcontainer->Release();
        } else {
            TEST_LOG("m_controller_OCIcontainer is NULL");
        }
    }
}

/*
 * Test case to get the RUNNING state of a container using COMRPC
 * This test case verifies the GetContainerState method of the OCIContainer plugin.
 */
TEST_F(OCIContainer_L2Test, ListContainer_RUNNING_COMRPC)
{
    uint32_t status = Core::ERROR_GENERAL;
    string containerID = "com.bskyb.epgui";
    ContainerState state;
    string errorReason;
    bool success = false;
    if (CreateOCIContainerInterfaceObject() != Core::ERROR_NONE) {
        TEST_LOG("Invalid OCIContainer_Client");
    } else {
        EXPECT_TRUE(m_controller_OCIcontainer != nullptr);
        if (m_controller_OCIcontainer) {
            EXPECT_TRUE(m_OCIContainerplugin != nullptr);
            if (m_OCIContainerplugin) {
                m_OCIContainerplugin->AddRef();

                std::list<std::pair<int32_t, std::string>> containerslist = { { 91, "com.bskyb.epgui" }, { 94, "Netflix" } };
                EXPECT_CALL(*p_dobbyProxyMock, listContainers())
                    .WillOnce(::testing::Return(containerslist));

                EXPECT_CALL(*p_dobbyProxyMock, getContainerState(91))
                    .WillOnce(::testing::Return(IDobbyProxyEvents::ContainerState::Running));

                status = m_OCIContainerplugin->GetContainerState(containerID, state, success, errorReason);
                EXPECT_EQ(status, Core::ERROR_NONE);
                EXPECT_EQ(state, ContainerState::RUNNING);
                EXPECT_TRUE(success);
                TEST_LOG("GetContainerState returned state: %d, success: %d, errorReason: %s", state, success, errorReason.c_str());

                m_OCIContainerplugin->Release();
            } else {
                TEST_LOG("m_OCIContainerplugin is NULL");
            }
            m_controller_OCIcontainer->Release();
        } else {
            TEST_LOG("m_controller_OCIcontainer is NULL");
        }
    }
}

/*
 * Test case to get the STOPPING state of a container using COMRPC
 * This test case verifies the GetContainerState method of the OCIContainer plugin.
 * It checks if the state is correctly returned as STOPPING when the container is in that state.
 */
TEST_F(OCIContainer_L2Test, ListContainer_STOPPING_COMRPC)
{
    uint32_t status = Core::ERROR_GENERAL;
    string containerID = "com.bskyb.epgui";
    ContainerState state;
    string errorReason;
    bool success = false;
    if (CreateOCIContainerInterfaceObject() != Core::ERROR_NONE) {
        TEST_LOG("Invalid OCIContainer_Client");
    } else {
        EXPECT_TRUE(m_controller_OCIcontainer != nullptr);
        if (m_controller_OCIcontainer) {
            EXPECT_TRUE(m_OCIContainerplugin != nullptr);
            if (m_OCIContainerplugin) {
                m_OCIContainerplugin->AddRef();

                std::list<std::pair<int32_t, std::string>> containerslist = { { 91, "com.bskyb.epgui" }, { 94, "Netflix" } };
                EXPECT_CALL(*p_dobbyProxyMock, listContainers())
                    .WillOnce(::testing::Return(containerslist));

                EXPECT_CALL(*p_dobbyProxyMock, getContainerState(91))
                    .WillOnce(::testing::Return(IDobbyProxyEvents::ContainerState::Stopping));

                status = m_OCIContainerplugin->GetContainerState(containerID, state, success, errorReason);
                EXPECT_EQ(status, Core::ERROR_NONE);
                EXPECT_EQ(state, ContainerState::STOPPING);
                EXPECT_TRUE(success);
                TEST_LOG("GetContainerState returned state: %d, success: %d, errorReason: %s", state, success, errorReason.c_str());

                m_OCIContainerplugin->Release();
            } else {
                TEST_LOG("m_OCIContainerplugin is NULL");
            }
            m_controller_OCIcontainer->Release();
        } else {
            TEST_LOG("m_controller_OCIcontainer is NULL");
        }
    }
}

/*
 * Test case to get the PAUSED state of a container using COMRPC
 * This test case verifies the GetContainerState method of the OCIContainer plugin.
 * It checks if the state is correctly returned as PAUSED when the container is in that state.
 */
TEST_F(OCIContainer_L2Test, ListContainer_PAUSED_COMRPC)
{
    uint32_t status = Core::ERROR_GENERAL;
    string containerID = "com.bskyb.epgui";
    ContainerState state;
    string errorReason;
    bool success = false;
    if (CreateOCIContainerInterfaceObject() != Core::ERROR_NONE) {
        TEST_LOG("Invalid OCIContainer_Client");
    } else {
        EXPECT_TRUE(m_controller_OCIcontainer != nullptr);
        if (m_controller_OCIcontainer) {
            EXPECT_TRUE(m_OCIContainerplugin != nullptr);
            if (m_OCIContainerplugin) {
                m_OCIContainerplugin->AddRef();

                std::list<std::pair<int32_t, std::string>> containerslist = { { 91, "com.bskyb.epgui" }, { 94, "Netflix" } };
                EXPECT_CALL(*p_dobbyProxyMock, listContainers())
                    .WillOnce(::testing::Return(containerslist));

                EXPECT_CALL(*p_dobbyProxyMock, getContainerState(91))
                    .WillOnce(::testing::Return(IDobbyProxyEvents::ContainerState::Paused));

                status = m_OCIContainerplugin->GetContainerState(containerID, state, success, errorReason);
                EXPECT_EQ(status, Core::ERROR_NONE);
                EXPECT_EQ(state, ContainerState::PAUSED);
                EXPECT_TRUE(success);
                TEST_LOG("GetContainerState returned state: %d, success: %d, errorReason: %s", state, success, errorReason.c_str());

                m_OCIContainerplugin->Release();
            } else {
                TEST_LOG("m_OCIContainerplugin is NULL");
            }
            m_controller_OCIcontainer->Release();
        } else {
            TEST_LOG("m_controller_OCIcontainer is NULL");
        }
    }
}

/*
 * Test case to get the STOPPED state of a container using COMRPC
 * This test case verifies the GetContainerState method of the OCIContainer plugin.
 * It checks if the state is correctly returned as STOPPED when the container is in that state.
 */
TEST_F(OCIContainer_L2Test, ListContainer_STOPPED_COMRPC)
{
    uint32_t status = Core::ERROR_GENERAL;
    string containerID = "com.bskyb.epgui";
    ContainerState state;
    string errorReason;
    bool success = false;
    if (CreateOCIContainerInterfaceObject() != Core::ERROR_NONE) {
        TEST_LOG("Invalid OCIContainer_Client");
    } else {
        EXPECT_TRUE(m_controller_OCIcontainer != nullptr);
        if (m_controller_OCIcontainer) {
            EXPECT_TRUE(m_OCIContainerplugin != nullptr);
            if (m_OCIContainerplugin) {
                m_OCIContainerplugin->AddRef();

                std::list<std::pair<int32_t, std::string>> containerslist = { { 91, "com.bskyb.epgui" }, { 94, "Netflix" } };
                EXPECT_CALL(*p_dobbyProxyMock, listContainers())
                    .WillOnce(::testing::Return(containerslist));

                EXPECT_CALL(*p_dobbyProxyMock, getContainerState(91))
                    .WillOnce(::testing::Return(IDobbyProxyEvents::ContainerState::Stopped));

                status = m_OCIContainerplugin->GetContainerState(containerID, state, success, errorReason);
                EXPECT_EQ(status, Core::ERROR_NONE);
                EXPECT_EQ(state, ContainerState::STOPPED);
                EXPECT_TRUE(success);
                TEST_LOG("GetContainerState returned state: %d, success: %d, errorReason: %s", state, success, errorReason.c_str());

                m_OCIContainerplugin->Release();
            } else {
                TEST_LOG("m_OCIContainerplugin is NULL");
            }
            m_controller_OCIcontainer->Release();
        } else {
            TEST_LOG("m_controller_OCIcontainer is NULL");
        }
    }
}

/*
 * Test case to get the HIBERNATING state of a container using COMRPC
 * This test case verifies the GetContainerState method of the OCIContainer plugin.
 * It checks if the state is correctly returned as HIBERNATING when the container is in that state.
 */

TEST_F(OCIContainer_L2Test, ListContainer_HIBERNATING_COMRPC)
{
    uint32_t status = Core::ERROR_GENERAL;
    string containerID = "com.bskyb.epgui";
    ContainerState state;
    string errorReason;
    bool success = false;
    if (CreateOCIContainerInterfaceObject() != Core::ERROR_NONE) {
        TEST_LOG("Invalid OCIContainer_Client");
    } else {
        EXPECT_TRUE(m_controller_OCIcontainer != nullptr);
        if (m_controller_OCIcontainer) {
            EXPECT_TRUE(m_OCIContainerplugin != nullptr);
            if (m_OCIContainerplugin) {
                m_OCIContainerplugin->AddRef();

                std::list<std::pair<int32_t, std::string>> containerslist = { { 91, "com.bskyb.epgui" }, { 94, "Netflix" } };
                EXPECT_CALL(*p_dobbyProxyMock, listContainers())
                    .WillOnce(::testing::Return(containerslist));

                EXPECT_CALL(*p_dobbyProxyMock, getContainerState(91))
                    .WillOnce(::testing::Return(IDobbyProxyEvents::ContainerState::Hibernating));

                status = m_OCIContainerplugin->GetContainerState(containerID, state, success, errorReason);
                EXPECT_EQ(status, Core::ERROR_NONE);
                EXPECT_EQ(state, ContainerState::HIBERNATING);
                EXPECT_TRUE(success);
                TEST_LOG("GetContainerState returned state: %d, success: %d, errorReason: %s", state, success, errorReason.c_str());

                m_OCIContainerplugin->Release();
            } else {
                TEST_LOG("m_OCIContainerplugin is NULL");
            }
            m_controller_OCIcontainer->Release();
        } else {
            TEST_LOG("m_controller_OCIcontainer is NULL");
        }
    }
}

/*
 * Test case to get the HIBERNATED state of a container using COMRPC
 * This test case verifies the GetContainerState method of the OCIContainer plugin.
 * It checks if the state is correctly returned as HIBERNATED when the container is in that state.
 */
TEST_F(OCIContainer_L2Test, ListContainer_HIBERNATED_COMRPC)
{
    uint32_t status = Core::ERROR_GENERAL;
    string containerID = "com.bskyb.epgui";
    ContainerState state;
    string errorReason;
    bool success = false;
    if (CreateOCIContainerInterfaceObject() != Core::ERROR_NONE) {
        TEST_LOG("Invalid OCIContainer_Client");
    } else {
        EXPECT_TRUE(m_controller_OCIcontainer != nullptr);
        if (m_controller_OCIcontainer) {
            EXPECT_TRUE(m_OCIContainerplugin != nullptr);
            if (m_OCIContainerplugin) {
                m_OCIContainerplugin->AddRef();

                std::list<std::pair<int32_t, std::string>> containerslist = { { 91, "com.bskyb.epgui" }, { 94, "Netflix" } };
                EXPECT_CALL(*p_dobbyProxyMock, listContainers())
                    .WillOnce(::testing::Return(containerslist));

                EXPECT_CALL(*p_dobbyProxyMock, getContainerState(91))
                    .WillOnce(::testing::Return(IDobbyProxyEvents::ContainerState::Hibernated));

                status = m_OCIContainerplugin->GetContainerState(containerID, state, success, errorReason);
                EXPECT_EQ(status, Core::ERROR_NONE);
                EXPECT_EQ(state, ContainerState::HIBERNATED);
                EXPECT_TRUE(success);
                TEST_LOG("GetContainerState returned state: %d, success: %d, errorReason: %s", state, success, errorReason.c_str());

                m_OCIContainerplugin->Release();
            } else {
                TEST_LOG("m_OCIContainerplugin is NULL");
            }
            m_controller_OCIcontainer->Release();
        } else {
            TEST_LOG("m_controller_OCIcontainer is NULL");
        }
    }
}

/*
 * Test case to get the AWAKENING state of a container using COMRPC
 * This test case verifies the GetContainerState method of the OCIContainer plugin.
 * It checks if the state is correctly returned as AWAKENING when the container is in that state.
 */

TEST_F(OCIContainer_L2Test, ListContainer_AWAKENING_COMRPC)
{
    uint32_t status = Core::ERROR_GENERAL;
    string containerID = "com.bskyb.epgui";
    ContainerState state;
    string errorReason;
    bool success = false;
    if (CreateOCIContainerInterfaceObject() != Core::ERROR_NONE) {
        TEST_LOG("Invalid OCIContainer_Client");
    } else {
        EXPECT_TRUE(m_controller_OCIcontainer != nullptr);
        if (m_controller_OCIcontainer) {
            EXPECT_TRUE(m_OCIContainerplugin != nullptr);
            if (m_OCIContainerplugin) {
                m_OCIContainerplugin->AddRef();

                std::list<std::pair<int32_t, std::string>> containerslist = { { 91, "com.bskyb.epgui" }, { 94, "Netflix" } };
                EXPECT_CALL(*p_dobbyProxyMock, listContainers())
                    .WillOnce(::testing::Return(containerslist));

                EXPECT_CALL(*p_dobbyProxyMock, getContainerState(91))
                    .WillOnce(::testing::Return(IDobbyProxyEvents::ContainerState::Awakening));

                status = m_OCIContainerplugin->GetContainerState(containerID, state, success, errorReason);
                EXPECT_EQ(status, Core::ERROR_NONE);
                EXPECT_EQ(state, ContainerState::AWAKENING);
                EXPECT_TRUE(success);
                TEST_LOG("GetContainerState returned state: %d, success: %d, errorReason: %s", state, success, errorReason.c_str());

                m_OCIContainerplugin->Release();
            } else {
                TEST_LOG("m_OCIContainerplugin is NULL");
            }
            m_controller_OCIcontainer->Release();
        } else {
            TEST_LOG("m_controller_OCIcontainer is NULL");
        }
    }
}

/*
 * Test case to get the INVALID state of a container using COMRPC
 * This test case verifies the GetContainerState method of the OCIContainer plugin.
 * It checks if the state is correctly returned as INVALID when the container is in that state.
 */
TEST_F(OCIContainer_L2Test, ListContainer_INVALID_COMRPC)
{
    uint32_t status = Core::ERROR_GENERAL;
    string containerID = "com.bskyb.epgui";
    ContainerState state;
    string errorReason;
    bool success = false;
    if (CreateOCIContainerInterfaceObject() != Core::ERROR_NONE) {
        TEST_LOG("Invalid OCIContainer_Client");
    } else {
        EXPECT_TRUE(m_controller_OCIcontainer != nullptr);
        if (m_controller_OCIcontainer) {
            EXPECT_TRUE(m_OCIContainerplugin != nullptr);
            if (m_OCIContainerplugin) {
                m_OCIContainerplugin->AddRef();

                std::list<std::pair<int32_t, std::string>> containerslist = { { 91, "com.bskyb.epgui" }, { 94, "Netflix" } };
                EXPECT_CALL(*p_dobbyProxyMock, listContainers())
                    .WillOnce(::testing::Return(containerslist));

                EXPECT_CALL(*p_dobbyProxyMock, getContainerState(91))
                    .WillOnce(::testing::Return(IDobbyProxyEvents::ContainerState::Invalid));

                status = m_OCIContainerplugin->GetContainerState(containerID, state, success, errorReason);
                EXPECT_EQ(status, Core::ERROR_NONE);
                EXPECT_EQ(state, ContainerState::INVALID);
                EXPECT_TRUE(success);
                TEST_LOG("GetContainerState returned state: %d, success: %d, errorReason: %s", state, success, errorReason.c_str());

                m_OCIContainerplugin->Release();
            } else {
                TEST_LOG("m_OCIContainerplugin is NULL");
            }
            m_controller_OCIcontainer->Release();
        } else {
            TEST_LOG("m_controller_OCIcontainer is NULL");
        }
    }
}

/*
 * Test case to start a container using COMRPC
 * This test case verifies the StartContainer method of the OCIContainer plugin.
 * It checks if the container can be started successfully and if the correct descriptor is returned.
 */
TEST_F(OCIContainer_L2Test, StartContainer_COMRPC)
{
    uint32_t status = Core::ERROR_GENERAL;
    Core::Sink<OCIContainerNotificationHandler> notify;
    uint32_t signalled = OCICONTAINER_STATUS_INVALID;
    string containerID = "com.bskyb.epgui", bundlepath = "/containers/myBundle", command = "command", westerOSSocket = "/usr/mySocket";
    int32_t descriptor = 0;
    string errorReason;
    bool success = false;
    if (CreateOCIContainerInterfaceObject() != Core::ERROR_NONE) {
        TEST_LOG("Invalid OCIContainer_Client");
    } else {
        EXPECT_TRUE(m_controller_OCIcontainer != nullptr);
        if (m_controller_OCIcontainer) {
            EXPECT_TRUE(m_OCIContainerplugin != nullptr);
            if (m_OCIContainerplugin) {
                m_OCIContainerplugin->AddRef();
                m_OCIContainerplugin->Register(&notify);

                EXPECT_CALL(*p_dobbyProxyMock, startContainerFromBundle(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
                    .WillOnce(::testing::Return(91));

                status = m_OCIContainerplugin->StartContainer(containerID, bundlepath, command, westerOSSocket, descriptor, success, errorReason);

                // manually trigger the state change event for the container
                // as the mock does not trigger it
                std::list<std::pair<int32_t, std::string>> containerslist = { { 91, "com.bskyb.epgui" }, { 94, "Netflix" } };
                EXPECT_CALL(*p_dobbyProxyMock, listContainers())
                    .WillRepeatedly(::testing::Return(containerslist));
                this->triggerStateChangeEvent(91, containerID, IDobbyProxyEvents::ContainerState::Starting);

                EXPECT_EQ(status, Core::ERROR_NONE);
                EXPECT_TRUE(success);
                TEST_LOG("StartContainer returned descriptor: %d, errorReason: %s", descriptor, errorReason.c_str());

                signalled = notify.WaitForRequestStatus(EVNT_TIMEOUT, ON_CONTAINER_STATECHANGED);
                EXPECT_TRUE(signalled & ON_CONTAINER_STATECHANGED);

                m_OCIContainerplugin->Unregister(&notify);
                m_OCIContainerplugin->Release();
            } else {
                TEST_LOG("m_OCIContainerplugin is NULL");
            }
            m_controller_OCIcontainer->Release();
        } else {
            TEST_LOG("m_controller_OCIcontainer is NULL");
        }
    }
}

/*
 * Test case to start a container from a Dobby spec using COMRPC
 * This test case verifies the StartContainerFromDobbySpec method of the OCIContainer plugin.
 * It checks if the container can be started successfully and if the correct descriptor is returned.
 */
TEST_F(OCIContainer_L2Test, StartContainerFromDobbySpec_COMRPC)
{
    uint32_t status = Core::ERROR_GENERAL;
    Core::Sink<OCIContainerNotificationHandler> notify;
    string containerID = "com.bskyb.epgui", dobbySpec = "/containers/dobbySpec", command = "command", westerOSSocket = "/usr/mySocket";
    int32_t descriptor = 0;
    string errorReason;
    bool success = false;
    if (CreateOCIContainerInterfaceObject() != Core::ERROR_NONE) {
        TEST_LOG("Invalid OCIContainer_Client");
    } else {
        EXPECT_TRUE(m_controller_OCIcontainer != nullptr);
        if (m_controller_OCIcontainer) {
            EXPECT_TRUE(m_OCIContainerplugin != nullptr);
            if (m_OCIContainerplugin) {
                m_OCIContainerplugin->AddRef();

                EXPECT_CALL(*p_dobbyProxyMock, startContainerFromSpec(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
                    .WillOnce(::testing::Return(91));

                status = m_OCIContainerplugin->StartContainerFromDobbySpec(containerID, dobbySpec, command, westerOSSocket, descriptor, success, errorReason);
                EXPECT_EQ(status, Core::ERROR_NONE);
                EXPECT_TRUE(success);
                TEST_LOG("StartContainerFromDobbySpec returned descriptor: %d, errorReason: %s", descriptor, errorReason.c_str());

                m_OCIContainerplugin->Release();
            } else {
                TEST_LOG("m_OCIContainerplugin is NULL");
            }
            m_controller_OCIcontainer->Release();
        } else {
            TEST_LOG("m_controller_OCIcontainer is NULL");
        }
    }
}

/*
 * Test case to stop a container using COMRPC
 * This test case verifies the StopContainer method of the OCIContainer plugin.
 * It checks if the container can be stopped successfully and if the correct status is returned.
 */
TEST_F(OCIContainer_L2Test, StopContainer_COMRPC)
{
    uint32_t status = Core::ERROR_GENERAL;
    Core::Sink<OCIContainerNotificationHandler> notify;
    uint32_t signalled = OCICONTAINER_STATUS_INVALID;
    string containerID = "com.bskyb.epgui";
    bool force = true;
    string errorReason;
    bool success = false;
    if (CreateOCIContainerInterfaceObject() != Core::ERROR_NONE) {
        TEST_LOG("Invalid OCIContainer_Client");
    } else {
        EXPECT_TRUE(m_controller_OCIcontainer != nullptr);
        if (m_controller_OCIcontainer) {
            EXPECT_TRUE(m_OCIContainerplugin != nullptr);
            if (m_OCIContainerplugin) {
                m_OCIContainerplugin->AddRef();
                m_OCIContainerplugin->Register(&notify);

                std::list<std::pair<int32_t, std::string>> containerslist = { { 91, "com.bskyb.epgui" }, { 94, "Netflix" } };
                EXPECT_CALL(*p_dobbyProxyMock, listContainers())
                    .WillOnce(::testing::Return(containerslist));

                EXPECT_CALL(*p_dobbyProxyMock, stopContainer(91, ::testing::_))
                    .WillOnce(::testing::Return(true));

                status = m_OCIContainerplugin->StopContainer(containerID, force, success, errorReason);

                // manually trigger the state change event for the container
                // as the mock does not trigger it
                EXPECT_CALL(*p_dobbyProxyMock, listContainers())
                    .WillRepeatedly(::testing::Return(containerslist));
                this->triggerStateChangeEvent(91, containerID, IDobbyProxyEvents::ContainerState::Stopping);

                signalled = notify.WaitForRequestStatus(EVNT_TIMEOUT, ON_CONTAINER_STATECHANGED);
                EXPECT_TRUE(signalled & ON_CONTAINER_STATECHANGED);

                EXPECT_EQ(status, Core::ERROR_NONE);
                EXPECT_TRUE(success);

                m_OCIContainerplugin->Unregister(&notify);
                m_OCIContainerplugin->Release();
            } else {
                TEST_LOG("m_OCIContainerplugin is NULL");
            }
            m_controller_OCIcontainer->Release();
        } else {
            TEST_LOG("m_controller_OCIcontainer is NULL");
        }
    }
}

/*
 * Test case to pause a container using COMRPC
 * This test case verifies the PauseContainer method of the OCIContainer plugin.
 * It checks if the container can be paused successfully and if the correct status and error reason are returned.
 */
TEST_F(OCIContainer_L2Test, PauseContainer_COMRPC)
{
    uint32_t status = Core::ERROR_GENERAL;
    Core::Sink<OCIContainerNotificationHandler> notify;
    uint32_t signalled = OCICONTAINER_STATUS_INVALID;
    string containerID = "com.bskyb.epgui";
    string errorReason;
    bool success = false;
    if (CreateOCIContainerInterfaceObject() != Core::ERROR_NONE) {
        TEST_LOG("Invalid OCIContainer_Client");
    } else {
        EXPECT_TRUE(m_controller_OCIcontainer != nullptr);
        if (m_controller_OCIcontainer) {
            EXPECT_TRUE(m_OCIContainerplugin != nullptr);
            if (m_OCIContainerplugin) {
                m_OCIContainerplugin->AddRef();
                m_OCIContainerplugin->Register(&notify);

                std::list<std::pair<int32_t, std::string>> containerslist = { { 91, "com.bskyb.epgui" }, { 94, "Netflix" } };
                EXPECT_CALL(*p_dobbyProxyMock, listContainers())
                    .WillOnce(::testing::Return(containerslist));

                EXPECT_CALL(*p_dobbyProxyMock, pauseContainer(91))
                    .WillOnce(::testing::Return(true));

                status = m_OCIContainerplugin->PauseContainer(containerID, success, errorReason);

                // manually trigger the state change event for the container
                // as the mock does not trigger it
                EXPECT_CALL(*p_dobbyProxyMock, listContainers())
                    .WillRepeatedly(::testing::Return(containerslist));
                this->triggerStateChangeEvent(91, containerID, IDobbyProxyEvents::ContainerState::Paused);

                EXPECT_EQ(status, Core::ERROR_NONE);
                EXPECT_TRUE(success);

                signalled = notify.WaitForRequestStatus(EVNT_TIMEOUT, ON_CONTAINER_STATECHANGED);
                EXPECT_TRUE(signalled & ON_CONTAINER_STATECHANGED);

                m_OCIContainerplugin->Unregister(&notify);
                m_OCIContainerplugin->Release();
            } else {
                TEST_LOG("m_OCIContainerplugin is NULL");
            }
            m_controller_OCIcontainer->Release();
        } else {
            TEST_LOG("m_controller_OCIcontainer is NULL");
        }
    }
}

/*
 * Test case to resume a container using COMRPC
 * This test case verifies the ResumeContainer method of the OCIContainer plugin.
 * It checks if the container can be resumed successfully and if the correct status is returned.
 */
TEST_F(OCIContainer_L2Test, ResumeContainer_COMRPC)
{
    uint32_t status = Core::ERROR_GENERAL;
    string containerID = "com.bskyb.epgui";
    string errorReason;
    bool success = false;
    if (CreateOCIContainerInterfaceObject() != Core::ERROR_NONE) {
        TEST_LOG("Invalid OCIContainer_Client");
    } else {
        EXPECT_TRUE(m_controller_OCIcontainer != nullptr);
        if (m_controller_OCIcontainer) {
            EXPECT_TRUE(m_OCIContainerplugin != nullptr);
            if (m_OCIContainerplugin) {
                m_OCIContainerplugin->AddRef();

                std::list<std::pair<int32_t, std::string>> containerslist = { { 91, "com.bskyb.epgui" }, { 94, "Netflix" } };
                EXPECT_CALL(*p_dobbyProxyMock, listContainers())
                    .WillOnce(::testing::Return(containerslist));

                EXPECT_CALL(*p_dobbyProxyMock, resumeContainer(91))
                    .WillOnce(::testing::Return(true));

                status = m_OCIContainerplugin->ResumeContainer(containerID, success, errorReason);
                EXPECT_EQ(status, Core::ERROR_NONE);
                EXPECT_TRUE(success);

                m_OCIContainerplugin->Release();
            } else {
                TEST_LOG("m_OCIContainerplugin is NULL");
            }
            m_controller_OCIcontainer->Release();
        } else {
            TEST_LOG("m_controller_OCIcontainer is NULL");
        }
    }
}

/*
 * Test case to hibernate a container using COMRPC
 * This test case verifies the HibernateContainer method of the OCIContainer plugin.
 * It checks if the container can be hibernated successfully and if the correct status is returned.
 */
TEST_F(OCIContainer_L2Test, HibernateContainer_COMRPC)
{
    uint32_t status = Core::ERROR_GENERAL;
    Core::Sink<OCIContainerNotificationHandler> notify;
    uint32_t signalled = OCICONTAINER_STATUS_INVALID;
    string containerID = "com.bskyb.epgui";
    string errorReason, options;
    bool success = false;
    if (CreateOCIContainerInterfaceObject() != Core::ERROR_NONE) {
        TEST_LOG("Invalid OCIContainer_Client");
    } else {
        EXPECT_TRUE(m_controller_OCIcontainer != nullptr);
        if (m_controller_OCIcontainer) {
            EXPECT_TRUE(m_OCIContainerplugin != nullptr);
            if (m_OCIContainerplugin) {
                m_OCIContainerplugin->AddRef();
                m_OCIContainerplugin->Register(&notify);

                std::list<std::pair<int32_t, std::string>> containerslist = { { 91, "com.bskyb.epgui" }, { 94, "Netflix" } };
                EXPECT_CALL(*p_dobbyProxyMock, listContainers())
                    .WillOnce(::testing::Return(containerslist));

                EXPECT_CALL(*p_dobbyProxyMock, hibernateContainer(91, ::testing::_))
                    .WillOnce(::testing::Return(true));

                status = m_OCIContainerplugin->HibernateContainer(containerID, options, success, errorReason);

                // manually trigger the state change event for the container
                // as the mock does not trigger it
                EXPECT_CALL(*p_dobbyProxyMock, listContainers())
                    .WillRepeatedly(::testing::Return(containerslist));
                this->triggerStateChangeEvent(91, containerID, IDobbyProxyEvents::ContainerState::Hibernating);

                EXPECT_EQ(status, Core::ERROR_NONE);
                EXPECT_TRUE(success);

                signalled = notify.WaitForRequestStatus(EVNT_TIMEOUT, ON_CONTAINER_STATECHANGED);
                EXPECT_TRUE(signalled & ON_CONTAINER_STATECHANGED);

                m_OCIContainerplugin->Unregister(&notify);
                m_OCIContainerplugin->Release();
            } else {
                TEST_LOG("m_OCIContainerplugin is NULL");
            }
            m_controller_OCIcontainer->Release();
        } else {
            TEST_LOG("m_controller_OCIcontainer is NULL");
        }
    }
}

/*
 * Test case to wake up a hibernated container using COMRPC
 * This test case verifies the WakeupContainer method of the OCIContainer plugin.
 * It checks if the container can be woken up successfully and if the correct status is returned.
 */
TEST_F(OCIContainer_L2Test, WakeupContainer_COMRPC)
{
    uint32_t status = Core::ERROR_GENERAL;
    Core::Sink<OCIContainerNotificationHandler> notify;
    uint32_t signalled = OCICONTAINER_STATUS_INVALID;
    string containerID = "com.bskyb.epgui";
    string errorReason;
    bool success = false;
    if (CreateOCIContainerInterfaceObject() != Core::ERROR_NONE) {
        TEST_LOG("Invalid OCIContainer_Client");
    } else {
        EXPECT_TRUE(m_controller_OCIcontainer != nullptr);
        if (m_controller_OCIcontainer) {
            EXPECT_TRUE(m_OCIContainerplugin != nullptr);
            if (m_OCIContainerplugin) {
                m_OCIContainerplugin->AddRef();
                m_OCIContainerplugin->Register(&notify);

                std::list<std::pair<int32_t, std::string>> containerslist = { { 91, "com.bskyb.epgui" }, { 94, "Netflix" } };
                EXPECT_CALL(*p_dobbyProxyMock, listContainers())
                    .WillOnce(::testing::Return(containerslist));

                EXPECT_CALL(*p_dobbyProxyMock, wakeupContainer(91))
                    .WillOnce(::testing::Return(true));

                status = m_OCIContainerplugin->WakeupContainer(containerID, success, errorReason);

                // manually trigger the state change event for the container
                // as the mock does not trigger it
                EXPECT_CALL(*p_dobbyProxyMock, listContainers())
                    .WillRepeatedly(::testing::Return(containerslist));
                this->triggerStateChangeEvent(91, containerID, IDobbyProxyEvents::ContainerState::Awakening);

                EXPECT_EQ(status, Core::ERROR_NONE);
                EXPECT_TRUE(success);

                signalled = notify.WaitForRequestStatus(EVNT_TIMEOUT, ON_CONTAINER_STATECHANGED);
                EXPECT_TRUE(signalled & ON_CONTAINER_STATECHANGED);

                m_OCIContainerplugin->Unregister(&notify);
                m_OCIContainerplugin->Release();
            } else {
                TEST_LOG("m_OCIContainerplugin is NULL");
            }
            m_controller_OCIcontainer->Release();
        } else {
            TEST_LOG("m_controller_OCIcontainer is NULL");
        }
    }
}

/*
 * Test case to execute a command in a container using COMRPC
 * This test case verifies the ExecuteCommand method of the OCIContainer plugin.
 * It checks if the command can be executed successfully in the specified container.
 */
TEST_F(OCIContainer_L2Test, ExecuteCommand_COMRPC)
{
    uint32_t status = Core::ERROR_GENERAL;
    string containerID = "com.bskyb.epgui";
    string errorReason, options, command;
    bool success = false;
    if (CreateOCIContainerInterfaceObject() != Core::ERROR_NONE) {
        TEST_LOG("Invalid OCIContainer_Client");
    } else {
        EXPECT_TRUE(m_controller_OCIcontainer != nullptr);
        if (m_controller_OCIcontainer) {
            EXPECT_TRUE(m_OCIContainerplugin != nullptr);
            if (m_OCIContainerplugin) {
                m_OCIContainerplugin->AddRef();

                std::list<std::pair<int32_t, std::string>> containerslist = { { 91, "com.bskyb.epgui" }, { 94, "Netflix" } };
                EXPECT_CALL(*p_dobbyProxyMock, listContainers())
                    .WillOnce(::testing::Return(containerslist));

                EXPECT_CALL(*p_dobbyProxyMock, execInContainer(91, ::testing::_, ::testing::_))
                    .WillOnce(::testing::Return(true));

                status = m_OCIContainerplugin->ExecuteCommand(containerID, options, command, success, errorReason);
                EXPECT_EQ(status, Core::ERROR_NONE);
                EXPECT_TRUE(success);

                m_OCIContainerplugin->Release();
            } else {
                TEST_LOG("m_OCIContainerplugin is NULL");
            }
            m_controller_OCIcontainer->Release();
        } else {
            TEST_LOG("m_controller_OCIcontainer is NULL");
        }
    }
}

/*
 * Test case to annotate a container using COMRPC
 * This test case verifies the Annotate method of the OCIContainer plugin.
 * It checks if the annotation can be added successfully to the specified container.
 */
TEST_F(OCIContainer_L2Test, Annotate_COMRPC)
{
    uint32_t status = Core::ERROR_GENERAL;
    string containerID = "com.bskyb.epgui";
    string errorReason, key, value;
    bool success = false;
    if (CreateOCIContainerInterfaceObject() != Core::ERROR_NONE) {
        TEST_LOG("Invalid OCIContainer_Client");
    } else {
        EXPECT_TRUE(m_controller_OCIcontainer != nullptr);
        if (m_controller_OCIcontainer) {
            EXPECT_TRUE(m_OCIContainerplugin != nullptr);
            if (m_OCIContainerplugin) {
                m_OCIContainerplugin->AddRef();

                std::list<std::pair<int32_t, std::string>> containerslist = { { 91, "com.bskyb.epgui" }, { 94, "Netflix" } };
                EXPECT_CALL(*p_dobbyProxyMock, listContainers())
                    .WillOnce(::testing::Return(containerslist));

                EXPECT_CALL(*p_dobbyProxyMock, addAnnotation(91, ::testing::_, ::testing::_))
                    .WillOnce(::testing::Return(true));

                status = m_OCIContainerplugin->Annotate(containerID, key, value, success, errorReason);
                EXPECT_EQ(status, Core::ERROR_NONE);
                EXPECT_TRUE(success);

                m_OCIContainerplugin->Release();
            } else {
                TEST_LOG("m_OCIContainerplugin is NULL");
            }
            m_controller_OCIcontainer->Release();
        } else {
            TEST_LOG("m_controller_OCIcontainer is NULL");
        }
    }
}

/*
 * Test case to remove an annotation from a container using COMRPC
 * This test case verifies the RemoveAnnotation method of the OCIContainer plugin.
 * It checks if the annotation can be removed successfully from the specified container.
 */
TEST_F(OCIContainer_L2Test, RemoveAnnotation_COMRPC)
{
    uint32_t status = Core::ERROR_GENERAL;
    string containerID = "com.bskyb.epgui";
    string errorReason, key;
    bool success = false;
    if (CreateOCIContainerInterfaceObject() != Core::ERROR_NONE) {
        TEST_LOG("Invalid OCIContainer_Client");
    } else {
        EXPECT_TRUE(m_controller_OCIcontainer != nullptr);
        if (m_controller_OCIcontainer) {
            EXPECT_TRUE(m_OCIContainerplugin != nullptr);
            if (m_OCIContainerplugin) {
                m_OCIContainerplugin->AddRef();

                std::list<std::pair<int32_t, std::string>> containerslist = { { 91, "com.bskyb.epgui" }, { 94, "Netflix" } };
                EXPECT_CALL(*p_dobbyProxyMock, listContainers())
                    .WillOnce(::testing::Return(containerslist));

                EXPECT_CALL(*p_dobbyProxyMock, removeAnnotation(91, ::testing::_))
                    .WillOnce(::testing::Return(true));

                status = m_OCIContainerplugin->RemoveAnnotation(containerID, key, success, errorReason);
                EXPECT_EQ(status, Core::ERROR_NONE);
                EXPECT_TRUE(success);

                m_OCIContainerplugin->Release();
            } else {
                TEST_LOG("m_OCIContainerplugin is NULL");
            }
            m_controller_OCIcontainer->Release();
        } else {
            TEST_LOG("m_controller_OCIcontainer is NULL");
        }
    }
}

/*
 * Test case to mount a source to a target in a container using COMRPC
 * This test case verifies the Mount method of the OCIContainer plugin.
 * It checks if the mount operation can be performed successfully and if the correct status is returned.
 */
TEST_F(OCIContainer_L2Test, Mount_COMRPC)
{
    uint32_t status = Core::ERROR_GENERAL;
    string containerID = "com.bskyb.epgui";
    string errorReason, source, target, type, options;
    bool success = false;
    if (CreateOCIContainerInterfaceObject() != Core::ERROR_NONE) {
        TEST_LOG("Invalid OCIContainer_Client");
    } else {
        EXPECT_TRUE(m_controller_OCIcontainer != nullptr);
        if (m_controller_OCIcontainer) {
            EXPECT_TRUE(m_OCIContainerplugin != nullptr);
            if (m_OCIContainerplugin) {
                m_OCIContainerplugin->AddRef();

                std::list<std::pair<int32_t, std::string>> containerslist = { { 91, "com.bskyb.epgui" }, { 94, "Netflix" } };
                EXPECT_CALL(*p_dobbyProxyMock, listContainers())
                    .WillOnce(::testing::Return(containerslist));

                EXPECT_CALL(*p_dobbyProxyMock, addContainerMount(91, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
                    .WillOnce(::testing::Return(true));

                status = m_OCIContainerplugin->Mount(containerID, source, target, type, options, success, errorReason);
                EXPECT_EQ(status, Core::ERROR_NONE);
                EXPECT_TRUE(success);

                m_OCIContainerplugin->Release();
            } else {
                TEST_LOG("m_OCIContainerplugin is NULL");
            }
            m_controller_OCIcontainer->Release();
        } else {
            TEST_LOG("m_controller_OCIcontainer is NULL");
        }
    }
}

/*
 * Test case to unmount a target from a container using COMRPC
 * This test case verifies the Unmount method of the OCIContainer plugin.
 * It checks if the unmount operation can be performed successfully and if the correct status is returned.
 */
TEST_F(OCIContainer_L2Test, Unmount_COMRPC)
{
    uint32_t status = Core::ERROR_GENERAL;
    string containerID = "com.bskyb.epgui";
    string errorReason, target;
    bool success = false;
    if (CreateOCIContainerInterfaceObject() != Core::ERROR_NONE) {
        TEST_LOG("Invalid OCIContainer_Client");
    } else {
        EXPECT_TRUE(m_controller_OCIcontainer != nullptr);
        if (m_controller_OCIcontainer) {
            EXPECT_TRUE(m_OCIContainerplugin != nullptr);
            if (m_OCIContainerplugin) {
                m_OCIContainerplugin->AddRef();

                std::list<std::pair<int32_t, std::string>> containerslist = { { 91, "com.bskyb.epgui" }, { 94, "Netflix" } };
                EXPECT_CALL(*p_dobbyProxyMock, listContainers())
                    .WillOnce(::testing::Return(containerslist));

                EXPECT_CALL(*p_dobbyProxyMock, removeContainerMount(91, ::testing::_))
                    .WillOnce(::testing::Return(true));

                status = m_OCIContainerplugin->Unmount(containerID, target, success, errorReason);
                EXPECT_EQ(status, Core::ERROR_NONE);
                EXPECT_TRUE(success);

                m_OCIContainerplugin->Release();
            } else {
                TEST_LOG("m_OCIContainerplugin is NULL");
            }
            m_controller_OCIcontainer->Release();
        } else {
            TEST_LOG("m_controller_OCIcontainer is NULL");
        }
    }
}

/*
 * Test case to get container information using JSONRPC
 * This test case verifies the getContainerInfo method of the OCIContainer plugin.
 * It checks if the container information can be retrieved successfully and if the correct data is returned.
 */
TEST_F(OCIContainer_L2Test, getcontainerInfo_JSONRPC)
{
    JSONRPC::LinkType<Core::JSON::IElement> jsonrpc(OCICONTAINER_CALLSIGN, OCICONTAINERTEST_CALLSIGN);
    uint32_t status = Core::ERROR_GENERAL;

    std::list<std::pair<int32_t, std::string>> containerslist = { { 91, "com.bskyb.epgui" }, { 94, "Netflix" } };
    EXPECT_CALL(*p_dobbyProxyMock, listContainers())
        .WillOnce(::testing::Return(containerslist));

    EXPECT_CALL(*p_dobbyProxyMock, getContainerInfo(91))
        .WillOnce(::testing::Return("{\"cpu\":{\"usage\" :{\"percpu\" :[3661526845,3773518079,4484546066,4700379608],"
                                    "\"total\" : 16619970598}},\"gpu\":{\"memory\" :{\"failcnt\" : 0,\"limit\" : 209715200,\"max\" : 3911680,\"usage\" : 0}},"
                                    "\"id\":\"Cobalt-0\",\"ion\":{\"heaps\" :{\"ion.\" :{\"failcnt\" : null,\"limit\" : null,\"max\" : null,\"usage\" : null}}},"
                                    "\"memory\":{\"user\" :{\"failcnt\" : 0,\"limit\" : 419430400,\"max\" : 73375744,\"usage\" : 53297152}},\"pids\":[13132,13418],"
                                    "\"processes\":[{\"cmdline\" : \"/usr/libexec/DobbyInit /usr/bin/WPEProcess -l libWPEFrameworkCobaltImpl.so -c"
                                    " CobaltImplementation -C Cobalt-0 -r /tmp/communicator -i 64 -x 48 -p \"/opt/persistent/rdkservices/Cobalt-0/\" -s"
                                    " \"/usr/lib/wpeframework/plugins/\" -d \"/usr/share/WPEFramework/Cobalt/\" -a \"/usr/bin/\" -v \"/tmp/Cobalt-0/\" -m"
                                    " \"/usr/lib/wpeframework/proxystubs/\" -P \"/opt/minidumps/\" \",\"executable\" : \"/usr/libexec/DobbyInit\",\"nsPid\" : 1,"
                                    "\"pid\" : 13132},{\"cmdline\" : \"WPEProcess -l libWPEFrameworkCobaltImpl.so -c CobaltImplementation -C Cobalt-0 -r /tmp/communicator"
                                    " -i 64 -x 48 -p \"/opt/persistent/rdkservices/Cobalt-0/\" -s \"/usr/lib/wpeframework/plugins/\" -d \"/usr/share/WPEFramework/Cobalt/\""
                                    " -a \"/usr/bin/\" -v \"/tmp/Cobalt-0/\" -m \"/usr/lib/wpeframework/proxystubs/\" -P \"/opt/minidumps/\" \",\"executable\" : "
                                    "\"/usr/bin/WPEProcess\",\"nsPid\" : 6,\"pid\" : 13418}],\"state\":\"running\",\"timestamp\":1298657054196}"));

    JsonObject params, result;
    params["containerId"] = "com.bskyb.epgui";

    status = InvokeServiceMethod("org.rdk.OCIContainer", "getContainerInfo", params, result);
    EXPECT_EQ(status, Core::ERROR_NONE);
    EXPECT_TRUE(result["success"].Boolean());
}

/*
 * Test case to start a container using JSONRPC
 * This test case verifies the startContainer method of the OCIContainer plugin.
 * It checks if the container can be started successfully and if the correct status is returned.
 */
TEST_F(OCIContainer_L2Test, Startcontainer_JSONRPC)
{
    JSONRPC::LinkType<Core::JSON::IElement> jsonrpc(OCICONTAINER_CALLSIGN, OCICONTAINERTEST_CALLSIGN);
    StrictMock<AsyncHandlerMock_OCIContainer> async_handler;
    uint32_t signalled = OCICONTAINER_STATUS_INVALID;
    uint32_t status = Core::ERROR_GENERAL;
    std::string message;
    JsonObject expected_status;

    EXPECT_CALL(*p_dobbyProxyMock, startContainerFromBundle(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(91));

    status = jsonrpc.Subscribe<JsonObject>(EVNT_TIMEOUT,
        _T("onContainerStateChanged"),
        &AsyncHandlerMock_OCIContainer::onContainerStateChanged,
        &async_handler);
    EXPECT_EQ(Core::ERROR_NONE, status);

    message = "{\"containerId\":\"com.bskyb.epgui\",\"state\":\"STARTING\"}";
    expected_status.FromString(message);
    EXPECT_CALL(async_handler, onContainerStateChanged(MatchRequestStatus(expected_status)))
        .WillOnce(Invoke(this, &OCIContainer_L2Test::onContainerStateChanged));

    JsonObject params, result;
    params["containerId"] = "com.bskyb.epgui";
    params["bundlePath"] = "/containers/myBundle";
    params["command"] = "command";
    params["westerosSocket"] = "/usr/mySocket";

    status = InvokeServiceMethod("org.rdk.OCIContainer", "startContainer", params, result);
    EXPECT_EQ(status, Core::ERROR_NONE);
    EXPECT_TRUE(result["success"].Boolean());

    // manually trigger the state change event for the container
    // as the mock does not trigger it
    std::list<std::pair<int32_t, std::string>> containerslist = { { 91, "com.bskyb.epgui" }, { 94, "Netflix" } };
    EXPECT_CALL(*p_dobbyProxyMock, listContainers())
        .WillRepeatedly(::testing::Return(containerslist));
    this->triggerStateChangeEvent(91, "com.bskyb.epgui", IDobbyProxyEvents::ContainerState::Starting);

    signalled = WaitForRequestStatus(EVNT_TIMEOUT, ON_CONTAINER_STATECHANGED);
    EXPECT_TRUE(signalled & ON_CONTAINER_STATECHANGED);
    jsonrpc.Unsubscribe(EVNT_TIMEOUT, _T("onContainerStateChanged"));
}

/*
 * Test case to start a container from a Dobby spec using JSONRPC
 * This test case verifies the startContainerFromDobbySpec method of the OCIContainer plugin.
 * It checks if the container can be started successfully and if the correct status is returned.
 */
TEST_F(OCIContainer_L2Test, StartcontainerFromDobbySpec_JSONRPC)
{
    JSONRPC::LinkType<Core::JSON::IElement> jsonrpc(OCICONTAINER_CALLSIGN, OCICONTAINERTEST_CALLSIGN);
    uint32_t status = Core::ERROR_GENERAL;
    EXPECT_CALL(*p_dobbyProxyMock, startContainerFromSpec(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(91));

    JsonObject params, result;
    params["containerId"] = "com.bskyb.epgui";
    params["dobbySpec"] = "/containers/dobbySpec";
    params["command"] = "command";
    params["westerosSocket"] = "/usr/mySocket";

    status = InvokeServiceMethod("org.rdk.OCIContainer", "startContainerFromDobbySpec", params, result);
    EXPECT_EQ(status, Core::ERROR_NONE);
    EXPECT_TRUE(result["success"].Boolean());
}

/*
 * Test case to stop a container using JSONRPC
 * This test case verifies the stopContainer method of the OCIContainer plugin.
 * It checks if the container can be stopped successfully and if the correct status is returned.
 */
TEST_F(OCIContainer_L2Test, StopContainer_JSONRPC)
{
    JSONRPC::LinkType<Core::JSON::IElement> jsonrpc(OCICONTAINER_CALLSIGN, OCICONTAINERTEST_CALLSIGN);
    StrictMock<AsyncHandlerMock_OCIContainer> async_handler;
    uint32_t signalled = OCICONTAINER_STATUS_INVALID;
    uint32_t status = Core::ERROR_GENERAL;
    std::string message;
    JsonObject expected_status;

    status = jsonrpc.Subscribe<JsonObject>(EVNT_TIMEOUT,
        _T("onContainerStateChanged"),
        &AsyncHandlerMock_OCIContainer::onContainerStateChanged,
        &async_handler);
    EXPECT_EQ(Core::ERROR_NONE, status);

    message = "{\"containerId\":\"com.bskyb.epgui\",\"state\":\"STOPPING\"}";
    expected_status.FromString(message);
    EXPECT_CALL(async_handler, onContainerStateChanged(MatchRequestStatus(expected_status)))
        .WillOnce(Invoke(this, &OCIContainer_L2Test::onContainerStateChanged));

    std::list<std::pair<int32_t, std::string>> containerslist = { { 91, "com.bskyb.epgui" }, { 94, "Netflix" } };
    EXPECT_CALL(*p_dobbyProxyMock, listContainers())
        .WillOnce(::testing::Return(containerslist));

    EXPECT_CALL(*p_dobbyProxyMock, stopContainer(91, ::testing::_))
        .WillOnce(::testing::Return(true));

    JsonObject params, result;
    params["containerId"] = "com.bskyb.epgui";
    params["force"] = true;

    status = InvokeServiceMethod("org.rdk.OCIContainer", "stopContainer", params, result);
    EXPECT_EQ(status, Core::ERROR_NONE);
    EXPECT_TRUE(result["success"].Boolean());

    // manually trigger the state change event for the container
    // as the mock does not trigger it
    EXPECT_CALL(*p_dobbyProxyMock, listContainers())
        .WillRepeatedly(::testing::Return(containerslist));
    this->triggerStateChangeEvent(91, "com.bskyb.epgui", IDobbyProxyEvents::ContainerState::Stopping);

    signalled = WaitForRequestStatus(EVNT_TIMEOUT, ON_CONTAINER_STATECHANGED);
    EXPECT_TRUE(signalled & ON_CONTAINER_STATECHANGED);
    jsonrpc.Unsubscribe(EVNT_TIMEOUT, _T("onContainerStateChanged"));
}

/*
 * Test case to pause a container using JSONRPC
 * This test case verifies the pauseContainer method of the OCIContainer plugin.
 * It checks if the container can be paused successfully and if the correct status is returned.
 */
TEST_F(OCIContainer_L2Test, PauseContainer_JSONRPC)
{
    JSONRPC::LinkType<Core::JSON::IElement> jsonrpc(OCICONTAINER_CALLSIGN, OCICONTAINERTEST_CALLSIGN);
    StrictMock<AsyncHandlerMock_OCIContainer> async_handler;
    uint32_t signalled = OCICONTAINER_STATUS_INVALID;
    uint32_t status = Core::ERROR_GENERAL;
    std::string message;
    JsonObject expected_status;

    status = jsonrpc.Subscribe<JsonObject>(EVNT_TIMEOUT,
        _T("onContainerStateChanged"),
        &AsyncHandlerMock_OCIContainer::onContainerStateChanged,
        &async_handler);
    EXPECT_EQ(Core::ERROR_NONE, status);

    message = "{\"containerId\":\"com.bskyb.epgui\",\"state\":\"PAUSED\"}";
    expected_status.FromString(message);
    EXPECT_CALL(async_handler, onContainerStateChanged(MatchRequestStatus(expected_status)))
        .WillOnce(Invoke(this, &OCIContainer_L2Test::onContainerStateChanged));

    std::list<std::pair<int32_t, std::string>> containerslist = { { 91, "com.bskyb.epgui" }, { 94, "Netflix" } };
    EXPECT_CALL(*p_dobbyProxyMock, listContainers())
        .WillOnce(::testing::Return(containerslist));

    EXPECT_CALL(*p_dobbyProxyMock, pauseContainer(91))
        .WillOnce(::testing::Return(true));

    JsonObject params, result;
    params["containerId"] = "com.bskyb.epgui";

    status = InvokeServiceMethod("org.rdk.OCIContainer", "pauseContainer", params, result);
    EXPECT_EQ(status, Core::ERROR_NONE);
    EXPECT_TRUE(result["success"].Boolean());

    // manually trigger the state change event for the container
    // as the mock does not trigger it
    EXPECT_CALL(*p_dobbyProxyMock, listContainers())
        .WillRepeatedly(::testing::Return(containerslist));
    this->triggerStateChangeEvent(91, "com.bskyb.epgui", IDobbyProxyEvents::ContainerState::Paused);

    signalled = WaitForRequestStatus(EVNT_TIMEOUT, ON_CONTAINER_STATECHANGED);
    EXPECT_TRUE(signalled & ON_CONTAINER_STATECHANGED);
    jsonrpc.Unsubscribe(EVNT_TIMEOUT, _T("onContainerStateChanged"));
}

/*
 * Test case to resume a container using JSONRPC
 * This test case verifies the resumeContainer method of the OCIContainer plugin.
 * It checks if the container can be resumed successfully and if the correct status is returned.
 */
TEST_F(OCIContainer_L2Test, ResumeContainer_JSONRPC)
{
    JSONRPC::LinkType<Core::JSON::IElement> jsonrpc(OCICONTAINER_CALLSIGN, OCICONTAINERTEST_CALLSIGN);
    uint32_t status = Core::ERROR_GENERAL;

    std::list<std::pair<int32_t, std::string>> containerslist = { { 91, "com.bskyb.epgui" }, { 94, "Netflix" } };
    EXPECT_CALL(*p_dobbyProxyMock, listContainers())
        .WillOnce(::testing::Return(containerslist));

    EXPECT_CALL(*p_dobbyProxyMock, resumeContainer(91))
        .WillOnce(::testing::Return(true));

    JsonObject params, result;
    params["containerId"] = "com.bskyb.epgui";

    status = InvokeServiceMethod("org.rdk.OCIContainer", "resumeContainer", params, result);
    EXPECT_EQ(status, Core::ERROR_NONE);
    EXPECT_TRUE(result["success"].Boolean());
}

/*
 * Test case to hibernate a container using JSONRPC
 * This test case verifies the hibernateContainer method of the OCIContainer plugin.
 * It checks if the container can be hibernated successfully and if the correct status is returned.
 */
TEST_F(OCIContainer_L2Test, HibernateContainer_JSONRPC)
{
    JSONRPC::LinkType<Core::JSON::IElement> jsonrpc(OCICONTAINER_CALLSIGN, OCICONTAINERTEST_CALLSIGN);
    StrictMock<AsyncHandlerMock_OCIContainer> async_handler;
    uint32_t signalled = OCICONTAINER_STATUS_INVALID;
    uint32_t status = Core::ERROR_GENERAL;
    std::string message;
    JsonObject expected_status;

    status = jsonrpc.Subscribe<JsonObject>(EVNT_TIMEOUT,
        _T("onContainerStateChanged"),
        &AsyncHandlerMock_OCIContainer::onContainerStateChanged,
        &async_handler);
    EXPECT_EQ(Core::ERROR_NONE, status);

    message = "{\"containerId\":\"com.bskyb.epgui\",\"state\":\"HIBERNATING\"}";
    expected_status.FromString(message);
    EXPECT_CALL(async_handler, onContainerStateChanged(MatchRequestStatus(expected_status)))
        .WillOnce(Invoke(this, &OCIContainer_L2Test::onContainerStateChanged));

    std::list<std::pair<int32_t, std::string>> containerslist = { { 91, "com.bskyb.epgui" }, { 94, "Netflix" } };
    EXPECT_CALL(*p_dobbyProxyMock, listContainers())
        .WillOnce(::testing::Return(containerslist));

    EXPECT_CALL(*p_dobbyProxyMock, hibernateContainer(91, ::testing::_))
        .WillOnce(::testing::Return(true));

    JsonObject params, result;
    params["containerId"] = "com.bskyb.epgui";
    params["options"] = "";

    status = InvokeServiceMethod("org.rdk.OCIContainer", "hibernateContainer", params, result);
    EXPECT_EQ(status, Core::ERROR_NONE);
    EXPECT_TRUE(result["success"].Boolean());

    // manually trigger the state change event for the container
    // as the mock does not trigger it
    EXPECT_CALL(*p_dobbyProxyMock, listContainers())
        .WillRepeatedly(::testing::Return(containerslist));
    this->triggerStateChangeEvent(91, "com.bskyb.epgui", IDobbyProxyEvents::ContainerState::Hibernating);

    signalled = WaitForRequestStatus(EVNT_TIMEOUT, ON_CONTAINER_STATECHANGED);
    EXPECT_TRUE(signalled & ON_CONTAINER_STATECHANGED);
    jsonrpc.Unsubscribe(EVNT_TIMEOUT, _T("onContainerStateChanged"));
}

/*
 * Test case to wake up a hibernated container using JSONRPC
 * This test case verifies the wakeupContainer method of the OCIContainer plugin.
 * It checks if the container can be woken up successfully and if the correct status is returned.
 */
TEST_F(OCIContainer_L2Test, WakeupContainer_JSONRPC)
{
    JSONRPC::LinkType<Core::JSON::IElement> jsonrpc(OCICONTAINER_CALLSIGN, OCICONTAINERTEST_CALLSIGN);
    uint32_t status = Core::ERROR_GENERAL;

    std::list<std::pair<int32_t, std::string>> containerslist = { { 91, "com.bskyb.epgui" }, { 94, "Netflix" } };
    EXPECT_CALL(*p_dobbyProxyMock, listContainers())
        .WillOnce(::testing::Return(containerslist));

    EXPECT_CALL(*p_dobbyProxyMock, wakeupContainer(91))
        .WillOnce(::testing::Return(true));

    JsonObject params, result;
    params["containerId"] = "com.bskyb.epgui";

    status = InvokeServiceMethod("org.rdk.OCIContainer", "wakeupContainer", params, result);
    EXPECT_EQ(status, Core::ERROR_NONE);
    EXPECT_TRUE(result["success"].Boolean());
}

/*
 * Test case to execute a command in a container using JSONRPC
 * This test case verifies the executeCommand method of the OCIContainer plugin.
 * It checks if the command can be executed successfully in the specified container.
 */
TEST_F(OCIContainer_L2Test, ExecuteCommand_JSONRPC)
{
    JSONRPC::LinkType<Core::JSON::IElement> jsonrpc(OCICONTAINER_CALLSIGN, OCICONTAINERTEST_CALLSIGN);
    uint32_t status = Core::ERROR_GENERAL;

    std::list<std::pair<int32_t, std::string>> containerslist = { { 91, "com.bskyb.epgui" }, { 94, "Netflix" } };
    EXPECT_CALL(*p_dobbyProxyMock, listContainers())
        .WillOnce(::testing::Return(containerslist));

    EXPECT_CALL(*p_dobbyProxyMock, execInContainer(91, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(true));

    JsonObject params, result;
    params["containerId"] = "com.bskyb.epgui";
    params["options"] = "";
    params["command"] = "command";

    status = InvokeServiceMethod("org.rdk.OCIContainer", "executeCommand", params, result);
    EXPECT_EQ(status, Core::ERROR_NONE);
    EXPECT_TRUE(result["success"].Boolean());
}

/*
 * Test case to annotate a container using JSONRPC
 * This test case verifies the annotate method of the OCIContainer plugin.
 * It checks if the annotation can be added successfully to the specified container.
 */
TEST_F(OCIContainer_L2Test, Annotate_JSONRPC)
{
    JSONRPC::LinkType<Core::JSON::IElement> jsonrpc(OCICONTAINER_CALLSIGN, OCICONTAINERTEST_CALLSIGN);
    uint32_t status = Core::ERROR_GENERAL;

    std::list<std::pair<int32_t, std::string>> containerslist = { { 91, "com.bskyb.epgui" }, { 94, "Netflix" } };
    EXPECT_CALL(*p_dobbyProxyMock, listContainers())
        .WillOnce(::testing::Return(containerslist));

    EXPECT_CALL(*p_dobbyProxyMock, addAnnotation(91, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(true));

    JsonObject params, result;
    params["containerId"] = "com.bskyb.epgui";
    params["key"] = "key";
    params["value"] = "value";

    status = InvokeServiceMethod("org.rdk.OCIContainer", "annotate", params, result);
    EXPECT_EQ(status, Core::ERROR_NONE);
    EXPECT_TRUE(result["success"].Boolean());
}

/*
 * Test case to remove an annotation from a container using JSONRPC
 * This test case verifies the removeAnnotation method of the OCIContainer plugin.
 * It checks if the annotation can be removed successfully from the specified container.
 */
TEST_F(OCIContainer_L2Test, RemoveAnnotation_JSONRPC)
{
    JSONRPC::LinkType<Core::JSON::IElement> jsonrpc(OCICONTAINER_CALLSIGN, OCICONTAINERTEST_CALLSIGN);
    uint32_t status = Core::ERROR_GENERAL;

    std::list<std::pair<int32_t, std::string>> containerslist = { { 91, "com.bskyb.epgui" }, { 94, "Netflix" } };
    EXPECT_CALL(*p_dobbyProxyMock, listContainers())
        .WillOnce(::testing::Return(containerslist));

    EXPECT_CALL(*p_dobbyProxyMock, removeAnnotation(91, ::testing::_))
        .WillOnce(::testing::Return(true));

    JsonObject params, result;
    params["containerId"] = "com.bskyb.epgui";
    params["key"] = "key";

    status = InvokeServiceMethod("org.rdk.OCIContainer", "removeAnnotation", params, result);
    EXPECT_EQ(status, Core::ERROR_NONE);
    EXPECT_TRUE(result["success"].Boolean());
}

/*
 * Test case to mount a source to a target in a container using JSONRPC
 * This test case verifies the mount method of the OCIContainer plugin.
 * It checks if the mount operation can be performed successfully and if the correct status is returned.
 */
TEST_F(OCIContainer_L2Test, Mount_JSONRPC)
{
    JSONRPC::LinkType<Core::JSON::IElement> jsonrpc(OCICONTAINER_CALLSIGN, OCICONTAINERTEST_CALLSIGN);
    uint32_t status = Core::ERROR_GENERAL;

    std::list<std::pair<int32_t, std::string>> containerslist = { { 91, "com.bskyb.epgui" }, { 94, "Netflix" } };
    EXPECT_CALL(*p_dobbyProxyMock, listContainers())
        .WillOnce(::testing::Return(containerslist));

    EXPECT_CALL(*p_dobbyProxyMock, addContainerMount(91, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Return(true));

    JsonObject params, result;
    params["containerId"] = "com.bskyb.epgui";
    params["source"] = "source";
    params["target"] = "target";
    params["type"] = "type";
    params["options"] = "options";

    status = InvokeServiceMethod("org.rdk.OCIContainer", "mount", params, result);
    EXPECT_EQ(status, Core::ERROR_NONE);
    EXPECT_TRUE(result["success"].Boolean());
}

/*
 * Test case to unmount a target from a container using JSONRPC
 * This test case verifies the unmount method of the OCIContainer plugin.
 * It checks if the unmount operation can be performed successfully and if the correct status is returned.
 */
TEST_F(OCIContainer_L2Test, Unmount_JSONRPC)
{
    JSONRPC::LinkType<Core::JSON::IElement> jsonrpc(OCICONTAINER_CALLSIGN, OCICONTAINERTEST_CALLSIGN);
    uint32_t status = Core::ERROR_GENERAL;

    std::list<std::pair<int32_t, std::string>> containerslist = { { 91, "com.bskyb.epgui" }, { 94, "Netflix" } };
    EXPECT_CALL(*p_dobbyProxyMock, listContainers())
        .WillOnce(::testing::Return(containerslist));

    EXPECT_CALL(*p_dobbyProxyMock, removeContainerMount(91, ::testing::_))
        .WillOnce(::testing::Return(true));

    JsonObject params, result;
    params["containerId"] = "com.bskyb.epgui";
    params["target"] = "target";

    status = InvokeServiceMethod("org.rdk.OCIContainer", "unmount", params, result);
    EXPECT_EQ(status, Core::ERROR_NONE);
    EXPECT_TRUE(result["success"].Boolean());
}

/*
 * Test case to list all containers using JSONRPC
 * This test case verifies the listContainers method of the OCIContainer plugin.
 * It checks if the list of containers can be retrieved successfully and if the correct status is returned.
 */
TEST_F(OCIContainer_L2Test, ListContainers_JSONRPC)
{
    JSONRPC::LinkType<Core::JSON::IElement> jsonrpc(OCICONTAINER_CALLSIGN, OCICONTAINERTEST_CALLSIGN);
    uint32_t status = Core::ERROR_GENERAL;

    std::list<std::pair<int32_t, std::string>> containerslist = { { 91, "com.bskyb.epgui" }, { 94, "Netflix" } };
    EXPECT_CALL(*p_dobbyProxyMock, listContainers())
        .WillOnce(::testing::Return(containerslist));

    JsonObject params, result;

    status = InvokeServiceMethod("org.rdk.OCIContainer", "listContainers", params, result);
    EXPECT_EQ(status, Core::ERROR_NONE);
    EXPECT_TRUE(result["success"].Boolean());
}

/*
 * Test case to get the status of a container using JSONRPC
 * This test case verifies the getContainerStatus method of the OCIContainer plugin.
 * It checks if the status of the specified container can be retrieved successfully and if the correct status is returned.
 */
TEST_F(OCIContainer_L2Test, ContainerStopped_EventTest)
{
    Core::Sink<OCIContainerNotificationHandler> notify;
    string containerID = "com.bskyb.epgui";
    uint32_t signalled = OCICONTAINER_STATUS_INVALID;
    int32_t descriptor = 91;
    if (CreateOCIContainerInterfaceObject() != Core::ERROR_NONE) {
        TEST_LOG("Invalid OCIContainer_Client");
    } else {
        EXPECT_TRUE(m_controller_OCIcontainer != nullptr);
        if (m_controller_OCIcontainer) {
            EXPECT_TRUE(m_OCIContainerplugin != nullptr);
            if (m_OCIContainerplugin) {
                m_OCIContainerplugin->AddRef();
                m_OCIContainerplugin->Register(&notify);

                EXPECT_CALL(*p_mockOmiProxy, umountCryptedBundle(::testing::_))
                    .WillOnce(::testing::Return(true));

                std::list<std::pair<int32_t, std::string>> containerslist = { { 91, "com.bskyb.epgui" }, { 94, "Netflix" } };
                // manually trigger the state change event for the container
                // as the mock does not trigger it
                EXPECT_CALL(*p_dobbyProxyMock, listContainers())
                    .WillRepeatedly(::testing::Return(containerslist));
                this->triggerStateChangeEvent(descriptor, containerID, IDobbyProxyEvents::ContainerState::Stopped);

                signalled = notify.WaitForRequestStatus(EVNT_TIMEOUT, ON_CONTAINER_STOPPED);
                EXPECT_TRUE(signalled & ON_CONTAINER_STOPPED);

                m_OCIContainerplugin->Unregister(&notify);
                m_OCIContainerplugin->Release();
            } else {
                TEST_LOG("m_OCIContainerplugin is NULL");
            }
            m_controller_OCIcontainer->Release();
        } else {
            TEST_LOG("m_controller_OCIcontainer is NULL");
        }
    }
}

/*
 * Test case to verify the ON_CONTAINER_STARTED event
 * This test case checks if the ON_CONTAINER_STARTED event is triggered correctly when a container starts.
 * It uses a mock to simulate the container state change and verifies the event notification.
 */
TEST_F(OCIContainer_L2Test, ContainerStarted_EventTest)
{
    Core::Sink<OCIContainerNotificationHandler> notify;
    string containerID = "com.bskyb.epgui";
    int32_t descriptor = 91;
    uint32_t signalled = OCICONTAINER_STATUS_INVALID;
    if (CreateOCIContainerInterfaceObject() != Core::ERROR_NONE) {
        TEST_LOG("Invalid OCIContainer_Client");
    } else {
        EXPECT_TRUE(m_controller_OCIcontainer != nullptr);
        if (m_controller_OCIcontainer) {
            EXPECT_TRUE(m_OCIContainerplugin != nullptr);
            if (m_OCIContainerplugin) {
                m_OCIContainerplugin->AddRef();
                m_OCIContainerplugin->Register(&notify);

                std::list<std::pair<int32_t, std::string>> containerslist = { { 91, "com.bskyb.epgui" }, { 94, "Netflix" } };
                // manually trigger the state change event for the container
                // as the mock does not trigger it
                EXPECT_CALL(*p_dobbyProxyMock, listContainers())
                    .WillRepeatedly(::testing::Return(containerslist));
                this->triggerStateChangeEvent(descriptor, containerID, IDobbyProxyEvents::ContainerState::Running);

                signalled = notify.WaitForRequestStatus(EVNT_TIMEOUT, ON_CONTAINER_STARTED);
                EXPECT_TRUE(signalled & ON_CONTAINER_STARTED);

                m_OCIContainerplugin->Unregister(&notify);
                m_OCIContainerplugin->Release();
            } else {
                TEST_LOG("m_OCIContainerplugin is NULL");
            }
            m_controller_OCIcontainer->Release();
        } else {
            TEST_LOG("m_controller_OCIcontainer is NULL");
        }
    }
}

/*
 * Test case to verify the ON_CONTAINER_FAILED event
 * This test case checks if the ON_CONTAINER_FAILED event is triggered correctly when a container fails.
 * It uses a mock to simulate the container failure and verifies the event notification.
 */
TEST_F(OCIContainer_L2Test, ContainerFailed_EventTest)
{
    Core::Sink<OCIContainerNotificationHandler> notify;
    string containerID = "com.bskyb.epgui";
    uint32_t signalled = OCICONTAINER_STATUS_INVALID;
    if (CreateOCIContainerInterfaceObject() != Core::ERROR_NONE) {
        TEST_LOG("Invalid OCIContainer_Client");
    } else {
        EXPECT_TRUE(m_controller_OCIcontainer != nullptr);
        if (m_controller_OCIcontainer) {
            EXPECT_TRUE(m_OCIContainerplugin != nullptr);
            if (m_OCIContainerplugin) {
                m_OCIContainerplugin->AddRef();
                m_OCIContainerplugin->Register(&notify);

                EXPECT_CALL(*p_dobbyProxyMock, stopContainer(91, ::testing::_))
                    .WillOnce(::testing::Return(true));

                std::list<std::pair<int32_t, std::string>> containerslist = { { 91, "com.bskyb.epgui" }, { 94, "Netflix" } };
                EXPECT_CALL(*p_dobbyProxyMock, listContainers())
                    .WillRepeatedly(::testing::Return(containerslist));
                // manually trigger the state change event for the container
                // as the mock does not trigger it
                this->triggerOmiErrorEvent(containerID, omi::IOmiProxy::ErrorType::verityFailed);

                signalled = notify.WaitForRequestStatus(EVNT_TIMEOUT, ON_CONTAINER_FAILED);
                EXPECT_TRUE(signalled & ON_CONTAINER_FAILED);

                m_OCIContainerplugin->Unregister(&notify);
                m_OCIContainerplugin->Release();
            } else {
                TEST_LOG("m_OCIContainerplugin is NULL");
            }
            m_controller_OCIcontainer->Release();
        } else {
            TEST_LOG("m_controller_OCIcontainer is NULL");
        }
    }
}

/*
 * Test case to verify the ON_CONTAINER_STOPPED event
 * This test case checks if the ON_CONTAINER_STOPPED event is triggered correctly when a container stops.
 * It uses a mock to simulate the container state change and verifies the event notification.
 */
TEST_F(OCIContainer_L2Test, ContainerStoppedEvent_JSONRPC)
{
    JSONRPC::LinkType<Core::JSON::IElement> jsonrpc(OCICONTAINER_CALLSIGN, OCICONTAINERTEST_CALLSIGN);
    StrictMock<AsyncHandlerMock_OCIContainer> async_handler;
    uint32_t signalled = OCICONTAINER_STATUS_INVALID;
    uint32_t status = Core::ERROR_GENERAL;
    std::string message;
    JsonObject expected_status;
    int32_t descriptor = 91;

    status = jsonrpc.Subscribe<JsonObject>(EVNT_TIMEOUT,
        _T("onContainerStopped"),
        &AsyncHandlerMock_OCIContainer::onContainerStopped,
        &async_handler);
    EXPECT_EQ(Core::ERROR_NONE, status);

    message = "{\"containerId\":\"com.bskyb.epgui\",\"name\":\"com.bskyb.epgui\"}";
    expected_status.FromString(message);
    EXPECT_CALL(async_handler, onContainerStopped(MatchRequestStatus(expected_status)))
        .WillOnce(Invoke(this, &OCIContainer_L2Test::onContainerStopped));

    std::list<std::pair<int32_t, std::string>> containerslist = { { 91, "com.bskyb.epgui" }, { 94, "Netflix" } };
    // manually trigger the state change event for the container
    // as the mock does not trigger it
    EXPECT_CALL(*p_dobbyProxyMock, listContainers())
        .WillRepeatedly(::testing::Return(containerslist));
    this->triggerStateChangeEvent(descriptor, "com.bskyb.epgui", IDobbyProxyEvents::ContainerState::Stopped);

    signalled = WaitForRequestStatus(EVNT_TIMEOUT, ON_CONTAINER_STOPPED);
    EXPECT_TRUE(signalled & ON_CONTAINER_STOPPED);
    jsonrpc.Unsubscribe(EVNT_TIMEOUT, _T("onContainerStopped"));
}

/*
 * Test case to verify the ON_CONTAINER_STARTED event
 * This test case checks if the ON_CONTAINER_STARTED event is triggered correctly when a container starts.
 * It uses a mock to simulate the container state change and verifies the event notification.
 */
TEST_F(OCIContainer_L2Test, ContainerStartedEvent_JSONRPC)
{
    JSONRPC::LinkType<Core::JSON::IElement> jsonrpc(OCICONTAINER_CALLSIGN, OCICONTAINERTEST_CALLSIGN);
    StrictMock<AsyncHandlerMock_OCIContainer> async_handler;
    uint32_t signalled = OCICONTAINER_STATUS_INVALID;
    uint32_t status = Core::ERROR_GENERAL;
    std::string message;
    JsonObject expected_status;
    int32_t descriptor = 91;

    status = jsonrpc.Subscribe<JsonObject>(EVNT_TIMEOUT,
        _T("onContainerStarted"),
        &AsyncHandlerMock_OCIContainer::onContainerStarted,
        &async_handler);
    EXPECT_EQ(Core::ERROR_NONE, status);

    message = "{\"containerId\":\"com.bskyb.epgui\",\"name\":\"com.bskyb.epgui\"}";
    expected_status.FromString(message);
    EXPECT_CALL(async_handler, onContainerStarted(MatchRequestStatus(expected_status)))
        .WillOnce(Invoke(this, &OCIContainer_L2Test::onContainerStarted));

    std::list<std::pair<int32_t, std::string>> containerslist = { { 91, "com.bskyb.epgui" }, { 94, "Netflix" } };
    // manually trigger the state change event for the container
    // as the mock does not trigger it
    EXPECT_CALL(*p_dobbyProxyMock, listContainers())
        .WillRepeatedly(::testing::Return(containerslist));
    this->triggerStateChangeEvent(descriptor, "com.bskyb.epgui", IDobbyProxyEvents::ContainerState::Running);

    signalled = WaitForRequestStatus(EVNT_TIMEOUT, ON_CONTAINER_STARTED);
    EXPECT_TRUE(signalled & ON_CONTAINER_STARTED);
    jsonrpc.Unsubscribe(EVNT_TIMEOUT, _T("onContainerStarted"));
}

/*
 * Test case to verify the ON_CONTAINER_FAILED event
 * This test case checks if the ON_CONTAINER_FAILED event is triggered correctly when a container fails.
 * It uses a mock to simulate the container failure and verifies the event notification.
 */
TEST_F(OCIContainer_L2Test, ContainerFailedEvent_JSONRPC)
{
    JSONRPC::LinkType<Core::JSON::IElement> jsonrpc(OCICONTAINER_CALLSIGN, OCICONTAINERTEST_CALLSIGN);
    StrictMock<AsyncHandlerMock_OCIContainer> async_handler;
    uint32_t signalled = OCICONTAINER_STATUS_INVALID;
    uint32_t status = Core::ERROR_GENERAL;
    std::string message;
    JsonObject expected_status;

    status = jsonrpc.Subscribe<JsonObject>(EVNT_TIMEOUT,
        _T("onContainerFailed"),
        &AsyncHandlerMock_OCIContainer::onContainerFailed,
        &async_handler);
    EXPECT_EQ(Core::ERROR_NONE, status);

    message = "{\"containerId\":\"com.bskyb.epgui\",\"name\":\"com.bskyb.epgui\",\"error\":1}";
    expected_status.FromString(message);
    EXPECT_CALL(async_handler, onContainerFailed(MatchRequestStatus(expected_status)))
        .WillOnce(Invoke(this, &OCIContainer_L2Test::onContainerFailed));

    EXPECT_CALL(*p_dobbyProxyMock, stopContainer(91, ::testing::_))
        .WillOnce(::testing::Return(true));

    std::list<std::pair<int32_t, std::string>> containerslist = { { 91, "com.bskyb.epgui" }, { 94, "Netflix" } };
    EXPECT_CALL(*p_dobbyProxyMock, listContainers())
        .WillRepeatedly(::testing::Return(containerslist));
    // manually trigger the state change event for the container
    // as the mock does not trigger it
    this->triggerOmiErrorEvent("com.bskyb.epgui", omi::IOmiProxy::ErrorType::verityFailed);

    signalled = WaitForRequestStatus(EVNT_TIMEOUT, ON_CONTAINER_FAILED);
    EXPECT_TRUE(signalled & ON_CONTAINER_FAILED);
    jsonrpc.Unsubscribe(EVNT_TIMEOUT, _T("onContainerFailed"));
}

/*
 * Test case to check the error case for GetContainerState API in OCIContainer plugin.
 * Checks the success param and print the error reason.
 */
TEST_F(OCIContainer_L2Test, GetContainerState_ErrorCase)
{
    uint32_t status = Core::ERROR_GENERAL;
    string containerID = "com.bskyb.epgui";
    ContainerState state;
    string errorReason;
    bool success = true;
    if (CreateOCIContainerInterfaceObject() != Core::ERROR_NONE) {
        TEST_LOG("Invalid OCIContainer_Client");
    } else {
        EXPECT_TRUE(m_controller_OCIcontainer != nullptr);
        if (m_controller_OCIcontainer) {
            EXPECT_TRUE(m_OCIContainerplugin != nullptr);
            if (m_OCIContainerplugin) {
                m_OCIContainerplugin->AddRef();

                std::list<std::pair<int32_t, std::string>> containerslist;
                EXPECT_CALL(*p_dobbyProxyMock, listContainers())
                    .WillRepeatedly(::testing::Return(containerslist));

                status = m_OCIContainerplugin->GetContainerState(containerID, state, success, errorReason);
                EXPECT_EQ(status, Core::ERROR_NONE);
                EXPECT_FALSE(success);
                TEST_LOG("GetContainerState returned state: %d, success: %d, errorReason: %s", state, success, errorReason.c_str());

                m_OCIContainerplugin->Release();
            } else {
                TEST_LOG("m_OCIContainerplugin is NULL");
            }
            m_controller_OCIcontainer->Release();
        } else {
            TEST_LOG("m_controller_OCIcontainer is NULL");
        }
    }
}

/*
 * Test case to check the error case for GetContainerInfo API in OCIContainer plugin.
 * Checks the success param and print the error reason.
 */
TEST_F(OCIContainer_L2Test, GetContainerInfo_ErrorCase)
{
    uint32_t status = Core::ERROR_GENERAL;
    string containerID = "com.bskyb.epgui";
    string info, errorReason;
    bool success = true;

    if (CreateOCIContainerInterfaceObject() != Core::ERROR_NONE) {
        TEST_LOG("Invalid OCIContainer_Client");
    } else {
        EXPECT_TRUE(m_controller_OCIcontainer != nullptr);
        if (m_controller_OCIcontainer) {
            EXPECT_TRUE(m_OCIContainerplugin != nullptr);
            if (m_OCIContainerplugin) {
                m_OCIContainerplugin->AddRef();
                std::list<std::pair<int32_t, std::string>> containerslist = { { 91, "com.bskyb.epgui" }, { 94, "Netflix" } };
                EXPECT_CALL(*p_dobbyProxyMock, listContainers())
                    .WillOnce(::testing::Return(containerslist));

                EXPECT_CALL(*p_dobbyProxyMock, getContainerInfo(91))
                    .WillOnce(::testing::Return(""));

                status = m_OCIContainerplugin->GetContainerInfo(containerID, info, success, errorReason);
                EXPECT_EQ(status, Core::ERROR_NONE);
                EXPECT_FALSE(success);
                TEST_LOG("GetContainerInfo returned info: %s, success: %d, errorReason: %s", info.c_str(), success, errorReason.c_str());

                m_OCIContainerplugin->Release();
            } else {
                TEST_LOG("m_OCIContainerplugin is NULL");
            }
            m_controller_OCIcontainer->Release();
        } else {
            TEST_LOG("m_controller_OCIcontainer is NULL");
        }
    }
}

/*
 * Test case to check the error case for StartContainer API in OCIContainer plugin.
 * Checks the success param and print the error reason.
 */
TEST_F(OCIContainer_L2Test, StartContainer_ErrorCase)
{
    uint32_t status = Core::ERROR_GENERAL;
    string containerID = "com.bskyb.epgui", bundlepath = "/containers/myBundle", command = "command", westerOSSocket = "/usr/mySocket";
    int32_t descriptor = 0;
    string errorReason;
    bool success = true;
    if (CreateOCIContainerInterfaceObject() != Core::ERROR_NONE) {
        TEST_LOG("Invalid OCIContainer_Client");
    } else {
        EXPECT_TRUE(m_controller_OCIcontainer != nullptr);
        if (m_controller_OCIcontainer) {
            EXPECT_TRUE(m_OCIContainerplugin != nullptr);
            if (m_OCIContainerplugin) {
                m_OCIContainerplugin->AddRef();

                EXPECT_CALL(*p_dobbyProxyMock, startContainerFromBundle(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
                    .WillOnce(::testing::Return(-1));

                status = m_OCIContainerplugin->StartContainer(containerID, bundlepath, command, westerOSSocket, descriptor, success, errorReason);
                EXPECT_EQ(status, Core::ERROR_NONE);
                EXPECT_FALSE(success);
                TEST_LOG("StartContainer returned descriptor: %d, errorReason: %s", descriptor, errorReason.c_str());

                m_OCIContainerplugin->Release();
            } else {
                TEST_LOG("m_OCIContainerplugin is NULL");
            }
            m_controller_OCIcontainer->Release();
        } else {
            TEST_LOG("m_controller_OCIcontainer is NULL");
        }
    }
}

/*
 * Test case to check the error case for StartContainerFromDobbySpec API in OCIContainer plugin.
 * Checks the success param and print the error reason.
 */
TEST_F(OCIContainer_L2Test, StartContainerFromDobbySpecErrorCase)
{
    uint32_t status = Core::ERROR_GENERAL;
    Core::Sink<OCIContainerNotificationHandler> notify;
    string containerID = "com.bskyb.epgui", dobbySpec = "/containers/dobbySpec", command = "command", westerOSSocket = "/usr/mySocket";
    int32_t descriptor = 0;
    string errorReason;
    bool success = true;
    if (CreateOCIContainerInterfaceObject() != Core::ERROR_NONE) {
        TEST_LOG("Invalid OCIContainer_Client");
    } else {
        EXPECT_TRUE(m_controller_OCIcontainer != nullptr);
        if (m_controller_OCIcontainer) {
            EXPECT_TRUE(m_OCIContainerplugin != nullptr);
            if (m_OCIContainerplugin) {
                m_OCIContainerplugin->AddRef();

                EXPECT_CALL(*p_dobbyProxyMock, startContainerFromSpec(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
                    .WillOnce(::testing::Return(-1));

                status = m_OCIContainerplugin->StartContainerFromDobbySpec(containerID, dobbySpec, command, westerOSSocket, descriptor, success, errorReason);
                EXPECT_EQ(status, Core::ERROR_NONE);
                EXPECT_FALSE(success);
                TEST_LOG("StartContainerFromDobbySpec returned descriptor: %d, errorReason: %s", descriptor, errorReason.c_str());

                m_OCIContainerplugin->Release();
            } else {
                TEST_LOG("m_OCIContainerplugin is NULL");
            }
            m_controller_OCIcontainer->Release();
        } else {
            TEST_LOG("m_controller_OCIcontainer is NULL");
        }
    }
}

/*
 * Test case to check the error case for StopContainer API in OCIContainer plugin.
 * Checks the success param and print the error reason.
 */
TEST_F(OCIContainer_L2Test, StopContainer_ErrorCase)
{
    uint32_t status = Core::ERROR_GENERAL;
    string containerID = "com.bskyb.epgui";
    bool force = true;
    string errorReason;
    bool success = true;
    if (CreateOCIContainerInterfaceObject() != Core::ERROR_NONE) {
        TEST_LOG("Invalid OCIContainer_Client");
    } else {
        EXPECT_TRUE(m_controller_OCIcontainer != nullptr);
        if (m_controller_OCIcontainer) {
            EXPECT_TRUE(m_OCIContainerplugin != nullptr);
            if (m_OCIContainerplugin) {
                m_OCIContainerplugin->AddRef();

                std::list<std::pair<int32_t, std::string>> containerslist = { { 91, "com.bskyb.epgui" }, { 94, "Netflix" } };
                EXPECT_CALL(*p_dobbyProxyMock, listContainers())
                    .WillOnce(::testing::Return(containerslist));

                EXPECT_CALL(*p_dobbyProxyMock, stopContainer(91, ::testing::_))
                    .WillOnce(::testing::Return(false));

                status = m_OCIContainerplugin->StopContainer(containerID, force, success, errorReason);
                EXPECT_EQ(status, Core::ERROR_NONE);
                EXPECT_FALSE(success);
                TEST_LOG("StopContainer returned ContainerID: %s, errorReason: %s", containerID.c_str(), errorReason.c_str());

                m_OCIContainerplugin->Release();
            } else {
                TEST_LOG("m_OCIContainerplugin is NULL");
            }
            m_controller_OCIcontainer->Release();
        } else {
            TEST_LOG("m_controller_OCIcontainer is NULL");
        }
    }
}

/*
 * Test case to check the error case for PauseContainer and ResumeContainer APIs in OCIContainer plugin.
 * Checks the success param and print the error reason.
 */
TEST_F(OCIContainer_L2Test, PauseContainer_ErrorCase)
{
    uint32_t status = Core::ERROR_GENERAL;
    string containerID = "com.bskyb.epgui";
    string errorReason;
    bool success = true;
    if (CreateOCIContainerInterfaceObject() != Core::ERROR_NONE) {
        TEST_LOG("Invalid OCIContainer_Client");
    } else {
        EXPECT_TRUE(m_controller_OCIcontainer != nullptr);
        if (m_controller_OCIcontainer) {
            EXPECT_TRUE(m_OCIContainerplugin != nullptr);
            if (m_OCIContainerplugin) {
                m_OCIContainerplugin->AddRef();

                std::list<std::pair<int32_t, std::string>> containerslist = { { 91, "com.bskyb.epgui" }, { 94, "Netflix" } };
                EXPECT_CALL(*p_dobbyProxyMock, listContainers())
                    .WillOnce(::testing::Return(containerslist));

                EXPECT_CALL(*p_dobbyProxyMock, pauseContainer(91))
                    .WillOnce(::testing::Return(false));

                status = m_OCIContainerplugin->PauseContainer(containerID, success, errorReason);
                EXPECT_EQ(status, Core::ERROR_NONE);
                EXPECT_FALSE(success);
                TEST_LOG("PauseContainer returned ContainerID: %s, errorReason: %s", containerID.c_str(), errorReason.c_str());
                m_OCIContainerplugin->Release();
            } else {
                TEST_LOG("m_OCIContainerplugin is NULL");
            }
            m_controller_OCIcontainer->Release();
        } else {
            TEST_LOG("m_controller_OCIcontainer is NULL");
        }
    }
}

/*
 * Test case to check the error case for ResumeContainer API in OCIContainer plugin.
 * Checks the success param and print the error reason.
 */
TEST_F(OCIContainer_L2Test, ResumeContainer_ErrorCase)
{
    uint32_t status = Core::ERROR_GENERAL;
    string containerID = "com.bskyb.epgui";
    string errorReason;
    bool success = true;
    if (CreateOCIContainerInterfaceObject() != Core::ERROR_NONE) {
        TEST_LOG("Invalid OCIContainer_Client");
    } else {
        EXPECT_TRUE(m_controller_OCIcontainer != nullptr);
        if (m_controller_OCIcontainer) {
            EXPECT_TRUE(m_OCIContainerplugin != nullptr);
            if (m_OCIContainerplugin) {
                m_OCIContainerplugin->AddRef();

                std::list<std::pair<int32_t, std::string>> containerslist = { { 91, "com.bskyb.epgui" }, { 94, "Netflix" } };
                EXPECT_CALL(*p_dobbyProxyMock, listContainers())
                    .WillOnce(::testing::Return(containerslist));

                EXPECT_CALL(*p_dobbyProxyMock, resumeContainer(91))
                    .WillOnce(::testing::Return(false));

                status = m_OCIContainerplugin->ResumeContainer(containerID, success, errorReason);
                EXPECT_EQ(status, Core::ERROR_NONE);
                EXPECT_FALSE(success);
                TEST_LOG("ResumeContainer returned ContainerID: %s, errorReason: %s", containerID.c_str(), errorReason.c_str());

                m_OCIContainerplugin->Release();
            } else {
                TEST_LOG("m_OCIContainerplugin is NULL");
            }
            m_controller_OCIcontainer->Release();
        } else {
            TEST_LOG("m_controller_OCIcontainer is NULL");
        }
    }
}

/*
 * Test case to check the error case for HibernateContainer and WakeupContainer APIs in OCIContainer plugin.
 * Checks the success param and print the error reason.
 */
TEST_F(OCIContainer_L2Test, HibernateContainer_ErrorCase)
{
    uint32_t status = Core::ERROR_GENERAL;
    string containerID = "com.bskyb.epgui";
    string errorReason, options;
    bool success = true;
    if (CreateOCIContainerInterfaceObject() != Core::ERROR_NONE) {
        TEST_LOG("Invalid OCIContainer_Client");
    } else {
        EXPECT_TRUE(m_controller_OCIcontainer != nullptr);
        if (m_controller_OCIcontainer) {
            EXPECT_TRUE(m_OCIContainerplugin != nullptr);
            if (m_OCIContainerplugin) {
                m_OCIContainerplugin->AddRef();

                std::list<std::pair<int32_t, std::string>> containerslist = { { 91, "com.bskyb.epgui" }, { 94, "Netflix" } };
                EXPECT_CALL(*p_dobbyProxyMock, listContainers())
                    .WillOnce(::testing::Return(containerslist));

                EXPECT_CALL(*p_dobbyProxyMock, hibernateContainer(91, ::testing::_))
                    .WillOnce(::testing::Return(false));

                status = m_OCIContainerplugin->HibernateContainer(containerID, options, success, errorReason);
                EXPECT_EQ(status, Core::ERROR_NONE);
                EXPECT_FALSE(success);
                TEST_LOG("HibernateContainer returned ContainerID: %s, errorReason: %s", containerID.c_str(), errorReason.c_str());

                m_OCIContainerplugin->Release();
            } else {
                TEST_LOG("m_OCIContainerplugin is NULL");
            }
            m_controller_OCIcontainer->Release();
        } else {
            TEST_LOG("m_controller_OCIcontainer is NULL");
        }
    }
}

/*
 * Test case to check the error case for WakeupContainer API in OCIContainer plugin.
 * Checks the success param and print the error reason.
 */
TEST_F(OCIContainer_L2Test, WakeupContainer_ErrorCase)
{
    uint32_t status = Core::ERROR_GENERAL;
    string containerID = "com.bskyb.epgui";
    string errorReason;
    bool success = true;
    if (CreateOCIContainerInterfaceObject() != Core::ERROR_NONE) {
        TEST_LOG("Invalid OCIContainer_Client");
    } else {
        EXPECT_TRUE(m_controller_OCIcontainer != nullptr);
        if (m_controller_OCIcontainer) {
            EXPECT_TRUE(m_OCIContainerplugin != nullptr);
            if (m_OCIContainerplugin) {
                m_OCIContainerplugin->AddRef();

                std::list<std::pair<int32_t, std::string>> containerslist = { { 91, "com.bskyb.epgui" }, { 94, "Netflix" } };
                EXPECT_CALL(*p_dobbyProxyMock, listContainers())
                    .WillOnce(::testing::Return(containerslist));

                EXPECT_CALL(*p_dobbyProxyMock, wakeupContainer(91))
                    .WillOnce(::testing::Return(false));

                status = m_OCIContainerplugin->WakeupContainer(containerID, success, errorReason);
                EXPECT_EQ(status, Core::ERROR_NONE);
                EXPECT_FALSE(success);
                TEST_LOG("WakeupContainer returned ContainerID: %s, errorReason: %s", containerID.c_str(), errorReason.c_str());

                m_OCIContainerplugin->Release();
            } else {
                TEST_LOG("m_OCIContainerplugin is NULL");
            }
            m_controller_OCIcontainer->Release();
        } else {
            TEST_LOG("m_controller_OCIcontainer is NULL");
        }
    }
}

/*
 * Test case to check the error case for ExecuteCommand API in OCIContainer plugin.
 * Checks the success param and print the error reason.
 */
TEST_F(OCIContainer_L2Test, ExecuteCommand_ErrorCase)
{
    uint32_t status = Core::ERROR_GENERAL;
    string containerID = "com.bskyb.epgui";
    string errorReason, options, command;
    bool success = true;
    if (CreateOCIContainerInterfaceObject() != Core::ERROR_NONE) {
        TEST_LOG("Invalid OCIContainer_Client");
    } else {
        EXPECT_TRUE(m_controller_OCIcontainer != nullptr);
        if (m_controller_OCIcontainer) {
            EXPECT_TRUE(m_OCIContainerplugin != nullptr);
            if (m_OCIContainerplugin) {
                m_OCIContainerplugin->AddRef();

                std::list<std::pair<int32_t, std::string>> containerslist = { { 91, "com.bskyb.epgui" }, { 94, "Netflix" } };
                EXPECT_CALL(*p_dobbyProxyMock, listContainers())
                    .WillOnce(::testing::Return(containerslist));

                EXPECT_CALL(*p_dobbyProxyMock, execInContainer(91, ::testing::_, ::testing::_))
                    .WillOnce(::testing::Return(false));

                status = m_OCIContainerplugin->ExecuteCommand(containerID, options, command, success, errorReason);
                EXPECT_EQ(status, Core::ERROR_NONE);
                EXPECT_FALSE(success);
                TEST_LOG("ExecuteCommand returned ContainerID: %s, errorReason: %s", containerID.c_str(), errorReason.c_str());

                m_OCIContainerplugin->Release();
            } else {
                TEST_LOG("m_OCIContainerplugin is NULL");
            }
            m_controller_OCIcontainer->Release();
        } else {
            TEST_LOG("m_controller_OCIcontainer is NULL");
        }
    }
}

/*
 * Test case to check the error case for Annotate API in OCIContainer plugin.
 * Checks the success param and print the error reason.
 */
TEST_F(OCIContainer_L2Test, Annotate_ErrorCase)
{
    uint32_t status = Core::ERROR_GENERAL;
    string containerID = "com.bskyb.epgui";
    string errorReason, key, value;
    bool success = true;
    if (CreateOCIContainerInterfaceObject() != Core::ERROR_NONE) {
        TEST_LOG("Invalid OCIContainer_Client");
    } else {
        EXPECT_TRUE(m_controller_OCIcontainer != nullptr);
        if (m_controller_OCIcontainer) {
            EXPECT_TRUE(m_OCIContainerplugin != nullptr);
            if (m_OCIContainerplugin) {
                m_OCIContainerplugin->AddRef();

                std::list<std::pair<int32_t, std::string>> containerslist = { { 91, "com.bskyb.epgui" }, { 94, "Netflix" } };
                EXPECT_CALL(*p_dobbyProxyMock, listContainers())
                    .WillOnce(::testing::Return(containerslist));

                EXPECT_CALL(*p_dobbyProxyMock, addAnnotation(91, ::testing::_, ::testing::_))
                    .WillOnce(::testing::Return(false));

                status = m_OCIContainerplugin->Annotate(containerID, key, value, success, errorReason);
                EXPECT_EQ(status, Core::ERROR_NONE);
                EXPECT_FALSE(success);
                TEST_LOG("Annotate returned ContainerID: %s, errorReason: %s", containerID.c_str(), errorReason.c_str());

                m_OCIContainerplugin->Release();
            } else {
                TEST_LOG("m_OCIContainerplugin is NULL");
            }
            m_controller_OCIcontainer->Release();
        } else {
            TEST_LOG("m_controller_OCIcontainer is NULL");
        }
    }
}

/*
 * Test case to check the error case for RemoveAnnotation API in OCIContainer plugin.
 * Checks the success param and print the error reason.
 */
TEST_F(OCIContainer_L2Test, RemoveAnnotation_ErrorCase)
{
    uint32_t status = Core::ERROR_GENERAL;
    string containerID = "com.bskyb.epgui";
    string errorReason, key;
    bool success = true;
    if (CreateOCIContainerInterfaceObject() != Core::ERROR_NONE) {
        TEST_LOG("Invalid OCIContainer_Client");
    } else {
        EXPECT_TRUE(m_controller_OCIcontainer != nullptr);
        if (m_controller_OCIcontainer) {
            EXPECT_TRUE(m_OCIContainerplugin != nullptr);
            if (m_OCIContainerplugin) {
                m_OCIContainerplugin->AddRef();

                std::list<std::pair<int32_t, std::string>> containerslist = { { 91, "com.bskyb.epgui" }, { 94, "Netflix" } };
                EXPECT_CALL(*p_dobbyProxyMock, listContainers())
                    .WillOnce(::testing::Return(containerslist));

                EXPECT_CALL(*p_dobbyProxyMock, removeAnnotation(91, ::testing::_))
                    .WillOnce(::testing::Return(false));

                status = m_OCIContainerplugin->RemoveAnnotation(containerID, key, success, errorReason);
                EXPECT_EQ(status, Core::ERROR_NONE);
                EXPECT_FALSE(success);
                TEST_LOG("RemoveAnnotation returned ContainerID: %s, errorReason: %s", containerID.c_str(), errorReason.c_str());

                m_OCIContainerplugin->Release();
            } else {
                TEST_LOG("m_OCIContainerplugin is NULL");
            }
            m_controller_OCIcontainer->Release();
        } else {
            TEST_LOG("m_controller_OCIcontainer is NULL");
        }
    }
}

/*
 * Test case to check the error case for Mount and Unmount APIs in OCIContainer plugin.
 * Checks the success param and print the error reason.
 */
TEST_F(OCIContainer_L2Test, Mount_ErrorCase)
{
    uint32_t status = Core::ERROR_GENERAL;
    string containerID = "com.bskyb.epgui";
    string errorReason, source, target, type, options;
    bool success = true;
    if (CreateOCIContainerInterfaceObject() != Core::ERROR_NONE) {
        TEST_LOG("Invalid OCIContainer_Client");
    } else {
        EXPECT_TRUE(m_controller_OCIcontainer != nullptr);
        if (m_controller_OCIcontainer) {
            EXPECT_TRUE(m_OCIContainerplugin != nullptr);
            if (m_OCIContainerplugin) {
                m_OCIContainerplugin->AddRef();

                std::list<std::pair<int32_t, std::string>> containerslist = { { 91, "com.bskyb.epgui" }, { 94, "Netflix" } };
                EXPECT_CALL(*p_dobbyProxyMock, listContainers())
                    .WillOnce(::testing::Return(containerslist));

                EXPECT_CALL(*p_dobbyProxyMock, addContainerMount(91, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
                    .WillOnce(::testing::Return(false));

                status = m_OCIContainerplugin->Mount(containerID, source, target, type, options, success, errorReason);
                EXPECT_EQ(status, Core::ERROR_NONE);
                EXPECT_FALSE(success);
                TEST_LOG("Mount returned ContainerID: %s, errorReason: %s", containerID.c_str(), errorReason.c_str());

                m_OCIContainerplugin->Release();
            } else {
                TEST_LOG("m_OCIContainerplugin is NULL");
            }
            m_controller_OCIcontainer->Release();
        } else {
            TEST_LOG("m_controller_OCIcontainer is NULL");
        }
    }
}

/*
 * Test case to check the error case for Unmount API in OCIContainer plugin.
 * Checks the success param and print the error reason.
 */
TEST_F(OCIContainer_L2Test, Unmount_ErrorCase)
{
    uint32_t status = Core::ERROR_GENERAL;
    string containerID = "com.bskyb.epgui";
    string errorReason, target;
    bool success = true;
    if (CreateOCIContainerInterfaceObject() != Core::ERROR_NONE) {
        TEST_LOG("Invalid OCIContainer_Client");
    } else {
        EXPECT_TRUE(m_controller_OCIcontainer != nullptr);
        if (m_controller_OCIcontainer) {
            EXPECT_TRUE(m_OCIContainerplugin != nullptr);
            if (m_OCIContainerplugin) {
                m_OCIContainerplugin->AddRef();

                std::list<std::pair<int32_t, std::string>> containerslist = { { 91, "com.bskyb.epgui" }, { 94, "Netflix" } };
                EXPECT_CALL(*p_dobbyProxyMock, listContainers())
                    .WillOnce(::testing::Return(containerslist));

                EXPECT_CALL(*p_dobbyProxyMock, removeContainerMount(91, ::testing::_))
                    .WillOnce(::testing::Return(false));

                status = m_OCIContainerplugin->Unmount(containerID, target, success, errorReason);
                EXPECT_EQ(status, Core::ERROR_NONE);
                EXPECT_FALSE(success);
                TEST_LOG("Unmount returned ContainerID: %s, errorReason: %s", containerID.c_str(), errorReason.c_str());

                m_OCIContainerplugin->Release();
            } else {
                TEST_LOG("m_OCIContainerplugin is NULL");
            }
            m_controller_OCIcontainer->Release();
        } else {
            TEST_LOG("m_controller_OCIcontainer is NULL");
        }
    }
}
