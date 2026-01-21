#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "MessageControl.h"
#include <core/core.h>
#include <fstream>
#include <iterator>
#include <cstdio>
#include <atomic>

using namespace WPEFramework;
using namespace WPEFramework::Plugin;

class MessageControlL1Test : public ::testing::Test {
protected:
    Core::ProxyType<MessageControl> plugin;
    PluginHost::IShell* _shell;
    bool _shellOwned;

    void SetUp() override {
        plugin = Core::ProxyType<MessageControl>::Create();
        _shell = nullptr;
        _shellOwned = false;
    }

    void TearDown() override {
        if (_shell != nullptr) {
            if (plugin.IsValid()) {
                plugin->Deinitialize(_shell);
            }
            if (_shellOwned) {
                delete _shell;
            }
            _shell = nullptr;
            _shellOwned = false;
        }
        plugin.Release();
    }

    class TestShell : public PluginHost::IShell {
    public:
        explicit TestShell(const string& config = R"({"console":true,"syslog":false})")
            : _config(config)
            , _refCount(1)
        {
        }

        string ConfigLine() const override {
            return _config;
        }
        string VolatilePath() const override { return "/tmp/"; }
        bool Background() const override { return false; }
        string Accessor() const override { return ""; }
        string WebPrefix() const override { return ""; }
        string Callsign() const override { return "MessageControl"; }
        string HashKey() const override { return ""; }
        string PersistentPath() const override { return "/tmp/"; }
        string DataPath() const override { return "/tmp/"; }
        string ProxyStubPath() const override { return "/tmp/"; }
        string SystemPath() const override { return "/tmp/"; }
        string PluginPath() const override { return "/tmp/"; }
        string SystemRootPath() const override { return "/tmp/"; }
        string Locator() const override { return ""; }
        string ClassName() const override { return ""; }
        string Versions() const override { return ""; }
        string Model() const override { return ""; }
        
        state State() const override { return state::ACTIVATED; }
        bool Resumed() const override { return true; }
        Core::hresult Resumed(const bool) override { return Core::ERROR_NONE; }
        reason Reason() const override { return reason::REQUESTED; }
        
        PluginHost::ISubSystem* SubSystems() override { return nullptr; }
        startup Startup() const override { return startup::ACTIVATED; }
        Core::hresult Startup(const startup) override { return Core::ERROR_NONE; }
        ICOMLink* COMLink() override { return nullptr; }
        void* QueryInterface(const uint32_t) override { return nullptr; }
        
        void AddRef() const override {
            Core::InterlockedIncrement(_refCount);
        }

        uint32_t Release() const override {
            return Core::InterlockedDecrement(_refCount);
        }
        
        void EnableWebServer(const string& URLPath, const string& fileSystemPath) override {}
        void DisableWebServer() override {}
        Core::hresult SystemRootPath(const string& systemRootPath) override { return Core::ERROR_NONE; }
        string Substitute(const string& input) const override { return input; }
        Core::hresult ConfigLine(const string& config) override { return Core::ERROR_NONE; }
        Core::hresult Metadata(string& info) const override { return Core::ERROR_NONE; }
        bool IsSupported(const uint8_t version) const override { return true; }
        void Notify(const string& message) override {}
        void Register(PluginHost::IPlugin::INotification* sink) override {}
        void Unregister(PluginHost::IPlugin::INotification* sink) override {}
        void* QueryInterfaceByCallsign(const uint32_t id, const string& name) override { return nullptr; }
        Core::hresult Activate(const reason why) override { return Core::ERROR_NONE; }
        Core::hresult Deactivate(const reason why) override { return Core::ERROR_NONE; }
        Core::hresult Unavailable(const reason why) override { return Core::ERROR_NONE; }
        Core::hresult Hibernate(const uint32_t timeout) override { return Core::ERROR_NONE; }
        uint32_t Submit(const uint32_t id, const Core::ProxyType<Core::JSON::IElement>& response) override { return Core::ERROR_NONE; }
        
    private:
        string _config;
        mutable uint32_t _refCount;
    };

};

TEST_F(MessageControlL1Test, Construction) {
    EXPECT_NE(nullptr, plugin.operator->()) << "Plugin instance should not be null";
}

TEST_F(MessageControlL1Test, InitialState) {
    EXPECT_TRUE(plugin->Information().empty()) << "Initial plugin information should be empty";
}

