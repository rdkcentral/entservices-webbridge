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
#include "COMLinkMock.h"

#include "Telemetry.h"
#include "TelemetryImplementation.h"
#include "TelemetryMock.h"
#include "WorkerPoolImplementation.h"
#include "FactoriesImplementation.h"
#include "RfcApiMock.h"
#include "ServiceMock.h"
#include "RBusMock.h"
#include "IarmBusMock.h"
#include "PowerManagerMock.h"
#include "WrapsMock.h"
#include "ThunderPortability.h"

namespace {
const string profileFN = _T("/tmp/DefaultProfile.json");
const string t2PpersistentFolder = _T("/tmp/.t2reportprofiles/");
const uint8_t profileContent[] = "{\"profile\":\"default\"}";
}

using namespace WPEFramework;
using ::testing::NiceMock;

class TelemetryTest : public ::testing::Test {
protected:
    NiceMock<ServiceMock> service;
    Core::ProxyType<Plugin::Telemetry> plugin;
    Core::JSONRPC::Handler& handler;
    DECL_CORE_JSONRPC_CONX connection;
    NiceMock<COMLinkMock> comLinkMock;
    Core::ProxyType<WorkerPoolImplementation> workerPool;
    Core::ProxyType<Plugin::TelemetryImplementation> TelemetryImpl;
    Exchange::ITelemetry::INotification *TelemetryNotification = nullptr;
    string response;
    FactoriesImplementation factoriesImplementation;
    PLUGINHOST_DISPATCHER *dispatcher;
    Core::JSONRPC::Message message;
    ServiceMock  *p_serviceMock  = nullptr;
    WrapsImplMock* p_wrapsImplMock = nullptr;
    TelemetryApiImplMock* p_telemetryApiImplMock = nullptr;
    RfcApiImplMock* p_rfcApiImplMock = nullptr ;
    RBusApiImplMock  *p_rBusApiImplMock = nullptr;

    TelemetryTest()
        : plugin(Core::ProxyType<Plugin::Telemetry>::Create())
        , handler(*plugin)
        , INIT_CONX(1, 0)
	, workerPool(Core::ProxyType<WorkerPoolImplementation>::Create(
            2, Core::Thread::DefaultStackSize(), 16))
    {

#ifdef USE_THUNDER_R4
        Core::Directory(t2PpersistentFolder.c_str()).Destroy();
#else
        Core::Directory(t2PpersistentFolder.c_str()).Destroy(true);
#endif /*USE_THUNDER_R4 */

	p_serviceMock = new NiceMock <ServiceMock>;

        p_telemetryApiImplMock  = new NiceMock <TelemetryApiImplMock>;
        TelemetryApi::setImpl(p_telemetryApiImplMock);

	p_wrapsImplMock = new NiceMock<WrapsImplMock>;
        Wraps::setImpl(p_wrapsImplMock);

	p_rfcApiImplMock = new NiceMock <RfcApiImplMock>;
        RfcApi::setImpl(p_rfcApiImplMock);

	
	PluginHost::IFactories::Assign(&factoriesImplementation);

        dispatcher = static_cast<PLUGINHOST_DISPATCHER*>(
           plugin->QueryInterface(PLUGINHOST_DISPATCHER_ID));
        dispatcher->Activate(&service);

        p_rBusApiImplMock = new NiceMock <RBusApiImplMock>;
        RBusApi::setImpl(p_rBusApiImplMock);

	ON_CALL(*p_telemetryApiImplMock, Register(::testing::_))
            .WillByDefault(::testing::Invoke(
                [&](Exchange::ITelemetry::INotification* notification) {
                    TelemetryNotification = notification;
		    return Core::ERROR_NONE;
		     ;
                }));

        ON_CALL(comLinkMock, Instantiate(::testing::_, ::testing::_, ::testing::_))
            .WillByDefault(::testing::Invoke(
                [&](const RPC::Object& object, const uint32_t waitTime, uint32_t& connectionId) {
                    TelemetryImpl = Core::ProxyType<Plugin::TelemetryImplementation>::Create();
                    return &TelemetryImpl;
                }));

	Core::IWorkerPool::Assign(&(*workerPool));
        workerPool->Run();

        plugin->Initialize(&service);
    }

