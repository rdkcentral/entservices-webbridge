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
#include <interfaces/ISharedStorage.h>
#include <interfaces/IStore2.h>
#include <interfaces/IStoreCache.h>
#include <mutex>
#include "SecureStorageServerMock.h"
#include "SecureStorageServiceMock.h"

#define EVNT_TIMEOUT (1000)
#define SHAREDSTORAGE_CALLSIGN _T("org.rdk.SharedStorage.1")
#define SHAREDSTORAGETEST_CALLSIGN _T("L2tests.1")

#define TEST_LOG(x, ...)                                                                                                                         \
    fprintf(stderr, "\033[1;32m[%s:%d](%s)<PID:%d><TID:%d>" x "\n\033[0m", __FILE__, __LINE__, __FUNCTION__, getpid(), gettid(), ##__VA_ARGS__); \
    fflush(stderr);

using ::testing::NiceMock;
using namespace WPEFramework;
using testing::StrictMock;
using ::WPEFramework::Exchange::ISharedStorage;
using ::WPEFramework::Exchange::ISharedStorageCache;
using ::WPEFramework::Exchange::ISharedStorageInspector;
using ::WPEFramework::Exchange::ISharedStorageLimit;
using ScopeType = WPEFramework::Exchange::ISharedStorage::ScopeType;
using Success = WPEFramework::Exchange::ISharedStorage::Success;

typedef enum : uint32_t {

    SHARED_STORAGE_ON_VALUE_CHANGED = 0x00000001,
    SHARED_STORAGE_STATUS_INVALID = 0x00000000

} SharedStorageL2test_async_events_t;

class AsyncHandlerMock_SharedStorage {
public:
    AsyncHandlerMock_SharedStorage()
    {
    }
    MOCK_METHOD(void, onValueChanged, (const JsonObject& message));
};

// SharedStorage and CloudStore Notification Handler Class
class SharedStorageNotificationHandler : public Exchange::ISharedStorage::INotification {
private:
    /** @brief Mutex */
    std::mutex m_mutex;

    /** @brief Condition variable */
    std::condition_variable m_condition_variable;

    /** @brief Event signalled flag */
    uint32_t m_event_signalled;

    BEGIN_INTERFACE_MAP(Notification)
    INTERFACE_ENTRY(Exchange::ISharedStorage::INotification)
    END_INTERFACE_MAP

public:
    SharedStorageNotificationHandler() {}
    ~SharedStorageNotificationHandler() {}

    // Shared Store notification
    void OnValueChanged(const ScopeType scope, const string& ns, const string& key, const string& value)
    {
        TEST_LOG("OnValueChanged event triggered ***\n");
        std::unique_lock<std::mutex> lock(m_mutex);

        TEST_LOG("OnValueChanged received (SCOPE): %s\n", scope == ScopeType::DEVICE ? "DEVICE" : "ACCOUNT");
        TEST_LOG("OnValueChanged received (NAMESPACE): %s\n", ns.c_str());
        TEST_LOG("OnValueChanged received (KEY): %s\n", key.c_str());
        TEST_LOG("OnValueChanged received (VALUE): %s\n", value.c_str());
        /* Notify the requester thread. */
        m_event_signalled |= SHARED_STORAGE_ON_VALUE_CHANGED;
        m_condition_variable.notify_one();
    }

    uint32_t WaitForRequestStatus(uint32_t timeout_ms, SharedStorageL2test_async_events_t expected_status)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        auto now = std::chrono::system_clock::now();
        std::chrono::milliseconds timeout(timeout_ms);
        uint32_t signalled = SHARED_STORAGE_STATUS_INVALID;

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

class SharedStorage_L2test : public L2TestMocks {
protected:
    virtual ~SharedStorage_L2test() override;
    SharedStorage_L2test();

public:
    void onValueChanged(const JsonObject& message);
    uint32_t WaitForRequestStatus(uint32_t timeout_ms, SharedStorageL2test_async_events_t expected_status);
    uint32_t CreateSharedStorageInterfaceObject();

private:
    /** @brief Mutex */
    std::mutex m_mutex;

    /** @brief Condition variable */
    std::condition_variable m_condition_variable;

    /** @brief Event signalled flag */
    uint32_t m_event_signalled;

protected:
    /** @brief Pointer to the IShell interface */
    PluginHost::IShell* m_controller_sharedstorage;

    /** @brief Pointer to the ISharedStorage interface */
    Exchange::ISharedStorage* m_sharedstorageplugin;

    std::unique_ptr<MockSecureStorageServer> mock_server;

    // start the mock server before the plugin is activated
    void SetUp() override
    {
        auto service = std::make_unique<NiceMock<SecureStorageServiceMock>>();
        mock_server = std::make_unique<MockSecureStorageServer>(std::move(service), "0.0.0.0:50051");

        uint32_t status = Core::ERROR_GENERAL;
        status = ActivateService("org.rdk.CloudStore");
        EXPECT_EQ(Core::ERROR_NONE, status);
        status = ActivateService("org.rdk.SharedStorage");
        EXPECT_EQ(Core::ERROR_NONE, status);
    }

    SecureStorageServiceMock* GetMock() { return mock_server->GetMock(); }
};

SharedStorage_L2test::SharedStorage_L2test()
    : L2TestMocks()
{
    uint32_t status = Core::ERROR_GENERAL;

    /* Activate plugin in constructor */
    status = ActivateService("org.rdk.PersistentStore");
    EXPECT_EQ(Core::ERROR_NONE, status);
}

SharedStorage_L2test::~SharedStorage_L2test()
{
    TEST_LOG("Inside SharedStorage_L2test destructor");

    uint32_t status = Core::ERROR_GENERAL;

    /* Deactivate plugin in destructor */
    status = DeactivateService("org.rdk.CloudStore");
    EXPECT_EQ(Core::ERROR_NONE, status);
    status = DeactivateService("org.rdk.SharedStorage");
    EXPECT_EQ(Core::ERROR_NONE, status);
    status = DeactivateService("org.rdk.PersistentStore");
    EXPECT_EQ(Core::ERROR_NONE, status);

    if (mock_server) {
        mock_server.reset();
        std::this_thread::sleep_for(std::chrono::seconds(5)); // Wait for the mock server to properly shutdown
    }
}

uint32_t SharedStorage_L2test::CreateSharedStorageInterfaceObject()
{
    uint32_t return_value = Core::ERROR_GENERAL;
    Core::ProxyType<RPC::InvokeServerType<1, 0, 4>> SharedStorage_Engine;
    Core::ProxyType<RPC::CommunicatorClient> SharedStorage_Client;

    TEST_LOG("Creating SharedStorage_Engine");
    SharedStorage_Engine = Core::ProxyType<RPC::InvokeServerType<1, 0, 4>>::Create();
    SharedStorage_Client = Core::ProxyType<RPC::CommunicatorClient>::Create(Core::NodeId("/tmp/communicator"), Core::ProxyType<Core::IIPCServer>(SharedStorage_Engine));

    TEST_LOG("Creating SharedStorage_Engine Announcements");
#if ((THUNDER_VERSION == 2) || ((THUNDER_VERSION == 4) && (THUNDER_VERSION_MINOR == 2)))
    SharedStorage_Engine->Announcements(mSharedStorage_Client->Announcement());
#endif
    if (!SharedStorage_Client.IsValid()) {
        TEST_LOG("Invalid SharedStorage_Client");
    } else {
        m_controller_sharedstorage = SharedStorage_Client->Open<PluginHost::IShell>(_T("org.rdk.SharedStorage"), ~0, 3000);
        if (m_controller_sharedstorage) {
            m_sharedstorageplugin = m_controller_sharedstorage->QueryInterface<Exchange::ISharedStorage>();
            return_value = Core::ERROR_NONE;
        }
    }
    return return_value;
}

class SharedStorage_L2testDeviceScope : public L2TestMocks {
protected:
    virtual ~SharedStorage_L2testDeviceScope() override;
    SharedStorage_L2testDeviceScope();

public:
    void onValueChanged(const JsonObject& message);
    uint32_t WaitForRequestStatus(uint32_t timeout_ms, SharedStorageL2test_async_events_t expected_status);
    uint32_t CreateSharedStorageInterfaceObject(int interface);

private:
    /** @brief Mutex */
    std::mutex m_mutex;

    /** @brief Condition variable */
    std::condition_variable m_condition_variable;

    /** @brief Event signalled flag */
    uint32_t m_event_signalled;

protected:
    /** @brief Pointer to the IShell interface */
    PluginHost::IShell* m_controller_sharedstorage;

    /** @brief Pointer to the ISharedStorage interface */
    Exchange::ISharedStorage* m_sharedstorageplugin;

    /** @brief Pointer to the ISharedStorageInspector interface */
    Exchange::ISharedStorageInspector* m_sharedstorageinspectorplugin;

    /** @brief Pointer to the ISharedStorageLimit interface */
    Exchange::ISharedStorageLimit* m_sharedstoragelimitplugin;

    /** @brief Pointer to the ISharedStorageCache interface */
    Exchange::ISharedStorageCache* m_sharedstoragecacheplugin;
};

SharedStorage_L2testDeviceScope::SharedStorage_L2testDeviceScope()
    : L2TestMocks()
{
    uint32_t status = Core::ERROR_GENERAL;

    /* Activate plugin in constructor */
    status = ActivateService("org.rdk.PersistentStore");
    EXPECT_EQ(Core::ERROR_NONE, status);
    status = ActivateService("org.rdk.SharedStorage");
    EXPECT_EQ(Core::ERROR_NONE, status);
}

SharedStorage_L2testDeviceScope::~SharedStorage_L2testDeviceScope()
{
    TEST_LOG("Inside SharedStorage_L2testDeviceScope destructor");

    uint32_t status = Core::ERROR_GENERAL;

    /* Deactivate plugin in destructor */
    status = DeactivateService("org.rdk.SharedStorage");
    EXPECT_EQ(Core::ERROR_NONE, status);
    status = DeactivateService("org.rdk.PersistentStore");
    EXPECT_EQ(Core::ERROR_NONE, status);
}

uint32_t SharedStorage_L2testDeviceScope::CreateSharedStorageInterfaceObject(int interface)
{
    uint32_t return_value = Core::ERROR_GENERAL;
    Core::ProxyType<RPC::InvokeServerType<1, 0, 4>> SharedStorage_Engine;
    Core::ProxyType<RPC::CommunicatorClient> SharedStorage_Client;

    TEST_LOG("Creating SharedStorage_Engine");
    SharedStorage_Engine = Core::ProxyType<RPC::InvokeServerType<1, 0, 4>>::Create();
    SharedStorage_Client = Core::ProxyType<RPC::CommunicatorClient>::Create(Core::NodeId("/tmp/communicator"), Core::ProxyType<Core::IIPCServer>(SharedStorage_Engine));

    TEST_LOG("Creating SharedStorage_Engine Announcements");
#if ((THUNDER_VERSION == 2) || ((THUNDER_VERSION == 4) && (THUNDER_VERSION_MINOR == 2)))
    SharedStorage_Engine->Announcements(mSharedStorage_Client->Announcement());
#endif
    if (!SharedStorage_Client.IsValid()) {
        TEST_LOG("Invalid SharedStorage_Client");
    } else {
        m_controller_sharedstorage = SharedStorage_Client->Open<PluginHost::IShell>(_T("org.rdk.SharedStorage"), ~0, 3000);
        if (m_controller_sharedstorage) {
            if (interface == 0) {
                m_sharedstorageplugin = m_controller_sharedstorage->QueryInterface<Exchange::ISharedStorage>();
                return_value = Core::ERROR_NONE;
            } else if (interface == 1) {
                m_sharedstorageinspectorplugin = m_controller_sharedstorage->QueryInterface<Exchange::ISharedStorageInspector>();
                return_value = Core::ERROR_NONE;
            } else if (interface == 2) {
                m_sharedstoragelimitplugin = m_controller_sharedstorage->QueryInterface<Exchange::ISharedStorageLimit>();
                return_value = Core::ERROR_NONE;
            } else if (interface == 3) {
                m_sharedstoragecacheplugin = m_controller_sharedstorage->QueryInterface<Exchange::ISharedStorageCache>();
                return_value = Core::ERROR_NONE;
            }
        }
    }
    return return_value;
}

uint32_t SharedStorage_L2test::WaitForRequestStatus(uint32_t timeout_ms, SharedStorageL2test_async_events_t expected_status)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    auto now = std::chrono::system_clock::now();
    std::chrono::milliseconds timeout(timeout_ms);
    uint32_t signalled = SHARED_STORAGE_STATUS_INVALID;

    while (!(expected_status & m_event_signalled)) {
        if (m_condition_variable.wait_until(lock, now + timeout) == std::cv_status::timeout) {
            TEST_LOG("Timeout waiting for request status event");
            break;
        }
    }

    signalled = m_event_signalled;
    return signalled;
}

void SharedStorage_L2test::onValueChanged(const JsonObject& message)
{
    TEST_LOG("OnValueChanged event triggered ***\n");
    std::unique_lock<std::mutex> lock(m_mutex);

    std::string str;
    message.ToString(str);

    TEST_LOG("OnValueChanged received: %s\n", str.c_str());

    /* Notify the requester thread. */
    m_event_signalled |= SHARED_STORAGE_ON_VALUE_CHANGED;
    m_condition_variable.notify_one();
}

uint32_t SharedStorage_L2testDeviceScope::WaitForRequestStatus(uint32_t timeout_ms, SharedStorageL2test_async_events_t expected_status)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    auto now = std::chrono::system_clock::now();
    std::chrono::milliseconds timeout(timeout_ms);
    uint32_t signalled = SHARED_STORAGE_STATUS_INVALID;

    while (!(expected_status & m_event_signalled)) {
        if (m_condition_variable.wait_until(lock, now + timeout) == std::cv_status::timeout) {
            TEST_LOG("Timeout waiting for request status event");
            break;
        }
    }

    signalled = m_event_signalled;
    return signalled;
}