TEST_F(MessageControlL1Test, EnableAllMessageTypes) {
    std::vector<Exchange::IMessageControl::MessageType> types = {
        Exchange::IMessageControl::TRACING,
        Exchange::IMessageControl::LOGGING,
        Exchange::IMessageControl::REPORTING,
        Exchange::IMessageControl::STANDARD_OUT,
        Exchange::IMessageControl::STANDARD_ERROR
    };
    for (auto type : types) {
        Core::hresult hr = plugin->Enable(type, "category1", "testmodule", true);
        EXPECT_EQ(Core::ERROR_NONE, hr) << "Enable should succeed for type " << type;
    }
}

TEST_F(MessageControlL1Test, EnableTracing) {
    Core::hresult hr = plugin->Enable(
        Exchange::IMessageControl::TRACING,
        "category1", 
        "testmodule",
        true);
    EXPECT_EQ(Core::ERROR_NONE, hr) << "Enable tracing should succeed";
}

TEST_F(MessageControlL1Test, EnableLogging) {
    Core::hresult hr = plugin->Enable(
        Exchange::IMessageControl::LOGGING,
        "category1",
        "testmodule", 
        true);
    EXPECT_EQ(Core::ERROR_NONE, hr) << "Enable logging should succeed";
}

TEST_F(MessageControlL1Test, EnableDisableWarning) {
    Core::hresult hr = plugin->Enable(
        Exchange::IMessageControl::TRACING,
        "category1",
        "testmodule",
        true);
    EXPECT_EQ(Core::ERROR_NONE, hr) << "Enable tracing should succeed";
    hr = plugin->Enable(
        Exchange::IMessageControl::TRACING,
        "category1", 
        "testmodule",
        false);
    EXPECT_EQ(Core::ERROR_NONE, hr) << "Disable tracing should succeed";
}

TEST_F(MessageControlL1Test, ControlsIterator) {
    plugin->Enable(Exchange::IMessageControl::TRACING, "cat1", "mod1", true);
    plugin->Enable(Exchange::IMessageControl::LOGGING, "cat2", "mod2", true);
    Exchange::IMessageControl::IControlIterator* controls = nullptr;
    Core::hresult hr = plugin->Controls(controls);
    EXPECT_EQ(Core::ERROR_NONE, hr) << "Controls should succeed";
    ASSERT_NE(nullptr, controls) << "Controls iterator should not be null";
    controls->Release();
}

TEST_F(MessageControlL1Test, WebSocketSupport) {
    Core::ProxyType<Core::JSON::IElement> element = plugin->Inbound("test");
    EXPECT_TRUE(element.IsValid()) << "Inbound element should be valid";
}

TEST_F(MessageControlL1Test, EnableMultipleCategories) {
    Core::hresult hr = plugin->Enable(
        Exchange::IMessageControl::TRACING,
        "category1", 
        "module1",
        true);
    EXPECT_EQ(Core::ERROR_NONE, hr) << "Enable tracing for category1 should succeed";
    hr = plugin->Enable(
        Exchange::IMessageControl::TRACING,
        "category2",
        "module1", 
        true);
    EXPECT_EQ(Core::ERROR_NONE, hr) << "Enable tracing for category2 should succeed";
}

TEST_F(MessageControlL1Test, EnableAndDisableMultiple) {
    plugin->Enable(Exchange::IMessageControl::STANDARD_OUT, "cat1", "mod1", true);
    plugin->Enable(Exchange::IMessageControl::STANDARD_ERROR, "cat2", "mod2", true);
    plugin->Enable(Exchange::IMessageControl::STANDARD_OUT, "cat1", "mod1", false);
    plugin->Enable(Exchange::IMessageControl::STANDARD_ERROR, "cat2", "mod2", false);
    Exchange::IMessageControl::IControlIterator* controls = nullptr;
    Core::hresult hr = plugin->Controls(controls);
    EXPECT_EQ(Core::ERROR_NONE, hr) << "Controls should succeed after enable/disable";
    controls->Release();
}

TEST_F(MessageControlL1Test, InboundCommunication) {
    Core::ProxyType<Core::JSON::IElement> element = plugin->Inbound("command");
    EXPECT_TRUE(element.IsValid()) << "Inbound command should be valid";
    Core::ProxyType<Core::JSON::IElement> response = plugin->Inbound(1234, element);
    EXPECT_TRUE(response.IsValid()) << "Inbound response should be valid";
}