    virtual ~TelemetryTest() override
    {
	
	plugin->Deinitialize(&service);

        Core::IWorkerPool::Assign(nullptr);
        workerPool.Release();

	if (p_serviceMock != nullptr)
        {
            delete p_serviceMock;
            p_serviceMock = nullptr;
        }

	TelemetryApi::setImpl(nullptr);
        if (p_telemetryApiImplMock != nullptr) {
            delete p_telemetryApiImplMock;
            p_telemetryApiImplMock = nullptr;
        }

        Wraps::setImpl(nullptr);
        if (p_wrapsImplMock != nullptr) {
            delete p_wrapsImplMock;
            p_wrapsImplMock = nullptr;
        }

	RfcApi::setImpl(nullptr);
        if (p_rfcApiImplMock != nullptr)
        {
            delete p_rfcApiImplMock;
            p_rfcApiImplMock = nullptr;
        }

	RBusApi::setImpl(nullptr);
        if (p_rBusApiImplMock != nullptr)
        {
            delete p_rBusApiImplMock;
            p_rBusApiImplMock = nullptr;
        }

        dispatcher->Deactivate();
        dispatcher->Release();

        PluginHost::IFactories::Assign(nullptr);
    }
};

TEST_F(TelemetryTest, RegisteredMethods)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("setReportProfileStatus")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("logApplicationEvent")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("uploadReport")));
}

TEST_F(TelemetryTest, InitializeDefaultProfile)
{
    ON_CALL(service, ConfigLine())
        .WillByDefault(
            ::testing::Return("{"
                              "\"t2PersistentFolder\":\"/tmp/.t2reportprofiles/\","
                              "\"defaultProfilesFile\":\"/tmp/DefaultProfile.json\""
                              "}"));
    ON_CALL(*p_rfcApiImplMock, setRFCParameter(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillByDefault(::testing::Invoke(
            [](char* pcCallerID, const char* pcParameterName, const char* pcParameterValue, DATA_TYPE eDataType) {
                EXPECT_EQ(string(pcCallerID), _T("Telemetry"));
                EXPECT_EQ(string(pcParameterName), _T("Device.X_RDKCENTRAL-COM_T2.ReportProfiles"));
                EXPECT_EQ(string(pcParameterValue), _T("{\\\"profile\\\":\\\"default\\\"}"));
                EXPECT_EQ(eDataType, WDMP_STRING);

                return WDMP_SUCCESS;
            }));
    {
        Core::Directory(t2PpersistentFolder.c_str()).CreatePath();
        Core::File file(profileFN);
        file.Create();
        file.Write(profileContent, sizeof(profileContent));
    }

}

TEST_F(TelemetryTest, InitializeDefaultProfileRFCFailure)
{
    ON_CALL(service, ConfigLine())
        .WillByDefault(
            ::testing::Return("{"
                              "\"t2PersistentFolder\":\"/tmp/.t2reportprofiles/\","
                              "\"defaultProfilesFile\":\"/tmp/DefaultProfile.json\""
                              "}"));

    ON_CALL(*p_rfcApiImplMock, setRFCParameter(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillByDefault(::testing::Invoke(
            [](char* pcCallerID, const char* pcParameterName, const char* pcParameterValue, DATA_TYPE eDataType) {
                return WDMP_FAILURE;
            }));

    {
        Core::Directory(t2PpersistentFolder.c_str()).CreatePath();
        Core::File file(profileFN);
        file.Create();
        file.Write(profileContent, sizeof(profileContent));
    }
}

TEST_F(TelemetryTest, InitializeZeroSizeDefaultProfile)
{
    ON_CALL(service, ConfigLine())
        .WillByDefault(
            ::testing::Return("{"
                              "\"t2PersistentFolder\":\"/tmp/.t2reportprofiles/\","
                              "\"defaultProfilesFile\":\"/tmp/DefaultProfile.json\""
                              "}"));
    {
        Core::Directory(t2PpersistentFolder.c_str()).CreatePath();
        Core::File file(profileFN);
        file.Create();
    }
}

TEST_F(TelemetryTest, InitializePersistentFolder)
{
    ON_CALL(service, ConfigLine())
        .WillByDefault(
            ::testing::Return("{"
                              "\"t2PersistentFolder\":\"/tmp/.t2reportprofiles/\","
                              "\"defaultProfilesFile\":\"/tmp/DefaultProfile.json\""
                              "}"));
    {
        Core::Directory(t2PpersistentFolder.c_str()).CreatePath();
        Core::File file(t2PpersistentFolder + "SomeReport");
        file.Create();
    }

}

TEST_F(TelemetryTest, Plugin)
{
    ON_CALL(service, ConfigLine())
        .WillByDefault(
            ::testing::Return("{}"));
    EXPECT_CALL(*p_rfcApiImplMock, setRFCParameter(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(3)
        .WillOnce(::testing::Invoke(
            [](char* pcCallerID, const char* pcParameterName, const char* pcParameterValue, DATA_TYPE eDataType) {
                return WDMP_FAILURE;
            }))
        .WillOnce(::testing::Invoke(
            [](char* pcCallerID, const char* pcParameterName, const char* pcParameterValue, DATA_TYPE eDataType) {
                EXPECT_EQ(string(pcCallerID), _T("Telemetry"));
                EXPECT_EQ(string(pcParameterName), _T("Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.Telemetry.FTUEReport.Enable"));
                EXPECT_EQ(string(pcParameterValue), _T("false"));
                EXPECT_EQ(eDataType, WDMP_BOOLEAN);

                return WDMP_SUCCESS;
            }))
        .WillOnce(::testing::Invoke(
            [](char* pcCallerID, const char* pcParameterName, const char* pcParameterValue, DATA_TYPE eDataType) {
                EXPECT_EQ(string(pcCallerID), _T("Telemetry"));
                EXPECT_EQ(string(pcParameterName), _T("Device.DeviceInfo.X_RDKCENTRAL-COM_RFC.Feature.Telemetry.FTUEReport.Enable"));
                EXPECT_EQ(string(pcParameterValue), _T("true"));
                EXPECT_EQ(eDataType, WDMP_BOOLEAN);

                return WDMP_SUCCESS;
            }));
    EXPECT_CALL(*p_telemetryApiImplMock, t2_event_s(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](char* marker, char* value) {
                EXPECT_EQ(string(marker), _T("NAME"));
                EXPECT_EQ(string(value), _T("VALUE"));
                return T2ERROR_SUCCESS;
            }));

    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("setReportProfileStatus"), _T("{}"), response));
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("setReportProfileStatus"), _T("{\"status\":\"wrongvalue\"}"), response));
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("setReportProfileStatus"), _T("{\"status\":\"STARTED\"}"), response));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setReportProfileStatus"), _T("{\"status\":\"STARTED\"}"), response));
    EXPECT_EQ(response, _T(""));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("setReportProfileStatus"), _T("{\"status\":\"COMPLETE\"}"), response));
    EXPECT_EQ(response, _T(""));
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("logApplicationEvent"), _T("{\"eventName\":\"NAME\"}"), response));
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("logApplicationEvent"), _T("{\"eventValue\":\"VALUE\"}"), response));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("logApplicationEvent"), _T("{\"eventName\":\"NAME\", \"eventValue\":\"VALUE\"}"), response));
    EXPECT_EQ(response, _T(""));

}