void SharedStorage_L2testDeviceScope::onValueChanged(const JsonObject& message)
{
    TEST_LOG("OnValueChanged event triggered ***\n");
    std::unique_lock<std::mutex> lock(m_mutex);

    std::string str;
    message.ToString(str);

    TEST_LOG("OnValueChanged received: %s\n", str.c_str());

    /* Notify the requester thread. */
    m_event_signalled |= SHARED_STORAGE_ON_VALUE_CHANGED;
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
 * Test case to test the SetValue method of SharedStorage plugin with scope set as ACCOUNT using JSONRPC.
 * This test case uses a mock gRPC server to simulate the behavior of the SecureStorageService.
 * The test case also checks that the OnValueChanged notification is triggered with the correct
 * parameters.
 */

TEST_F(SharedStorage_L2test, SetValue_ACCOUNT_Scope_JSONRPC)
{
    JSONRPC::LinkType<Core::JSON::IElement> jsonrpc(SHAREDSTORAGE_CALLSIGN, SHAREDSTORAGETEST_CALLSIGN);
    StrictMock<AsyncHandlerMock_SharedStorage> async_handler;
    uint32_t signalled = SHARED_STORAGE_STATUS_INVALID;
    uint32_t status = Core::ERROR_GENERAL;
    std::string message;
    JsonObject expected_status;

    status = jsonrpc.Subscribe<JsonObject>(EVNT_TIMEOUT,
        _T("onValueChanged"),
        &AsyncHandlerMock_SharedStorage::onValueChanged,
        &async_handler);
    EXPECT_EQ(Core::ERROR_NONE, status);

    message = "{\"scope\":\"account\",\"namespace\":\"ns1\",\"key\":\"key1\",\"value\":\"value1\"}";
    expected_status.FromString(message);
    EXPECT_CALL(async_handler, onValueChanged(MatchRequestStatus(expected_status)))
        .WillOnce(Invoke(this, &SharedStorage_L2test::onValueChanged));

    EXPECT_CALL(*GetMock(), UpdateValue(::testing::_, ::testing::_, ::testing::_))
        .WillOnce(testing::Invoke([](::grpc::ServerContext*,
                                      const distp::gateway::secure_storage::v1::UpdateValueRequest* request,
                                      distp::gateway::secure_storage::v1::UpdateValueResponse* response) {
            EXPECT_EQ(request->value().value(), "value1");
            EXPECT_EQ(request->value().key().key(), "key1");
            EXPECT_EQ(request->value().key().app_id(), "ns1");
            EXPECT_EQ(request->value().key().scope(), distp::gateway::secure_storage::v1::SCOPE_ACCOUNT);
            return grpc::Status::OK;
        }));

    JsonObject params, result;
    params["namespace"] = "ns1";
    params["key"] = "key1";
    params["value"] = "value1";
    params["scope"] = "account";
    params["ttl"] = 60;
    status = InvokeServiceMethod("org.rdk.SharedStorage.1", "setValue", params, result);
    EXPECT_EQ(status, Core::ERROR_NONE);
    EXPECT_TRUE(result["success"].Boolean());

    signalled = WaitForRequestStatus(EVNT_TIMEOUT, SHARED_STORAGE_ON_VALUE_CHANGED);
    EXPECT_TRUE(signalled & SHARED_STORAGE_ON_VALUE_CHANGED);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    jsonrpc.Unsubscribe(EVNT_TIMEOUT, _T("onValueChanged"));
}

/*
 * Test case to test the GetValue method of SharedStorage plugin with scope set as ACCOUNT using JSONRPC.
 * This test case uses a mock gRPC server to simulate the behavior of the SecureStorageService.
 */
TEST_F(SharedStorage_L2test, GetValue_ACCOUNT_Scope_JSONRPC)
{
    JSONRPC::LinkType<Core::JSON::IElement> jsonrpc(SHAREDSTORAGE_CALLSIGN, SHAREDSTORAGETEST_CALLSIGN);
    uint32_t status = Core::ERROR_GENERAL;

    EXPECT_CALL(*GetMock(), GetValue(::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Invoke(
            [](grpc::ServerContext*,
                const distp::gateway::secure_storage::v1::GetValueRequest* request,
                distp::gateway::secure_storage::v1::GetValueResponse* response) {
                EXPECT_EQ(request->key().key(), "key1");
                EXPECT_EQ(request->key().app_id(), "ns1");
                EXPECT_EQ(request->key().scope(), distp::gateway::secure_storage::v1::SCOPE_ACCOUNT);

                auto* value = new distp::gateway::secure_storage::v1::Value();
                value->set_value("value1");
                auto* ttl = new google::protobuf::Duration();
                ttl->set_seconds(60);
                value->set_allocated_ttl(ttl);
                auto* key = new distp::gateway::secure_storage::v1::Key();
                key->set_key(request->key().key());
                key->set_app_id(request->key().app_id());
                key->set_scope(request->key().scope());
                value->set_allocated_key(key);
                response->set_allocated_value(value);
                return grpc::Status::OK;
            }));

    JsonObject params, result;
    params["scope"] = "ACCOUNT";
    params["namespace"] = "ns1";
    params["key"] = "key1";
    status = InvokeServiceMethod("org.rdk.SharedStorage.1", "getValue", params, result);
    EXPECT_EQ(status, Core::ERROR_NONE);
    EXPECT_EQ(result["value"], "value1");
    EXPECT_TRUE(result["success"].Boolean());
}

/*
 * Test case to test the DeleteKey method of SharedStorage plugin with scope set as ACCOUNT using JSONRPC.
 * This test case uses a mock gRPC server to simulate the behavior of the SecureStorageService.
 */
TEST_F(SharedStorage_L2test, deleteKey_ACCOUNT_Scope_JSONRPC)
{
    JSONRPC::LinkType<Core::JSON::IElement> jsonrpc(SHAREDSTORAGE_CALLSIGN, SHAREDSTORAGETEST_CALLSIGN);
    uint32_t status = Core::ERROR_GENERAL;

    EXPECT_CALL(*GetMock(), DeleteValue(::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Invoke(
            [&](::grpc::ServerContext* context,
                const distp::gateway::secure_storage::v1::DeleteValueRequest* request,
                distp::gateway::secure_storage::v1::DeleteValueResponse* response) {
                return grpc::Status::OK;
            }));

    JsonObject params, result;
    params["namespace"] = "ns1";
    params["key"] = "key1";
    params["scope"] = "ACCOUNT";
    status = InvokeServiceMethod("org.rdk.SharedStorage.1", "deleteKey", params, result);
    EXPECT_EQ(status, Core::ERROR_NONE);
    EXPECT_TRUE(result["success"].Boolean());
}

/*
 * Test case to test the DeleteNamespace method of SharedStorage plugin with scope set as ACCOUNT using JSONRPC.
 * This test case uses a mock gRPC server to simulate the behavior of the SecureStorageService.
 */
TEST_F(SharedStorage_L2test, deleteNamespace_ACCOUNT_Scope_JSONRPC)
{
    JSONRPC::LinkType<Core::JSON::IElement> jsonrpc(SHAREDSTORAGE_CALLSIGN, SHAREDSTORAGETEST_CALLSIGN);
    uint32_t status = Core::ERROR_GENERAL;

    EXPECT_CALL(*GetMock(), DeleteAllValues(::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::Invoke(
            [](grpc::ServerContext* context,
                const distp::gateway::secure_storage::v1::DeleteAllValuesRequest* request,
                distp::gateway::secure_storage::v1::DeleteAllValuesResponse* response) {
                EXPECT_EQ(request->scope(), distp::gateway::secure_storage::v1::Scope::SCOPE_ACCOUNT);
                EXPECT_EQ(request->app_id(), "ns1");
                return grpc::Status::OK;
            }));

    JsonObject params, result;
    params["namespace"] = "ns1";
    params["scope"] = "ACCOUNT";
    status = InvokeServiceMethod("org.rdk.SharedStorage.1", "deleteNamespace", params, result);
    EXPECT_EQ(status, Core::ERROR_NONE);
    EXPECT_TRUE(result["success"].Boolean());
}

/*
 * Test case to test the SetValue method of SharedStorage plugin with scope set as DEVICE using JSONRPC.
 * The test case also checks that the OnValueChanged notification is triggered with the correct
 * parameters.
 */
TEST_F(SharedStorage_L2testDeviceScope, SetValue_DEVICE_Scope_JSONRPC)
{
    JSONRPC::LinkType<Core::JSON::IElement> jsonrpc(SHAREDSTORAGE_CALLSIGN, SHAREDSTORAGETEST_CALLSIGN);
    StrictMock<AsyncHandlerMock_SharedStorage> async_handler;
    uint32_t signalled = SHARED_STORAGE_STATUS_INVALID;
    uint32_t status = Core::ERROR_GENERAL;
    std::string message;
    JsonObject expected_status;

    status = jsonrpc.Subscribe<JsonObject>(EVNT_TIMEOUT,
        _T("onValueChanged"),
        &AsyncHandlerMock_SharedStorage::onValueChanged,
        &async_handler);
    EXPECT_EQ(Core::ERROR_NONE, status);

    message = "{\"scope\":\"device\",\"namespace\":\"ns1\",\"key\":\"key1\",\"value\":\"value1\"}";
    expected_status.FromString(message);
    EXPECT_CALL(async_handler, onValueChanged(MatchRequestStatus(expected_status)))
        .WillOnce(Invoke(this, &SharedStorage_L2testDeviceScope::onValueChanged));

    JsonObject params, result;
    params["namespace"] = "ns1";
    params["key"] = "key1";
    params["value"] = "value1";
    params["scope"] = "device";
    params["ttl"] = 100;
    status = InvokeServiceMethod("org.rdk.SharedStorage.1", "setValue", params, result);
    EXPECT_EQ(status, Core::ERROR_NONE);
    EXPECT_TRUE(result["success"].Boolean());

    signalled = WaitForRequestStatus(EVNT_TIMEOUT, SHARED_STORAGE_ON_VALUE_CHANGED);
    EXPECT_TRUE(signalled & SHARED_STORAGE_ON_VALUE_CHANGED);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    jsonrpc.Unsubscribe(EVNT_TIMEOUT, _T("onValueChanged"));
}

/*
 * Test case to test the GetValue method of SharedStorage plugin with scope set as DEVICE using JSONRPC.
 */
TEST_F(SharedStorage_L2testDeviceScope, GetValue_DEVICE_Scope_JSONRPC)
{
    JSONRPC::LinkType<Core::JSON::IElement> jsonrpc(SHAREDSTORAGE_CALLSIGN, SHAREDSTORAGETEST_CALLSIGN);
    uint32_t status = Core::ERROR_GENERAL;

    JsonObject params, result;
    params["scope"] = "device";
    params["namespace"] = "ns1";
    params["key"] = "key1";
    status = InvokeServiceMethod("org.rdk.SharedStorage.1", "getValue", params, result);
    EXPECT_EQ(status, Core::ERROR_NONE);
    EXPECT_TRUE(result["success"].Boolean());
    sleep(5);
    int file_status = remove("/tmp/secure/persistent/rdkservicestore");
    // Check if the file has been successfully removed
    if (file_status != 0) {
        TEST_LOG("Error deleting file[/tmp/secure/persistent/rdkservicestore]");
    } else {
        TEST_LOG("File[/tmp/secure/persistent/rdkservicestore] successfully deleted");
    }
}

/*
 * Test case to test the DeleteKey method of SharedStorage plugin with scope set as DEVICE using JSONRPC.
 */
TEST_F(SharedStorage_L2testDeviceScope, deleteKey_DEVICE_Scope_JSONRPC)
{
    JSONRPC::LinkType<Core::JSON::IElement> jsonrpc(SHAREDSTORAGE_CALLSIGN, SHAREDSTORAGETEST_CALLSIGN);
    uint32_t status = Core::ERROR_GENERAL;

    JsonObject params, result;
    params["namespace"] = "ns1";
    params["key"] = "key1";
    params["scope"] = "device";
    status = InvokeServiceMethod("org.rdk.SharedStorage.1", "deleteKey", params, result);
    EXPECT_EQ(status, Core::ERROR_NONE);
    EXPECT_TRUE(result["success"].Boolean());
}

/*
 * Test case to test the DeleteNamespace method of SharedStorage plugin with scope set as DEVICE using JSONRPC.
 */
TEST_F(SharedStorage_L2testDeviceScope, deleteNamespace_DEVICE_Scope_JSONRPC)
{
    JSONRPC::LinkType<Core::JSON::IElement> jsonrpc(SHAREDSTORAGE_CALLSIGN, SHAREDSTORAGETEST_CALLSIGN);
    uint32_t status = Core::ERROR_GENERAL;

    JsonObject params, result;
    params["namespace"] = "ns1";
    params["scope"] = "device";
    status = InvokeServiceMethod("org.rdk.SharedStorage.1", "deleteNamespace", params, result);
    EXPECT_EQ(status, Core::ERROR_NONE);
    EXPECT_TRUE(result["success"].Boolean());
}

/*
 * Test case to test the SetValue method of SharedStorage plugin with scope set as ACCOUNT using COMRPC.
 * This test case uses a mock gRPC server to simulate the behavior of the SecureStorageService.
 * The test case also checks that the OnValueChanged notification is triggered with the correct
 * parameters.
 */
TEST_F(SharedStorage_L2test, SetValue_ACCOUNT_Scope_COMRPC)
{
    uint32_t signalled = SHARED_STORAGE_STATUS_INVALID;
    Core::Sink<SharedStorageNotificationHandler> notify;
    uint32_t status = Core::ERROR_GENERAL;
    const auto kKey = "key1";
    const auto kValue = "value1";
    const auto kAppId = "ns1";
    const auto kTtl = 60;
    Success result;

    if (CreateSharedStorageInterfaceObject() != Core::ERROR_NONE) {
        TEST_LOG("Invalid SharedStore_Client");
    } else {
        EXPECT_TRUE(m_controller_sharedstorage != nullptr);
        if (m_controller_sharedstorage) {
            EXPECT_TRUE(m_sharedstorageplugin != nullptr);
            if (m_sharedstorageplugin) {
                m_sharedstorageplugin->AddRef();
                m_sharedstorageplugin->Register(&notify);

                EXPECT_CALL(*GetMock(), UpdateValue(::testing::_, ::testing::_, ::testing::_))
                    .WillOnce(testing::Invoke([](::grpc::ServerContext*,
                                                  const distp::gateway::secure_storage::v1::UpdateValueRequest* request,
                                                  distp::gateway::secure_storage::v1::UpdateValueResponse* response) {
                        EXPECT_EQ(request->value().value(), "value1");
                        EXPECT_EQ(request->value().key().key(), "key1");
                        EXPECT_EQ(request->value().key().app_id(), "ns1");
                        EXPECT_EQ(request->value().key().scope(), distp::gateway::secure_storage::v1::SCOPE_ACCOUNT);
                        return grpc::Status::OK;
                    }));

                status = m_sharedstorageplugin->SetValue(ScopeType::ACCOUNT, kAppId, kKey, kValue, kTtl, result);
                EXPECT_EQ(status, Core::ERROR_NONE);
                EXPECT_TRUE(result.success);

                signalled = notify.WaitForRequestStatus(EVNT_TIMEOUT, SHARED_STORAGE_ON_VALUE_CHANGED);
                EXPECT_TRUE(signalled & SHARED_STORAGE_ON_VALUE_CHANGED);

                std::this_thread::sleep_for(std::chrono::milliseconds(10));

                m_sharedstorageplugin->Unregister(&notify);
                m_sharedstorageplugin->Release();
            } else {
                TEST_LOG("m_sharedstorageplugin is NULL");
            }
            m_controller_sharedstorage->Release();
        } else {
            TEST_LOG("m_controller_sharedstorage is NULL");
        }
    }
}

/*
 * Test case to test the GetValue method of SharedStorage plugin with scope set as ACCOUNT using COMRPC.
 * This test case uses a mock gRPC server to simulate the behavior of the SecureStorageService.
 */
TEST_F(SharedStorage_L2test, GetValue_ACCOUNT_Scope_COMRPC)
{
    uint32_t status = Core::ERROR_GENERAL;
    string value;
    uint32_t ttl;
    bool success = false;
    if (CreateSharedStorageInterfaceObject() != Core::ERROR_NONE) {
        TEST_LOG("Invalid SharedStore_Client");
    } else {
        EXPECT_TRUE(m_controller_sharedstorage != nullptr);
        if (m_controller_sharedstorage) {
            EXPECT_TRUE(m_sharedstorageplugin != nullptr);
            if (m_sharedstorageplugin) {
                m_sharedstorageplugin->AddRef();

                EXPECT_CALL(*GetMock(), GetValue(::testing::_, ::testing::_, ::testing::_))
                    .WillOnce(::testing::Invoke(
                        [](grpc::ServerContext*,
                            const distp::gateway::secure_storage::v1::GetValueRequest* request,
                            distp::gateway::secure_storage::v1::GetValueResponse* response) {
                            EXPECT_EQ(request->key().key(), "key1");
                            EXPECT_EQ(request->key().app_id(), "ns1");
                            EXPECT_EQ(request->key().scope(), distp::gateway::secure_storage::v1::SCOPE_ACCOUNT);

                            auto* value = new distp::gateway::secure_storage::v1::Value();
                            value->set_value("mocked_value");
                            auto* ttl = new google::protobuf::Duration();
                            ttl->set_seconds(60);
                            value->set_allocated_ttl(ttl);
                            auto* key = new distp::gateway::secure_storage::v1::Key();
                            key->set_key(request->key().key());
                            key->set_app_id(request->key().app_id());
                            key->set_scope(request->key().scope());
                            value->set_allocated_key(key);
                            response->set_allocated_value(value);
                            return grpc::Status::OK;
                        }));

                status = m_sharedstorageplugin->GetValue(ScopeType::ACCOUNT, "ns1", "key1", value, ttl, success);
                EXPECT_EQ(status, Core::ERROR_NONE);
                TEST_LOG("GetValue returned value: %s, ttl: %u, success: %d", value.c_str(), ttl, success);
                EXPECT_TRUE(success);

                m_sharedstorageplugin->Release();
            } else {
                TEST_LOG("m_sharedstorageplugin is NULL");
            }
            m_controller_sharedstorage->Release();
        } else {
            TEST_LOG("m_controller_sharedstorage is NULL");
        }
    }
}

/*
 * Test case to test the DeleteKey method of SharedStorage plugin with scope set as ACCOUNT using COMRPC.
 * This test case uses a mock gRPC server to simulate the behavior of the SecureStorageService.
 */
TEST_F(SharedStorage_L2test, DeleteKey_ACCOUNT_Scope_COMRPC)
{
    uint32_t status = Core::ERROR_GENERAL;
    Success result;

    if (CreateSharedStorageInterfaceObject() != Core::ERROR_NONE) {
        TEST_LOG("Invalid SharedStore_Client");
    } else {
        EXPECT_TRUE(m_controller_sharedstorage != nullptr);
        if (m_controller_sharedstorage) {
            EXPECT_TRUE(m_sharedstorageplugin != nullptr);
            if (m_sharedstorageplugin) {
                m_sharedstorageplugin->AddRef();

                EXPECT_CALL(*GetMock(), DeleteValue(::testing::_, ::testing::_, ::testing::_))
                    .WillOnce(::testing::Invoke(
                        [&](::grpc::ServerContext* context,
                            const distp::gateway::secure_storage::v1::DeleteValueRequest* request,
                            distp::gateway::secure_storage::v1::DeleteValueResponse* response) {
                            return grpc::Status::OK;
                        }));

                status = m_sharedstorageplugin->DeleteKey(ScopeType::ACCOUNT, "namespace", "key", result);
                EXPECT_EQ(status, Core::ERROR_NONE);
                EXPECT_TRUE(result.success);

                m_sharedstorageplugin->Release();
            } else {
                TEST_LOG("m_sharedstorageplugin is NULL");
            }
            m_controller_sharedstorage->Release();
        } else {
            TEST_LOG("m_controller_sharedstorage is NULL");
        }
    }
}

/* * Test case to test the DeleteNamespace method of SharedStorage plugin with scope set as ACCOUNT using COMRPC.
 * This test case uses a mock gRPC server to simulate the behavior of the SecureStorageService.
 */
TEST_F(SharedStorage_L2test, DeleteNamespace_ACCOUNT_Scope_COMRPC)
{
    uint32_t status = Core::ERROR_GENERAL;
    Success result;

    if (CreateSharedStorageInterfaceObject() != Core::ERROR_NONE) {
        TEST_LOG("Invalid SharedStore_Client");
    } else {
        EXPECT_TRUE(m_controller_sharedstorage != nullptr);
        if (m_controller_sharedstorage) {
            EXPECT_TRUE(m_sharedstorageplugin != nullptr);
            if (m_sharedstorageplugin) {
                m_sharedstorageplugin->AddRef();

                EXPECT_CALL(*GetMock(), DeleteAllValues(::testing::_, ::testing::_, ::testing::_))
                    .WillOnce(::testing::Invoke(
                        [](grpc::ServerContext* context,
                            const distp::gateway::secure_storage::v1::DeleteAllValuesRequest* request,
                            distp::gateway::secure_storage::v1::DeleteAllValuesResponse* response) {
                            EXPECT_EQ(request->scope(), distp::gateway::secure_storage::v1::Scope::SCOPE_ACCOUNT);
                            EXPECT_EQ(request->app_id(), "ns1");
                            return grpc::Status::OK;
                        }));

                status = m_sharedstorageplugin->DeleteNamespace(ScopeType::ACCOUNT, "ns1", result);
                EXPECT_EQ(status, Core::ERROR_NONE);
                EXPECT_TRUE(result.success);

                m_sharedstorageplugin->Release();
            } else {
                TEST_LOG("m_sharedstorageplugin is NULL");
            }
            m_controller_sharedstorage->Release();
        } else {
            TEST_LOG("m_controller_sharedstorage is NULL");
        }
    }
}

/*
 * Test case to test the SetValue method of SharedStorage plugin with scope set as Device using COMRPC.
 * The test case also checks that the OnValueChanged notification is triggered with the correct
 * parameters.
 */
TEST_F(SharedStorage_L2testDeviceScope, SetValue_DEVICE_Scope_COMRPC)
{
    uint32_t signalled = SHARED_STORAGE_STATUS_INVALID;
    Core::Sink<SharedStorageNotificationHandler> notify;
    uint32_t status = Core::ERROR_GENERAL;
    int interface = 0; // 0 for ISharedStorage
    const auto kKey = "key1";
    const auto kValue = "value1";
    const auto kAppId = "ns1";
    const auto kTtl = 60;
    Success result;

    if (CreateSharedStorageInterfaceObject(interface) != Core::ERROR_NONE) {
        TEST_LOG("Invalid SharedStore_Client");
    } else {
        EXPECT_TRUE(m_controller_sharedstorage != nullptr);
        if (m_controller_sharedstorage) {
            EXPECT_TRUE(m_sharedstorageplugin != nullptr);
            if (m_sharedstorageplugin) {
                m_sharedstorageplugin->AddRef();
                m_sharedstorageplugin->Register(&notify);

                status = m_sharedstorageplugin->SetValue(ScopeType::DEVICE, kAppId, kKey, kValue, kTtl, result);
                EXPECT_EQ(status, Core::ERROR_NONE);
                EXPECT_TRUE(result.success);

                signalled = notify.WaitForRequestStatus(EVNT_TIMEOUT, SHARED_STORAGE_ON_VALUE_CHANGED);
                EXPECT_TRUE(signalled & SHARED_STORAGE_ON_VALUE_CHANGED);

                std::this_thread::sleep_for(std::chrono::milliseconds(10));

                m_sharedstorageplugin->Unregister(&notify);
                m_sharedstorageplugin->Release();
            } else {
                TEST_LOG("m_sharedstorageplugin is NULL");
            }
            m_controller_sharedstorage->Release();
        } else {
            TEST_LOG("m_controller_sharedstorage is NULL");
        }
    }
}

/*
 * Test case to test the GetValue method of SharedStorage plugin with scope set as DEVICE using COMRPC.
 */
TEST_F(SharedStorage_L2testDeviceScope, GetValue_DEVICE_Scope_COMRPC)
{
    uint32_t status = Core::ERROR_GENERAL;
    int interface = 0; // 0 for ISharedStorage
    string value;
    uint32_t ttl;
    bool success = false;
    if (CreateSharedStorageInterfaceObject(interface) != Core::ERROR_NONE) {
        TEST_LOG("Invalid SharedStore_Client");
    } else {
        EXPECT_TRUE(m_controller_sharedstorage != nullptr);
        if (m_controller_sharedstorage) {
            EXPECT_TRUE(m_sharedstorageplugin != nullptr);
            if (m_sharedstorageplugin) {
                m_sharedstorageplugin->AddRef();

                status = m_sharedstorageplugin->GetValue(ScopeType::DEVICE, "ns1", "key1", value, ttl, success);
                EXPECT_EQ(status, Core::ERROR_NONE);
                TEST_LOG("GetValue returned value: %s, ttl: %u, success: %d", value.c_str(), ttl, success);
                EXPECT_TRUE(success);

                sleep(5);
                int file_status = remove("/tmp/secure/persistent/rdkservicestore");
                // Check if the file has been successfully removed
                if (file_status != 0) {
                    TEST_LOG("Error deleting file[/tmp/secure/persistent/rdkservicestore]");
                } else {
                    TEST_LOG("File[/tmp/secure/persistent/rdkservicestore] successfully deleted");
                }

                m_sharedstorageplugin->Release();
            } else {
                TEST_LOG("m_sharedstorageplugin is NULL");
            }
            m_controller_sharedstorage->Release();
        } else {
            TEST_LOG("m_controller_sharedstorage is NULL");
        }
    }
}

/*
 * Test case to test the DeleteKey method of SharedStorage plugin with scope set as DEVICE using COMRPC.
 */
TEST_F(SharedStorage_L2testDeviceScope, DeleteKey_DEVICE_Scope_COMRPC)
{
    uint32_t status = Core::ERROR_GENERAL;
    int interface = 0; // 0 for ISharedStorage
    Success result;

    if (CreateSharedStorageInterfaceObject(interface) != Core::ERROR_NONE) {
        TEST_LOG("Invalid SharedStore_Client");
    } else {
        EXPECT_TRUE(m_controller_sharedstorage != nullptr);
        if (m_controller_sharedstorage) {
            EXPECT_TRUE(m_sharedstorageplugin != nullptr);
            if (m_sharedstorageplugin) {
                m_sharedstorageplugin->AddRef();

                status = m_sharedstorageplugin->DeleteKey(ScopeType::DEVICE, "ns1", "key1", result);
                EXPECT_EQ(status, Core::ERROR_NONE);
                EXPECT_TRUE(result.success);

                m_sharedstorageplugin->Release();
            } else {
                TEST_LOG("m_sharedstorageplugin is NULL");
            }
            m_controller_sharedstorage->Release();
        } else {
            TEST_LOG("m_controller_sharedstorage is NULL");
        }
    }
}

/*
 * Test case to test the DeleteNamespace method of SharedStorage plugin with scope set as DEVICE using COMRPC.
 */
TEST_F(SharedStorage_L2testDeviceScope, DeleteNamespace_DEVICE_Scope_COMRPC)
{
    uint32_t status = Core::ERROR_GENERAL;
    int interface = 0; // 0 for ISharedStorage
    Success result;

    if (CreateSharedStorageInterfaceObject(interface) != Core::ERROR_NONE) {
        TEST_LOG("Invalid SharedStore_Client");
    } else {
        EXPECT_TRUE(m_controller_sharedstorage != nullptr);
        if (m_controller_sharedstorage) {
            EXPECT_TRUE(m_sharedstorageplugin != nullptr);
            if (m_sharedstorageplugin) {
                m_sharedstorageplugin->AddRef();

                status = m_sharedstorageplugin->DeleteNamespace(ScopeType::DEVICE, "ns1", result);
                EXPECT_EQ(status, Core::ERROR_NONE);
                EXPECT_TRUE(result.success);

                m_sharedstorageplugin->Release();
            } else {
                TEST_LOG("m_sharedstorageplugin is NULL");
            }
            m_controller_sharedstorage->Release();
        } else {
            TEST_LOG("m_controller_sharedstorage is NULL");
        }
    }
}

/*
 * Test cases to test the GetKeys method of SharedStorageInspector plugin with scope set as DEVICE using COMRPC.
 */
TEST_F(SharedStorage_L2testDeviceScope, GetKeys_DEVICE_Scope_COMRPC)
{
    uint32_t status = Core::ERROR_GENERAL;
    int interface = 1; // 1 for ISharedStorageInspector
    Exchange::ISharedStorageInspector::IStringIterator* keys = nullptr;
    bool result;

    if (CreateSharedStorageInterfaceObject(interface) != Core::ERROR_NONE) {
        TEST_LOG("Invalid SharedStore_Client");
    } else {
        EXPECT_TRUE(m_controller_sharedstorage != nullptr);
        if (m_controller_sharedstorage) {
            EXPECT_TRUE(m_sharedstorageinspectorplugin != nullptr);
            if (m_sharedstorageinspectorplugin) {
                m_sharedstorageinspectorplugin->AddRef();

                status = m_sharedstorageinspectorplugin->GetKeys(ScopeType::DEVICE, "ns1", keys, result);
                EXPECT_EQ(status, Core::ERROR_NONE);
                EXPECT_TRUE(result);
                // print the keys
                if (keys != nullptr) {
                    std::string key;
                    while (keys->Next(key)) {
                        TEST_LOG("Key: %s", key.c_str());
                    }
                    keys->Release();
                }

                m_sharedstorageinspectorplugin->Release();
            } else {
                TEST_LOG("m_sharedstorageinspectorplugin is NULL");
            }
            m_controller_sharedstorage->Release();
        } else {
            TEST_LOG("m_controller_sharedstorage is NULL");
        }
    }
}

/*
 * Test case to test the GetNamespaces method of SharedStorageInspector plugin with scope set as DEVICE using COMRPC.
 */
TEST_F(SharedStorage_L2testDeviceScope, GetNamespaces_DEVICE_Scope_COMRPC)
{
    uint32_t status = Core::ERROR_GENERAL;
    int interface = 1; // 1 for ISharedStorageInspector
    Exchange::ISharedStorageInspector::IStringIterator* namespaces = nullptr;
    bool result;

    if (CreateSharedStorageInterfaceObject(interface) != Core::ERROR_NONE) {
        TEST_LOG("Invalid SharedStore_Client");
    } else {
        EXPECT_TRUE(m_controller_sharedstorage != nullptr);
        if (m_controller_sharedstorage) {
            EXPECT_TRUE(m_sharedstorageinspectorplugin != nullptr);
            if (m_sharedstorageinspectorplugin) {
                m_sharedstorageinspectorplugin->AddRef();

                status = m_sharedstorageinspectorplugin->GetNamespaces(ScopeType::DEVICE, namespaces, result);
                EXPECT_EQ(status, Core::ERROR_NONE);
                EXPECT_TRUE(result);
                // print the namespaces
                if (namespaces != nullptr) {
                    std::string ns;
                    while (namespaces->Next(ns)) {
                        TEST_LOG("Namespace: %s", ns.c_str());
                    }
                    namespaces->Release();
                }

                m_sharedstorageinspectorplugin->Release();
            } else {
                TEST_LOG("m_sharedstorageinspectorplugin is NULL");
            }
            m_controller_sharedstorage->Release();
        } else {
            TEST_LOG("m_controller_sharedstorage is NULL");
        }
    }
}

/*
 * Test case to test the GetStorageSizes method of SharedStorageInspector plugin with scope set as DEVICE using COMRPC.
 */
TEST_F(SharedStorage_L2testDeviceScope, GetStorageSizes_DEVICE_Scope_COMRPC)
{
    uint32_t status = Core::ERROR_GENERAL;
    int interface = 1; // 1 for ISharedStorageInspector
    Exchange::ISharedStorageInspector::INamespaceSizeIterator* storagelist = nullptr;
    bool result;

    if (CreateSharedStorageInterfaceObject(interface) != Core::ERROR_NONE) {
        TEST_LOG("Invalid SharedStore_Client");
    } else {
        EXPECT_TRUE(m_controller_sharedstorage != nullptr);
        if (m_controller_sharedstorage) {
            EXPECT_TRUE(m_sharedstorageinspectorplugin != nullptr);
            if (m_sharedstorageinspectorplugin) {
                m_sharedstorageinspectorplugin->AddRef();

                status = m_sharedstorageinspectorplugin->GetStorageSizes(ScopeType::DEVICE, storagelist, result);
                EXPECT_EQ(status, Core::ERROR_NONE);
                EXPECT_TRUE(result);
                Exchange::ISharedStorageInspector::NamespaceSize nsSize{};
                while (storagelist->Next(nsSize)) {
                    TEST_LOG("Namespace: %s, Size: %u bytes", nsSize.ns.c_str(), nsSize.size);
                }
                storagelist->Release();

                m_sharedstorageinspectorplugin->Release();
            } else {
                TEST_LOG("m_sharedstorageinspectorplugin is NULL");
            }
            m_controller_sharedstorage->Release();
        } else {
            TEST_LOG("m_controller_sharedstorage is NULL");
        }
    }
}

/*
 * Test case to test the SetNamespaceStorageLimit and GetNamespaceStorageLimit method of SharedStorageLimit plugin with scope set as DEVICE using COMRPC.
 */
TEST_F(SharedStorage_L2testDeviceScope, Set_GetNamespaceStorageLimit_DEVICE_Scope_COMRPC)
{
    uint32_t status = Core::ERROR_GENERAL;
    int interface = 2; // 2 for ISharedStorageLimit
    Exchange::ISharedStorageLimit::StorageLimit Limit;
    uint32_t limit = 100;
    bool result;

    if (CreateSharedStorageInterfaceObject(interface) != Core::ERROR_NONE) {
        TEST_LOG("Invalid SharedStore_Client");
    } else {
        EXPECT_TRUE(m_controller_sharedstorage != nullptr);
        if (m_controller_sharedstorage) {
            EXPECT_TRUE(m_sharedstoragelimitplugin != nullptr);
            if (m_sharedstoragelimitplugin) {
                m_sharedstoragelimitplugin->AddRef();

                status = m_sharedstoragelimitplugin->SetNamespaceStorageLimit(ScopeType::DEVICE, "ns1", limit, result);
                EXPECT_EQ(status, Core::ERROR_NONE);
                EXPECT_TRUE(result);

                status = m_sharedstoragelimitplugin->GetNamespaceStorageLimit(ScopeType::DEVICE, "ns1", Limit);
                EXPECT_EQ(status, Core::ERROR_NONE);
                TEST_LOG("\nStorage Limit: %u bytes\n", Limit.storageLimit);

                m_sharedstoragelimitplugin->Release();
            } else {
                TEST_LOG("m_sharedstoragelimitplugin is NULL");
            }
            m_controller_sharedstorage->Release();
        } else {
            TEST_LOG("m_controller_sharedstorage is NULL");
        }
    }
}

/*
 * Test case to test the FlushCache method of SharedStorageCache plugin using COMRPC.
 */
TEST_F(SharedStorage_L2testDeviceScope, FlushCache_COMRPC)
{
    uint32_t status = Core::ERROR_GENERAL;
    int interface = 3; // 3 for ISharedStorageCache

    if (CreateSharedStorageInterfaceObject(interface) != Core::ERROR_NONE) {
        TEST_LOG("Invalid SharedStore_Client");
    } else {
        EXPECT_TRUE(m_controller_sharedstorage != nullptr);
        if (m_controller_sharedstorage) {
            EXPECT_TRUE(m_sharedstoragecacheplugin != nullptr);
            if (m_sharedstoragecacheplugin) {
                m_sharedstoragecacheplugin->AddRef();

                status = m_sharedstoragecacheplugin->FlushCache();
                EXPECT_EQ(status, Core::ERROR_NONE);

                m_sharedstoragecacheplugin->Release();
            } else {
                TEST_LOG("m_sharedstoragecacheplugin is NULL");
            }
            m_controller_sharedstorage->Release();
        } else {
            TEST_LOG("m_controller_sharedstorage is NULL");
        }
    }
}

/*
 * Test cases to test the GetKeys method of SharedStorageInspector plugin with scope set as DEVICE using JSONRPC.
 */
TEST_F(SharedStorage_L2testDeviceScope, GetKeys_DEVICE_Scope_JSONRPC)
{
    JSONRPC::LinkType<Core::JSON::IElement> jsonrpc(SHAREDSTORAGE_CALLSIGN, SHAREDSTORAGETEST_CALLSIGN);
    uint32_t status = Core::ERROR_GENERAL;

    JsonObject params, result;
    params["namespace"] = "ns1";
    params["scope"] = "device";
    status = InvokeServiceMethod("org.rdk.SharedStorage.1", "getKeys", params, result);
    EXPECT_EQ(status, Core::ERROR_NONE);
    EXPECT_TRUE(result["success"].Boolean());
}

/*
 * Test case to test the GetNamespaces method of SharedStorageInspector plugin with scope set as DEVICE using JSONRPC.
 */
TEST_F(SharedStorage_L2testDeviceScope, GetNamespaces_DEVICE_Scope_JSONRPC)
{
    JSONRPC::LinkType<Core::JSON::IElement> jsonrpc(SHAREDSTORAGE_CALLSIGN, SHAREDSTORAGETEST_CALLSIGN);
    uint32_t status = Core::ERROR_GENERAL;

    JsonObject params, result;
    params["scope"] = "device";
    status = InvokeServiceMethod("org.rdk.SharedStorage.1", "getNamespaces", params, result);
    EXPECT_EQ(status, Core::ERROR_NONE);
    EXPECT_TRUE(result["success"].Boolean());
}

/*
 * Test case to test the GetStorageSizes method of SharedStorageInspector plugin with scope set as DEVICE using JSONRPC.
 */
TEST_F(SharedStorage_L2testDeviceScope, GetStorageSizes_DEVICE_Scope_JSONRPC)
{
    JSONRPC::LinkType<Core::JSON::IElement> jsonrpc(SHAREDSTORAGE_CALLSIGN, SHAREDSTORAGETEST_CALLSIGN);
    uint32_t status = Core::ERROR_GENERAL;

    JsonObject params, result;
    params["scope"] = "device";
    status = InvokeServiceMethod("org.rdk.SharedStorage.1", "getStorageSizes", params, result);
    EXPECT_EQ(status, Core::ERROR_NONE);
    EXPECT_TRUE(result["success"].Boolean());
}

/*
 * Test case to test the SetNamespaceStorageLimit and GetNamespaceStorageLimit method of SharedStorageLimit plugin with scope set as DEVICE using JSONRPC.
 */
TEST_F(SharedStorage_L2testDeviceScope, Set_GetNamespaceStorageLimit_DEVICE_Scope_JSONRPC)
{
    JSONRPC::LinkType<Core::JSON::IElement> jsonrpc(SHAREDSTORAGE_CALLSIGN, SHAREDSTORAGETEST_CALLSIGN);
    uint32_t status = Core::ERROR_GENERAL;

    JsonObject params, result;
    params["scope"] = "device";
    params["namespace"] = "ns1";
    params["storageLimit"] = 100;
    status = InvokeServiceMethod("org.rdk.SharedStorage.1", "setNamespaceStorageLimit", params, result);
    EXPECT_EQ(status, Core::ERROR_NONE);

    status = InvokeServiceMethod("org.rdk.SharedStorage.1", "getNamespaceStorageLimit", params, result);
    EXPECT_EQ(status, Core::ERROR_NONE);
}

/*
 * Test case to test the FlushCache method of SharedStorageCache plugin using JSONRPC.
 */
TEST_F(SharedStorage_L2testDeviceScope, FlushCache_JSONRPC)
{
    JSONRPC::LinkType<Core::JSON::IElement> jsonrpc(SHAREDSTORAGE_CALLSIGN, SHAREDSTORAGETEST_CALLSIGN);
    uint32_t status = Core::ERROR_GENERAL;

    JsonObject params, result;
    status = InvokeServiceMethod("org.rdk.SharedStorage.1", "flushCache", params, result);
    EXPECT_EQ(status, Core::ERROR_NONE);
}