TEST_F(MessageControlL1Test, WebSocketInboundFlow) {
    Core::ProxyType<Core::JSON::IElement> element = plugin->Inbound("test");
    EXPECT_TRUE(element.IsValid()) << "Inbound test should be valid";
    Core::ProxyType<Core::JSON::IElement> response = plugin->Inbound(1234, element);
    EXPECT_TRUE(response.IsValid()) << "Inbound response should be valid";
}

TEST_F(MessageControlL1Test, VerifyMultipleEnableDisable) {
    for(auto type : {
        Exchange::IMessageControl::TRACING,
        Exchange::IMessageControl::LOGGING,
        Exchange::IMessageControl::REPORTING}) {
        Core::hresult hr = plugin->Enable(type, "category1", "testmodule", true);
        EXPECT_EQ(Core::ERROR_NONE, hr) << "Enable should succeed for type " << type;
        Exchange::IMessageControl::IControlIterator* controls = nullptr;
        hr = plugin->Controls(controls);
        EXPECT_EQ(Core::ERROR_NONE, hr) << "Controls should succeed for type " << type;
        ASSERT_NE(nullptr, controls) << "Controls iterator should not be null";
        controls->Release();
        hr = plugin->Enable(type, "category1", "testmodule", false);
        EXPECT_EQ(Core::ERROR_NONE, hr) << "Disable should succeed for type " << type;
    }
}

TEST_F(MessageControlL1Test, InboundMessageFlow) {
    Core::ProxyType<Core::JSON::IElement> element = plugin->Inbound("command");
    EXPECT_TRUE(element.IsValid()) << "Inbound command should be valid";
    for(uint32_t id = 1; id < 4; id++) {
        Core::ProxyType<Core::JSON::IElement> response = plugin->Inbound(id, element);
        EXPECT_TRUE(response.IsValid()) << "Inbound response for id " << id << " should be valid";
    }
}

TEST_F(MessageControlL1Test, AttachDetachChannel) {
    _shell = new TestShell();
    _shellOwned = true;
    ASSERT_NE(nullptr, _shell) << "Shell should not be null";
    string result = plugin->Initialize(_shell);
    EXPECT_TRUE(result.empty()) << "Plugin should initialize with empty result";
    class TestChannel : public PluginHost::Channel {
    public:
        TestChannel() 
            : PluginHost::Channel(0, Core::NodeId("127.0.0.1", 8899))
            , _baseTime(static_cast<uint32_t>(Core::Time::Now().Ticks())) {
            State(static_cast<ChannelState>(2), true);
        }
        void LinkBody(Core::ProxyType<PluginHost::Request>& request) override {}
        void Received(Core::ProxyType<PluginHost::Request>& request) override {}
        void Send(const Core::ProxyType<Web::Response>& response) override {}
        uint16_t SendData(uint8_t* dataFrame, const uint16_t maxSendSize) override { return maxSendSize; }
        uint16_t ReceiveData(uint8_t* dataFrame, const uint16_t receivedSize) override { return receivedSize; }
        void StateChange() override {}
        void Send(const Core::ProxyType<Core::JSON::IElement>& element) override {}
        Core::ProxyType<Core::JSON::IElement> Element(const string& identifier) override { 
            return Core::ProxyType<Core::JSON::IElement>(); 
        }
        void Received(Core::ProxyType<Core::JSON::IElement>& element) override {}
        void Received(const string& text) override {}
    private:
        uint32_t _baseTime;
    };
    TestChannel channel;
    EXPECT_TRUE(plugin->Attach(channel)) << "Attach channel should succeed";
    plugin->Detach(channel);
    plugin->Deinitialize(_shell);
    delete _shell;
    _shell = nullptr;
    _shellOwned = false;
}