TEST_F(TelemetryTest, uploadLogsRbusOpenFailure)
{
    EXPECT_CALL(*p_rBusApiImplMock, rbus_open(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](rbusHandle_t* handle, char const* componentName) {
                EXPECT_TRUE(nullptr != handle);
                EXPECT_EQ(string(componentName), _T("TelemetryThunderPlugin"));
                return RBUS_ERROR_BUS_ERROR;
            }));

    EXPECT_EQ(Core::ERROR_OPENING_FAILED, handler.Invoke(connection, _T("uploadReport"), _T("{}"), response));
}

TEST_F(TelemetryTest, uploadLogsRbusMethodFailure)
{
    ON_CALL(*p_rBusApiImplMock, rbus_open(::testing::_, ::testing::_))
        .WillByDefault(
            ::testing::Return(RBUS_ERROR_SUCCESS));

    EXPECT_CALL(*p_rBusApiImplMock, rbusMethod_InvokeAsync(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [&](rbusHandle_t handle, char const* methodName, rbusObject_t inParams, rbusMethodAsyncRespHandler_t callback, int timeout) {
                return RBUS_ERROR_BUS_ERROR;
            }));

    ON_CALL(*p_rBusApiImplMock, rbus_close(::testing::_))
        .WillByDefault(
            ::testing::Return(RBUS_ERROR_SUCCESS));

    EXPECT_EQ(Core::ERROR_RPC_CALL_FAILED, handler.Invoke(connection, _T("uploadReport"), _T("{}"), response));

}

