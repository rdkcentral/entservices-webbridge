
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "ResourceManager.h"
#include "ServiceMock.h"
#include <core/core.h>
#include "ThunderPortability.h"

using namespace WPEFramework;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::_;
using ::testing::DoAll;
using ::testing::SetArgReferee;
using std::string;

// ===== Testable Wrapper Class =====
class TestableResourceManagerPlugin : public Plugin::ResourceManager {
public:
    void setDisableReserveTTS(bool value) {
        mDisableReserveTTS = value;
    }

    bool callSetAVBlocked(const std::string& client, bool blocked) {
        return setAVBlocked(client, blocked);
}

    bool getBlockedAVApplicationsPublic(std::vector<std::string>& appsList) {
        return getBlockedAVApplications(appsList);
    }

};

// ===== Test Fixture Base =====
class ResourceManagerTest : public ::testing::Test {
protected:
    Core::ProxyType<TestableResourceManagerPlugin> plugin;
    Core::JSONRPC::Handler& handler;
    DECL_CORE_JSONRPC_CONX connection;
    string response;

    ResourceManagerTest()
        : plugin(Core::ProxyType<TestableResourceManagerPlugin>::Create())
        , handler(*plugin)
        , INIT_CONX(1, 0)
    {}

    virtual ~ResourceManagerTest() = default;
};

// ===== Mock for SecurityAgent =====
class MockAuthenticate : public PluginHost::IAuthenticate {
public:
    MOCK_METHOD(uint32_t, CreateToken, (const uint16_t length, const uint8_t buffer[], std::string& token), (override));
    MOCK_METHOD(PluginHost::ISecurity*, Officer, (const std::string& token), (override));
    MOCK_METHOD(uint32_t, Release, (), (const, override));
    MOCK_METHOD(void, AddRef, (), (const, override));

    BEGIN_INTERFACE_MAP(MockAuthenticate)
        INTERFACE_ENTRY(PluginHost::IAuthenticate)
    END_INTERFACE_MAP
};

// ===== Derived Fixture with Initialized Plugin =====
class ResourceManagerInitializedTest : public ResourceManagerTest {
protected:
    NiceMock<ServiceMock> service;
    NiceMock<MockAuthenticate>* mockAuth = nullptr;

public:
    ResourceManagerInitializedTest() {
        mockAuth = new NiceMock<MockAuthenticate>();

        ON_CALL(service, QueryInterfaceByCallsign(_, _))
            .WillByDefault([this](const uint32_t, const std::string& name) -> void* {
                if (name == "SecurityAgent") {
                    mockAuth->AddRef();
                    return static_cast<void*>(mockAuth);
                }
                return nullptr;
            });

        plugin->Initialize(&service);
    }

    ~ResourceManagerInitializedTest() override {
        plugin->Deinitialize(&service);
        delete mockAuth;
    }
};

// =========================
// ===== TEST CASES ========
// =========================

TEST_F(ResourceManagerInitializedTest, InitializeGetsSecurityToken)
{
    EXPECT_CALL(*mockAuth, CreateToken(_, _, _))
        .WillOnce(DoAll(SetArgReferee<2>("mocked_token"), Return(Core::ERROR_NONE)));

    EXPECT_CALL(*mockAuth, Release()).Times(1);

    EXPECT_EQ("", plugin->Initialize(&service));
}

TEST_F(ResourceManagerInitializedTest, InitializeFailsToGetToken)
{
    EXPECT_CALL(*mockAuth, CreateToken(_, _, _))
        .WillOnce(Return(Core::ERROR_GENERAL));

    EXPECT_CALL(*mockAuth, Release()).Times(1);

    EXPECT_EQ("", plugin->Initialize(&service));
}

TEST_F(ResourceManagerTest, InitializeWithoutSecurityAgent)
{
    NiceMock<ServiceMock> noSecurityService;

    ON_CALL(noSecurityService, QueryInterfaceByCallsign(_, _))
        .WillByDefault(Return(nullptr));

    EXPECT_EQ("", plugin->Initialize(&noSecurityService));
}

TEST_F(ResourceManagerInitializedTest, RegisteredMethods)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("setAVBlocked")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("reserveTTSResource")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("getBlockedAVApplications")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("reserveTTSResourceForApps")));

}

TEST_F(ResourceManagerInitializedTest, SetAVBlockedTest)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setAVBlocked"),
        _T("{\"appid\":\"testApp\",\"blocked\":true}"), response));
}

TEST_F(ResourceManagerInitializedTest, ReserveTTSResourceTest_1)
{

    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("reserveTTSResource"),
        _T("{\"appid\":\"testApp\"}"), response));
}

TEST_F(ResourceManagerInitializedTest, ReserveTTSResourceForAppsTest_1)
{

    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("reserveTTSResourceForApps"),
        _T("{\"appids\":[\"testApp1\",\"testApp2\"]}"), response));
}

TEST_F(ResourceManagerInitializedTest, ReserveTTSResourceTest_3)
{
    plugin->Deinitialize(&service);  
    plugin->Initialize(&service);       

    plugin->setDisableReserveTTS(true);

    std::cout << "[TEST] tiwarisetDisableReserveTTS(true) called\n";
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("reserveTTSResource"),
        _T("{ \"appid\": \"testApp\" }"), response));

}

TEST_F(ResourceManagerInitializedTest, ReserveTTSResourceForAppsTest_3)
{
    plugin->Deinitialize(&service);
    plugin->Initialize(&service);

    plugin->setDisableReserveTTS(true);

    std::cout << "[TEST] tiwarisetDisableReserveTTS(true) called\n";
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("reserveTTSResourceForApps"),
        _T("{ \"appids\": [\"testApp1\",\"testApp2\"]}"), response));

}

TEST_F(ResourceManagerInitializedTest, ReserveTTSResource_MissingAppId)
{
    plugin->Deinitialize(&service);
    plugin->Initialize(&service);

    plugin->setDisableReserveTTS(false);

    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("reserveTTSResource"),
        _T("{ }"), response));
}

TEST_F(ResourceManagerInitializedTest, ReserveTTSResourceForApps_MissingAppIds)
{
    plugin->Deinitialize(&service);
    plugin->Initialize(&service);

    plugin->setDisableReserveTTS(false);

    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("reserveTTSResourceForApps"),
        _T("{ }"), response));
}

TEST_F(ResourceManagerInitializedTest, InformationReturnsEmptyStringTest)
{
    EXPECT_EQ("", plugin->Information());
}
TEST_F(ResourceManagerInitializedTest, getBlockedAVApplicationsTest_1)
{
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("getBlockedAVApplications"), _T("{}"), response));
}

TEST_F(ResourceManagerInitializedTest, GetBlockedAVApplicationsTest_1)
{
    std::vector<std::string> expectedAppsList = {"App1", "App2", "App3"};
    
    plugin->getBlockedAVApplicationsPublic(expectedAppsList);

    ASSERT_EQ(expectedAppsList.size(), 3);
    EXPECT_EQ(expectedAppsList[0], "App1");
    EXPECT_EQ(expectedAppsList[1], "App2");
    EXPECT_EQ(expectedAppsList[2], "App3");
}

TEST_F(ResourceManagerInitializedTest, SetAVBlockedInternalTest)
{
    bool result = plugin->callSetAVBlocked("testApp", true);
    EXPECT_TRUE(result);
}