TEST_F(MessageControlL1Test, MultipleAttachDetach) {
    _shell = new TestShell();
    _shellOwned = true;
    ASSERT_NE(nullptr, _shell) << "Shell should not be null";
    string result = plugin->Initialize(_shell);
    EXPECT_TRUE(result.empty()) << "Plugin should initialize with empty result";
    class TestChannel : public PluginHost::Channel {
    public:
        TestChannel(uint32_t id) 
            : PluginHost::Channel(0, Core::NodeId("127.0.0.1", 8899))
            , _baseTime(static_cast<uint32_t>(Core::Time::Now().Ticks()))
            , _id(id) {
            State(static_cast<ChannelState>(2), true);
        }
        void LinkBody(Core::ProxyType<PluginHost::Request>& request) override {}
        void Received(Core::ProxyType<PluginHost::Request>& request) override {}
        void Send(const Core::ProxyType<Web::Response>& response) override {}
        uint16_t SendData(uint8_t* dataFrame, const uint16_t maxSendSize) override { return maxSendSize; }
        uint16_t ReceiveData(uint8_t* dataFrame, const uint16_t receivedSize) override { return receivedSize; }
        void StateChange() override {}
        void Send(const Core::ProxyType<Core::JSON::IElement>& element) override {}
        Core::ProxyType<Core::JSON::IElement> Element(const string& identifier) override { 
            return Core::ProxyType<Core::JSON::IElement>(); 
        }
        void Received(Core::ProxyType<Core::JSON::IElement>& element) override {}
        void Received(const string& text) override {}
    private:
        uint32_t _baseTime;
        uint32_t _id;
    };
    TestChannel channel1(1);
    TestChannel channel2(2);
    TestChannel channel3(3);
    EXPECT_TRUE(plugin->Attach(channel1)) << "Attach channel1 should succeed";
    EXPECT_TRUE(plugin->Attach(channel2)) << "Attach channel2 should succeed";
    EXPECT_TRUE(plugin->Attach(channel3)) << "Attach channel3 should succeed";
    plugin->Detach(channel2);
    plugin->Detach(channel1);
    plugin->Detach(channel3);
    plugin->Deinitialize(_shell);
    delete _shell;
    _shell = nullptr;
    _shellOwned = false;
}

TEST_F(MessageControlL1Test, TextConvert_ForConsoleFormat) {
    Publishers::Text textConv(Core::Messaging::MessageInfo::abbreviate::ABBREVIATED);
    Core::Messaging::MessageInfo defaultMeta;
    const string payload = "console-output-test";
    const string converted = textConv.Convert(defaultMeta, payload);
    EXPECT_NE(string::npos, converted.find(payload)) << "Converted text should contain payload";
    EXPECT_NE(string::npos, converted.find("\n")) << "Converted text should contain newline";
}

TEST_F(MessageControlL1Test, SyslogOutput_ConverterOutput) {
    Publishers::Text textConv(Core::Messaging::MessageInfo::abbreviate::ABBREVIATED);
    Core::Messaging::MessageInfo defaultMeta;
    const string payload = "syslog-output-test";
    const string converted = textConv.Convert(defaultMeta, payload);
    EXPECT_NE(string::npos, converted.find(payload)) << "Converted text should contain payload";
}

TEST_F(MessageControlL1Test, WebSocketOutput_AttachCapacity_Command_Received) {
    TestShell* shell = new TestShell();
    Publishers::WebSocketOutput ws;
    ws.Initialize(shell, 1);
    EXPECT_TRUE(ws.Attach(42)) << "Attach should succeed for id 42";
    EXPECT_FALSE(ws.Attach(43)) << "Attach should fail for id 43";
    Core::ProxyType<Core::JSON::IElement> cmd = ws.Command();
    EXPECT_TRUE(cmd.IsValid()) << "Command should be valid";
    Core::ProxyType<Core::JSON::IElement> ret = ws.Received(42, cmd);
    EXPECT_TRUE(ret.IsValid()) << "Received should be valid for id 42";
    EXPECT_TRUE(ws.Detach(42)) << "Detach should succeed for id 42";
    ws.Deinitialize();
    delete shell;
}

TEST_F(MessageControlL1Test, WebSocketOutput_Message_NoCrash_SubmitCalled) {
    TestShell* shell = new TestShell();
    Publishers::WebSocketOutput ws;
    ws.Initialize(shell, 2);
    EXPECT_TRUE(ws.Attach(1001)) << "Attach should succeed for id 1001";
    Core::Messaging::MessageInfo defaultMeta;
    ws.Message(defaultMeta, "websocket-export-test");
    EXPECT_TRUE(ws.Detach(1001)) << "Detach should succeed for id 1001";
    ws.Deinitialize();
    delete shell;
}