TEST_F(TelemetryTest, uploadLogsCallbackFailed)
{
    Core::Event onReportUpload(false, true);

    struct _rbusObject rbObject;

    ON_CALL(*p_rBusApiImplMock, rbus_open(::testing::_, ::testing::_))
        .WillByDefault(
            ::testing::Return(RBUS_ERROR_SUCCESS));

    EXPECT_CALL(*p_rBusApiImplMock, rbusMethod_InvokeAsync(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [&](rbusHandle_t handle, char const* methodName, rbusObject_t inParams, rbusMethodAsyncRespHandler_t callback, int timeout) {
                callback(handle, methodName, RBUS_ERROR_BUS_ERROR, &rbObject);

                return RBUS_ERROR_SUCCESS;
            }));

    ON_CALL(*p_rBusApiImplMock, rbus_close(::testing::_))
        .WillByDefault(
            ::testing::Return(RBUS_ERROR_SUCCESS));

    EXPECT_CALL(service, Submit(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [&](const uint32_t, const Core::ProxyType<Core::JSON::IElement>& json) {
                string text;
                EXPECT_TRUE(json->ToString(text));

                EXPECT_EQ(text, 
                    "{\"jsonrpc\":\"2.0\",\"method\":\"org.rdk.Telemetry.onReportUpload\",\"params\":{\"telemetryUploadStatus\":\"{\\\"telemetryUploadStatus\\\":\\\"UPLOAD_FAILURE\\\"}\"}}"
                );

                onReportUpload.SetEvent();

                return Core::ERROR_NONE;
            }));

    EVENT_SUBSCRIBE(0, _T("onReportUpload"), _T("org.rdk.Telemetry"), message);

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("uploadReport"), _T("{}"), response));
    EXPECT_EQ(Core::ERROR_NONE, onReportUpload.Lock());

    EVENT_UNSUBSCRIBE(0, _T("onReportUpload"), _T("org.rdk.Telemetry"), message);
}

TEST_F(TelemetryTest, uploadLogsGetValueFailure)
{
    Core::Event onReportUpload(false, true);

    struct _rbusObject rbObject;

    ON_CALL(*p_rBusApiImplMock, rbus_open(::testing::_, ::testing::_))
        .WillByDefault(
            ::testing::Return(RBUS_ERROR_SUCCESS));

    EXPECT_CALL(*p_rBusApiImplMock, rbusObject_GetValue(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [&](rbusObject_t object, char const* name) {
                EXPECT_EQ(object, &rbObject);
                EXPECT_EQ(string(name), _T("UPLOAD_STATUS"));
                return nullptr;
            }));

    ON_CALL(*p_rBusApiImplMock, rbusMethod_InvokeAsync(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillByDefault(::testing::Invoke(
            [&](rbusHandle_t handle, char const* methodName, rbusObject_t inParams, rbusMethodAsyncRespHandler_t callback, int timeout) {
                callback(handle, methodName, RBUS_ERROR_SUCCESS, &rbObject);

                return RBUS_ERROR_SUCCESS;
            }));

    ON_CALL(*p_rBusApiImplMock, rbus_close(::testing::_))
        .WillByDefault(
            ::testing::Return(RBUS_ERROR_SUCCESS));

    EXPECT_CALL(service, Submit(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [&](const uint32_t, const Core::ProxyType<Core::JSON::IElement>& json) {
                string text;
                EXPECT_TRUE(json->ToString(text));

                EXPECT_EQ(text, 
                    "{\"jsonrpc\":\"2.0\",\"method\":\"org.rdk.Telemetry.onReportUpload\",\"params\":{\"telemetryUploadStatus\":\"{\\\"telemetryUploadStatus\\\":\\\"UPLOAD_FAILURE\\\"}\"}}"
                );

                onReportUpload.SetEvent();

                return Core::ERROR_NONE;
            }));

    EVENT_SUBSCRIBE(0, _T("onReportUpload"), _T("org.rdk.Telemetry"), message);

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("uploadReport"), _T("{}"), response));
    EXPECT_EQ(Core::ERROR_NONE, onReportUpload.Lock());

    EVENT_UNSUBSCRIBE(0, _T("onReportUpload"), _T("org.rdk.Telemetry"), message);
}