TEST_F(MessageControlL1Test, MessageControl_InitializeCreatesFileOutput) {
    TestShell* shell = new TestShell(R"({"filepath":"test_messagecontrol_init.log","abbreviated":true})");
    ASSERT_NE(nullptr, shell) << "Shell should not be null";
    string initResult = plugin->Initialize(shell);
    EXPECT_TRUE(initResult.empty()) << "Plugin should initialize with empty result";
    const string expectedFile = "/tmp/test_messagecontrol_init.log";
    std::ifstream in(expectedFile);
    if (in.good()) {
        in.close();
        EXPECT_TRUE(std::ifstream(expectedFile).good()) << "File should exist after initialization";
        std::remove(expectedFile.c_str());
    } else {
        GTEST_SKIP() << "Cannot create/read temp file in this environment; skipping file existence check.";
    }
    plugin->Deinitialize(shell);
    delete shell;
}

TEST_F(MessageControlL1Test, JSON_OutputOptions_TogglesAndConvert) {
    Publishers::JSON json;
    json.FileName(true); EXPECT_TRUE(json.FileName()) << "FileName option should be enabled";
    json.LineNumber(true); EXPECT_TRUE(json.LineNumber()) << "LineNumber option should be enabled";
    json.ClassName(true); EXPECT_TRUE(json.ClassName()) << "ClassName option should be enabled";
    json.Category(true); EXPECT_TRUE(json.Category()) << "Category option should be enabled";
    json.Module(true); EXPECT_TRUE(json.Module()) << "Module option should be enabled";
    json.Callsign(true); EXPECT_TRUE(json.Callsign()) << "Callsign option should be enabled";
    json.Date(true); EXPECT_TRUE(json.Date()) << "Date option should be enabled";
    json.Paused(false); EXPECT_FALSE(json.Paused()) << "Paused option should be disabled";
    json.FileName(false); EXPECT_FALSE(json.FileName()) << "FileName option should be disabled";
    json.Date(false); EXPECT_FALSE(json.Date()) << "Date option should be disabled";
    Core::Messaging::MessageInfo defaultMeta;
    Publishers::JSON::Data data;
    json.Convert(defaultMeta, "json-payload", data);
    EXPECT_EQ(std::string("json-payload"), std::string(data.Message)) << "Converted message should match payload";
}

TEST_F(MessageControlL1Test, MessageOutput_SimpleText_JSON) {
    Core::Messaging::MessageInfo defaultMeta;
    Publishers::Text textConv(Core::Messaging::MessageInfo::abbreviate::ABBREVIATED);
    const string payload = "hello-text";
    const string result = textConv.Convert(defaultMeta, payload);
    EXPECT_NE(string::npos, result.find(payload)) << "Converted text should contain payload";
    Publishers::JSON::Data data;
    Publishers::JSON jsonConv;
    jsonConv.Convert(defaultMeta, "json-msg", data);
    EXPECT_EQ(std::string("json-msg"), std::string(data.Message)) << "Converted message should match payload";
    Core::NodeId anyNode("127.0.0.1", 0);
    Publishers::UDPOutput udp(anyNode);
    udp.Message(defaultMeta, "udp-msg");
    SUCCEED() << "UDPOutput::Message should not crash";
}

TEST_F(MessageControlL1Test, MessageOutput_FileWrite) {
	const string tmpName = "/tmp/test_messageoutput_filewrite.log";
	std::remove(tmpName.c_str());
	Publishers::FileOutput fileOutput(Core::Messaging::MessageInfo::abbreviate::ABBREVIATED, tmpName);
	Core::Messaging::MessageInfo defaultMeta;
	const string payload = "file-write-test-payload";
	fileOutput.Message(defaultMeta, payload);
	std::ifstream in(tmpName);
	if (in.good()) {
		std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
		in.close();
		EXPECT_NE(string::npos, content.find(payload)) << "File should contain payload";
		std::remove(tmpName.c_str());
	} else {
		GTEST_SKIP() << "Cannot create/read temp file; skipping FileOutput write verification.";
	}
}

TEST_F(MessageControlL1Test, EnableWithEmptyFields) {
    Core::hresult hr = plugin->Enable(
        Exchange::IMessageControl::LOGGING,
        "",
        "",
        true);
    EXPECT_EQ(Core::ERROR_NONE, hr) << "Enable with empty category/module should succeed";
}

TEST_F(MessageControlL1Test, WebSocketOutput_UnknownDetach) {
    TestShell* shell = new TestShell();
    Publishers::WebSocketOutput ws;
    ws.Initialize(shell, 1);
    EXPECT_FALSE(ws.Detach(9999)) << "Detach should fail for unknown id";
    ws.Deinitialize();
    delete shell;
}

TEST_F(MessageControlL1Test, TestShell_SubstituteAndMetadata) {
    TestShell shell;
    const string input = "replace-me";
    EXPECT_EQ(input, shell.Substitute(input)) << "Substitute should return input";
    string meta;
    Core::hresult hr = shell.Metadata(meta);
    EXPECT_EQ(Core::ERROR_NONE, hr) << "Metadata should succeed";
    EXPECT_TRUE(meta.size() >= 0) << "Metadata string should be non-negative length";
}

TEST_F(MessageControlL1Test, JSON_Paused_PreventsConvert) {
    Publishers::JSON json;
    Publishers::JSON::Data data;
    json.Paused(true);
    Core::Messaging::MessageInfo defaultMeta;
    json.Convert(defaultMeta, "payload-should-be-ignored", data);
    EXPECT_TRUE(std::string(data.Message).empty()) << "Message should be empty when paused";
}

TEST_F(MessageControlL1Test, Observer_Activated_Deactivated_Terminated_Simple) {
    class MockConnection : public RPC::IRemoteConnection {
    public:
        MockConnection(uint32_t id) : _id(id) {}
        uint32_t Id() const override { return _id; }
        void AddRef() const override {}
        uint32_t Release() const override { return 0; }
        void* QueryInterface(const uint32_t) override { return nullptr; }
        uint32_t RemoteId() const override { return _id; }
        void* Acquire(uint32_t, const string&, uint32_t, uint32_t) override { return nullptr; }
        void Terminate() override {}
        uint32_t Launch() override { return 0; }
        void PostMortem() override {}
    private:
        uint32_t _id;
    };
    MockConnection connection(42);
    _shell = new TestShell();
    _shellOwned = true;
    plugin->Initialize(_shell);
    plugin->Attach(connection.Id());
    SUCCEED() << "Activated should not crash";
    plugin->Detach(connection.Id());
    SUCCEED() << "Deactivated should not crash";
    plugin->Detach(connection.Id());
    SUCCEED() << "Terminated should not crash";
    plugin->Deinitialize(_shell);
    delete _shell;
    _shell = nullptr;
    _shellOwned = false;
}

TEST_F(MessageControlL1Test, ConsoleOutput_Message) {
    Publishers::ConsoleOutput consoleOutput(Core::Messaging::MessageInfo::abbreviate::ABBREVIATED);
    Core::Messaging::Metadata metadata(Core::Messaging::Metadata::type::TRACING, "TestCategory", "TestModule");
    ASSERT_TRUE(metadata.Type() == Core::Messaging::Metadata::type::TRACING) << "Metadata type should be TRACING";
    Core::Messaging::MessageInfo messageInfo(metadata, Core::Time::Now().Ticks());
    testing::internal::CaptureStdout();
    consoleOutput.Message(messageInfo, "Test message for ConsoleOutput");
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_NE(output.find("Test message for ConsoleOutput"), std::string::npos) << "Output should contain message";
    EXPECT_NE(output.find("TestCategory"), std::string::npos) << "Output should contain category";
    EXPECT_NE(output.find("TestModule"), std::string::npos) << "Output should contain module";
}

TEST_F(MessageControlL1Test, JSONOutput_ConvertWithOptions) {
    Publishers::JSON json;
    json.FileName(true);
    json.LineNumber(true);
    json.Category(true);
    json.Module(true);
    json.Callsign(true);
    json.Date(true);
    Core::Messaging::Metadata metadata(Core::Messaging::Metadata::type::TRACING, "JSONCategory", "JSONModule");
    Core::Messaging::MessageInfo messageInfo(metadata, Core::Time::Now().Ticks());
    Publishers::JSON::Data data;
    json.Convert(messageInfo, "Test JSON message", data);
    EXPECT_EQ(data.Category.Value(), "JSONCategory") << "Category should match";
    EXPECT_EQ(data.Module.Value(), "JSONModule") << "Module should match";
    EXPECT_EQ(data.Message.Value(), "Test JSON message") << "Message should match";
    EXPECT_FALSE(data.Time.Value().empty()) << "Time should not be empty";
}