TEST_F(TelemetryTest, uploadLogsFailure)
{
    Core::Event onReportUpload(false, true);

    struct _rbusObject rbObject;
    struct _rbusValue rbValue;

    ON_CALL(*p_rBusApiImplMock, rbus_open(::testing::_, ::testing::_))
        .WillByDefault(
            ::testing::Return(RBUS_ERROR_SUCCESS));

    EXPECT_CALL(*p_rBusApiImplMock, rbusValue_GetString(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [&](rbusValue_t value, int* len) {
                EXPECT_EQ(value, &rbValue);
                return "FAILURE";
            }));

    ON_CALL(*p_rBusApiImplMock, rbusObject_GetValue(::testing::_, ::testing::_))
        .WillByDefault(::testing::Invoke(
            [&](rbusObject_t object, char const* name) {
                EXPECT_EQ(object, &rbObject);
                EXPECT_EQ(string(name), _T("UPLOAD_STATUS"));
                return &rbValue;
            }));

    EXPECT_CALL(*p_rBusApiImplMock, rbusMethod_InvokeAsync(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [&](rbusHandle_t handle, char const* methodName, rbusObject_t inParams, rbusMethodAsyncRespHandler_t callback, int timeout) {
                callback(handle, methodName, RBUS_ERROR_SUCCESS, &rbObject);

                return RBUS_ERROR_SUCCESS;
            }));

    ON_CALL(*p_rBusApiImplMock, rbus_close(::testing::_))
        .WillByDefault(
            ::testing::Return(RBUS_ERROR_SUCCESS));

    EXPECT_CALL(service, Submit(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [&](const uint32_t, const Core::ProxyType<Core::JSON::IElement>& json) {
                string text;
                EXPECT_TRUE(json->ToString(text));

                EXPECT_EQ(text,
                    "{\"jsonrpc\":\"2.0\",\"method\":\"org.rdk.Telemetry.onReportUpload\",\"params\":{\"telemetryUploadStatus\":\"{\\\"telemetryUploadStatus\\\":\\\"UPLOAD_FAILURE\\\"}\"}}"
                );

                onReportUpload.SetEvent();

                return Core::ERROR_NONE;
            }));

    EVENT_SUBSCRIBE(0, _T("onReportUpload"), _T("org.rdk.Telemetry"), message);

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("uploadReport"), _T("{}"), response));
    EXPECT_EQ(Core::ERROR_NONE, onReportUpload.Lock());

    EVENT_UNSUBSCRIBE(0, _T("onReportUpload"), _T("org.rdk.Telemetry"), message);
}

TEST_F(TelemetryTest, uploadLogs)
{
    Core::Event onReportUpload(false, true);

    struct _rbusObject rbObject;
    struct _rbusValue rbValue;

    ON_CALL(*p_rBusApiImplMock, rbus_open(::testing::_, ::testing::_))
        .WillByDefault(
            ::testing::Return(RBUS_ERROR_SUCCESS));

    ON_CALL(*p_rBusApiImplMock, rbusValue_GetString(::testing::_, ::testing::_))
        .WillByDefault(
            ::testing::Return( "SUCCESS"));

    ON_CALL(*p_rBusApiImplMock, rbusObject_GetValue(::testing::_, ::testing::_))
        .WillByDefault(::testing::Invoke(
            [&](rbusObject_t object, char const* name) {
                EXPECT_EQ(object, &rbObject);
                EXPECT_EQ(string(name), _T("UPLOAD_STATUS"));
                return &rbValue;
            }));

    EXPECT_CALL(*p_rBusApiImplMock, rbusMethod_InvokeAsync(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [&](rbusHandle_t handle, char const* methodName, rbusObject_t inParams, rbusMethodAsyncRespHandler_t callback, int timeout) {
                callback(handle, methodName, RBUS_ERROR_SUCCESS, &rbObject);

                return RBUS_ERROR_SUCCESS;
            }));

    ON_CALL(*p_rBusApiImplMock, rbus_close(::testing::_))
        .WillByDefault(
            ::testing::Return(RBUS_ERROR_SUCCESS));

    EXPECT_CALL(service, Submit(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [&](const uint32_t, const Core::ProxyType<Core::JSON::IElement>& json) {
                string text;
                EXPECT_TRUE(json->ToString(text));

                EXPECT_EQ(text,
                    "{\"jsonrpc\":\"2.0\",\"method\":\"org.rdk.Telemetry.onReportUpload\",\"params\":{\"telemetryUploadStatus\":\"{\\\"telemetryUploadStatus\\\":\\\"UPLOAD_SUCCESS\\\"}\"}}"
                );

                onReportUpload.SetEvent();

                return Core::ERROR_NONE;
            }));

    EVENT_SUBSCRIBE(0, _T("onReportUpload"), _T("org.rdk.Telemetry"), message);

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("uploadReport"), _T("{}"), response));
    EXPECT_EQ(Core::ERROR_NONE, onReportUpload.Lock());

    EVENT_UNSUBSCRIBE(0, _T("onReportUpload"), _T("org.rdk.Telemetry"), message);
}
