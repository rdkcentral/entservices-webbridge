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
#include <mntent.h>
#include <fstream>
#include "USBDevice.h"
#include "USBDeviceImplementation.h"
#include "libUSBMock.h"
#include "ServiceMock.h"
#include "FactoriesImplementation.h"
#include <fstream> // Added for file creation
#include <string>
#include <vector>
#include <cstdio>
#include "COMLinkMock.h"
#include "WorkerPoolImplementation.h"
#include "WrapsMock.h"
#include "secure_wrappermock.h"
#include "ThunderPortability.h"

#define TEST_LOG(x, ...) fprintf(stderr, "\033[1;32m[%s:%d](%s)<PID:%d><TID:%d>" x "\n\033[0m", __FILE__, __LINE__, __FUNCTION__, getpid(), gettid(), ##__VA_ARGS__); fflush(stderr);

using ::testing::NiceMock;
using namespace WPEFramework;

#define MOCK_USB_DEVICE_BUS_NUMBER_1    100
#define MOCK_USB_DEVICE_ADDRESS_1       001
#define MOCK_USB_DEVICE_PORT_1          123

#define MOCK_USB_DEVICE_BUS_NUMBER_2    101
#define MOCK_USB_DEVICE_ADDRESS_2       002
#define MOCK_USB_DEVICE_PORT_2          124

#define MOCK_USB_DEVICE_SERIAL_NO "0401805e4532973503374df52a239c898397d348"
#define MOCK_USB_DEVICE_MANUFACTURER "USB"
#define MOCK_USB_DEVICE_PRODUCT "SanDisk 3.2Gen1"
#define LIBUSB_CONFIG_ATT_BUS_POWERED 0x80
namespace {
const string callSign = _T("USBDevice");
}

class USBDeviceTest : public ::testing::Test {
protected:
    Core::ProxyType<Plugin::USBDevice> plugin;
    Core::JSONRPC::Handler& handler;
    DECL_CORE_JSONRPC_CONX connection;
    Core::JSONRPC::Message message;
    string response;
    libUSBImplMock  *p_libUSBImplMock   = nullptr;
    Core::ProxyType<Plugin::USBDeviceImplementation> USBDeviceImpl;
    NiceMock<COMLinkMock> comLinkMock;
    NiceMock<ServiceMock> service;
    PLUGINHOST_DISPATCHER* dispatcher;
    libusb_hotplug_callback_fn libUSBHotPlugCbDeviceAttached = nullptr;
    libusb_hotplug_callback_fn libUSBHotPlugCbDeviceDetached = nullptr;
    Core::ProxyType<WorkerPoolImplementation> workerPool;
    NiceMock<FactoriesImplementation> factoriesImplementation;

    USBDeviceTest()
        : plugin(Core::ProxyType<Plugin::USBDevice>::Create())
        , handler(*(plugin))
        , INIT_CONX(1, 0)
        , workerPool(Core::ProxyType<WorkerPoolImplementation>::Create(
            2, Core::Thread::DefaultStackSize(), 16))
    {
        p_libUSBImplMock  = new NiceMock <libUSBImplMock>;
        libusbApi::setImpl(p_libUSBImplMock);

        ON_CALL(service, COMLink())
            .WillByDefault(::testing::Invoke(
                  [this]() {
                        TEST_LOG("Pass created comLinkMock: %p ", &comLinkMock);
                        return &comLinkMock;
                    }));

#ifdef USE_THUNDER_R4
        ON_CALL(comLinkMock, Instantiate(::testing::_, ::testing::_, ::testing::_))
			.WillByDefault(::testing::Invoke(
                  [&](const RPC::Object& object, const uint32_t waitTime, uint32_t& connectionId) {
                        USBDeviceImpl = Core::ProxyType<Plugin::USBDeviceImplementation>::Create();
                        TEST_LOG("Pass created USBDeviceImpl: %p &USBDeviceImpl: %p", USBDeviceImpl, &USBDeviceImpl);
                        return &USBDeviceImpl;
                    }));
#else
	  ON_CALL(comLinkMock, Instantiate(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
	    .WillByDefault(::testing::Return(USBDeviceImpl));
#endif /*USE_THUNDER_R4 */

        PluginHost::IFactories::Assign(&factoriesImplementation);

        Core::IWorkerPool::Assign(&(*workerPool));
        workerPool->Run();

        dispatcher = static_cast<PLUGINHOST_DISPATCHER*>(
           plugin->QueryInterface(PLUGINHOST_DISPATCHER_ID));
        dispatcher->Activate(&service);

        /* Set all the asynchronouse event handler with libusb to handle various events*/
        ON_CALL(*p_libUSBImplMock, libusb_hotplug_register_callback(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillByDefault(::testing::Invoke(
            [&](libusb_context *ctx, int events, int flags, int vendor_id, int product_id, int dev_class,
                 libusb_hotplug_callback_fn cb_fn, void *user_data, libusb_hotplug_callback_handle *callback_handle) {
                if (LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED == events) {
                    libUSBHotPlugCbDeviceAttached = cb_fn;
                    *callback_handle = 1;
                }
                if (LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT == events) {
                    libUSBHotPlugCbDeviceDetached = cb_fn;
                    *callback_handle = 2;
                }
                return LIBUSB_SUCCESS;
            }));

        EXPECT_EQ(string(""), plugin->Initialize(&service));
    }
    virtual ~USBDeviceTest() override
    {
        TEST_LOG("USBDeviceTest Destructor");

        plugin->Deinitialize(&service);

        dispatcher->Deactivate();
        dispatcher->Release();

        Core::IWorkerPool::Assign(nullptr);
        workerPool.Release();

        PluginHost::IFactories::Assign(nullptr);

        libusbApi::setImpl(nullptr);
        if (p_libUSBImplMock != nullptr)
        {
            delete p_libUSBImplMock;
            p_libUSBImplMock = nullptr;
        }
    }

    virtual void SetUp()
    {
        ASSERT_TRUE(libUSBHotPlugCbDeviceAttached != nullptr);
    }
    void Mock_SetDeviceDesc(uint8_t bus_number, uint8_t device_address);
    void Mock_SetSerialNumberInUSBDevicePath();
};

void USBDeviceTest::Mock_SetDeviceDesc(uint8_t bus_number, uint8_t device_address)
{
     ON_CALL(*p_libUSBImplMock, libusb_get_device_descriptor(::testing::_, ::testing::_))
        .WillByDefault(
            [bus_number, device_address](libusb_device *dev, struct libusb_device_descriptor *desc) {
                 if ((bus_number == dev->bus_number) &&
                     (device_address == dev->device_address))
                 {
                      desc->bDeviceSubClass = LIBUSB_CLASS_MASS_STORAGE;
                      desc->bDeviceClass = LIBUSB_CLASS_MASS_STORAGE;
                 }
                 return LIBUSB_SUCCESS;
     });

    ON_CALL(*p_libUSBImplMock, libusb_get_device_address(::testing::_))
        .WillByDefault(::testing::Return(device_address));

    ON_CALL(*p_libUSBImplMock, libusb_get_bus_number(::testing::_))
        .WillByDefault(::testing::Return(bus_number));

    ON_CALL(*p_libUSBImplMock, libusb_get_port_numbers(::testing::_, ::testing::_, ::testing::_))
        .WillByDefault([](libusb_device *dev, uint8_t *port_numbers, int port_numbers_len) {
            if((nullptr != dev) && (nullptr != port_numbers))
            {
                port_numbers[0] = dev->port_number;
                return 1;
            }
            else
            {
                return 0;
            }
        });

    if (device_address == MOCK_USB_DEVICE_ADDRESS_1)
    {
        std::string vendorFileName = "/tmp/block/sda/device/vendor";
        std::ofstream outVendorStream(vendorFileName);

        if (!outVendorStream) {
            TEST_LOG("Error opening file for writing!");
        }
        outVendorStream << "Generic" << std::endl;
        outVendorStream.close();

        std::string modelFileName = "/tmp/block/sda/device/model";
        std::ofstream outModelStream(modelFileName);

        if (!outModelStream) {
            TEST_LOG("Error opening file for writing!");
        }
        outModelStream << "Flash Disk" << std::endl;
        outModelStream.close();
    }

    if (device_address == MOCK_USB_DEVICE_ADDRESS_2)
    {
        std::string vendorFileName = "/tmp/block/sdb/device/vendor";
        std::ofstream  outVendorStream(vendorFileName);

        if (!outVendorStream) {
            TEST_LOG("Error opening file for writing!");
        }
        outVendorStream << "JetFlash" << std::endl;
        outVendorStream.close();

        std::string modelFileName = "/tmp/block/sdb/device/model";
        std::ofstream outModelStream(modelFileName);

        if (!outModelStream) {
            TEST_LOG("Error opening file for writing!");
        }
        outModelStream << "Transcend_16GB" << std::endl;
        outModelStream.close();
    }
}

void USBDeviceTest::Mock_SetSerialNumberInUSBDevicePath()
{
    std::string serialNumFileName1 = "/tmp/bus/usb/devices/100-123/serial";
    std::ofstream serialNumOutFile1(serialNumFileName1);

    if (!serialNumOutFile1) {
        TEST_LOG("Error opening file for writing!");
    }
    serialNumOutFile1 << "B32FD507" << std::endl;
    serialNumOutFile1.close();

    std::string serialNumFileName2 = "/tmp/bus/usb/devices/101-124/serial";
    std::ofstream serialNumOutFile2(serialNumFileName2);

    if (!serialNumOutFile2) {
        TEST_LOG("Error opening file for writing!");
    }
    serialNumOutFile2<< "UEUIRCXT" << std::endl;
    serialNumOutFile2.close();

    std::string serialNumFileSda = "/dev/sda";
    std::ofstream serialNumOutFileSda(serialNumFileSda);

    if (!serialNumOutFileSda) {
        TEST_LOG("Error opening file for writing!");
    }
    serialNumOutFileSda << "B32FD507 100-123" << std::endl;
    serialNumOutFileSda.close();


    std::string serialNumFileSdb = "/dev/sdb";
    std::ofstream serialNumOutFileSdb(serialNumFileSdb);

    if (!serialNumOutFileSdb) {
        TEST_LOG("Error opening file for writing!");
    }
    serialNumOutFileSdb << "UEUIRCXT 101-124" << std::endl;
    serialNumOutFileSdb.close();
}


TEST_F(USBDeviceTest, RegisteredMethods)
{
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("getDeviceList")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("getDeviceInfo")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("bindDriver")));
    EXPECT_EQ(Core::ERROR_NONE, handler.Exists(_T("unbindDriver")));
}

TEST_F(USBDeviceTest, Information_ReturnsEmptyString)
{
    EXPECT_EQ(plugin->Information(), string(""));
}

/*******************************************************************************************************************
*Test function for Event:onDevicePluggedIn
*Event : onDevicePluggedIn
*             Triggered when a USB device is plugged in
*
*                @return (i) Exchange::IUSBDevice::USBDevice structure
* Use case coverage:
*                @Success :1
********************************************************************************************************************/

/**
 * @brief : Validates onDevicePluggedIn event when a USB mass storage device is connected
 *          Verifies that the event is triggered with correct device information including
 *          device class (8), subclass (8), device name (100/001), and device path (/dev/sda)
 * @param[in] : No parameters
 * @return : JSON event with device structure containing class 8, subclass 8, name "100/001", path "/dev/sda"
 *
 */
TEST_F(USBDeviceTest, OnDevicePluggedInSuccess)
{
    Core::Event onDevicePluggedIn(false, true);

    Mock_SetSerialNumberInUSBDevicePath();

    EXPECT_CALL(service, Submit(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [&](const uint32_t, const Core::ProxyType<Core::JSON::IElement>& json) {
                string text;
                TEST_LOG("json to string!");
                EXPECT_TRUE(json->ToString(text));
                EXPECT_EQ(text, "{\"jsonrpc\":\"2.0\",\"method\":\"org.rdk.USBDevice.onDevicePluggedIn\",\"params\":{\"device\":{\"deviceClass\":8,\"deviceSubclass\":8,\"deviceName\":\"100\\/001\",\"devicePath\":\"\\/dev\\/sda\"}}}");
                onDevicePluggedIn.SetEvent();

                return Core::ERROR_NONE;
            }));

    /* HotPlug Attach Device 1 Verification */
    Mock_SetDeviceDesc(MOCK_USB_DEVICE_BUS_NUMBER_1, MOCK_USB_DEVICE_ADDRESS_1);

    EVENT_SUBSCRIBE(0, _T("onDevicePluggedIn"), _T("org.rdk.USBDevice"), message);

    libusb_device dev = {0};
    dev.bus_number = MOCK_USB_DEVICE_BUS_NUMBER_1;
    dev.device_address = MOCK_USB_DEVICE_ADDRESS_1;
    dev.port_number = MOCK_USB_DEVICE_PORT_1;

    libUSBHotPlugCbDeviceAttached(nullptr, &dev, LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED, 0);
    TEST_LOG("After libUSBHotPlugCbDeviceAttached");

    EXPECT_EQ(Core::ERROR_NONE, onDevicePluggedIn.Lock());
    TEST_LOG("After EVENT_UNSUBSCRIBE");

    EVENT_UNSUBSCRIBE(0, _T("onDevicePluggedIn"), _T("org.rdk.USBDevice"), message);
 }
/*Test cases for onDevicePluggedIn ends here*/

/*******************************************************************************************************************
*Test function for Event:onDevicePluggedOut
*Event : onDevicePluggedOut
*             Triggered when a USB drive is plugged out
*
*                @return (i) Exchange::IUSBDevice::USBDevice structure
* Use case coverage:
*                @Success :1
********************************************************************************************************************/

/**
 * @brief : Validates onDevicePluggedOut event when a USB mass storage device is disconnected
 *          Verifies that the event is triggered with correct device information when device is removed
 * @param[in] : No parameters
 * @return : JSON event with device structure containing class 8, subclass 8, name "100/001", path "/dev/sda"
 *
 */
TEST_F(USBDeviceTest, onDevicePluggedOutSuccess)
{
    Core::Event onDevicePluggedOut(false, true);

    Mock_SetSerialNumberInUSBDevicePath();

    EXPECT_CALL(service, Submit(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [&](const uint32_t, const Core::ProxyType<Core::JSON::IElement>& json) {
                string text;
                EXPECT_TRUE(json->ToString(text));
                EXPECT_EQ(text, "{\"jsonrpc\":\"2.0\",\"method\":\"org.rdk.USBDevice.onDevicePluggedOut\",\"params\":{\"device\":{\"deviceClass\":8,\"deviceSubclass\":8,\"deviceName\":\"100\\/001\",\"devicePath\":\"\\/dev\\/sda\"}}}");

                onDevicePluggedOut.SetEvent();

                return Core::ERROR_NONE;
            }));

    /* HotPlug Attach Device 1 Verification */
    Mock_SetDeviceDesc(MOCK_USB_DEVICE_BUS_NUMBER_1, MOCK_USB_DEVICE_ADDRESS_1);

    EVENT_SUBSCRIBE(0, _T("onDevicePluggedOut"), _T("org.rdk.USBDevice"), message);

    libusb_device dev = {0};
    dev.bus_number = MOCK_USB_DEVICE_BUS_NUMBER_1;
    dev.device_address = MOCK_USB_DEVICE_ADDRESS_1;
    dev.port_number = MOCK_USB_DEVICE_PORT_1;

    libUSBHotPlugCbDeviceDetached(nullptr, &dev, LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT, 0);

    EXPECT_EQ(Core::ERROR_NONE, onDevicePluggedOut.Lock());
    EVENT_UNSUBSCRIBE(0, _T("onDevicePluggedOut"), _T("org.rdk.USBDevice"), message);
 }
/*Test cases for onDevicePluggedOut ends here*/

/*******************************************************************************************************************
 * Test function for :getDeviceList
 * getDeviceList :
 *                Gets a list of usb device details
 *
 *                @return Response object containing the usb device list
 * Use case coverage:
 *                @Success :4 (single device, multiple devices, empty list, non-mass storage devices)
 *                @Failure :2 (libusb errors, descriptor retrieval failures)
 ********************************************************************************************************************/

/**
 * @brief : Tests getDeviceList with a single mass storage USB device connected
 *          Verifies correct retrieval of device information including class, subclass, name and path
 *
 * @param[out]   :  Array with one USB device entry
 * @return      :  ERROR_NONE with JSON array containing one device with class 8, subclass 8
 */
TEST_F(USBDeviceTest, getDeviceListUsingWithSingleMassStorageUSBSuccessCase)
{
    Mock_SetSerialNumberInUSBDevicePath();
    Mock_SetDeviceDesc(MOCK_USB_DEVICE_BUS_NUMBER_1, MOCK_USB_DEVICE_ADDRESS_1);

    /* Mock for Device List, 1 Device */
    ON_CALL(*p_libUSBImplMock, libusb_get_device_list(::testing::_, ::testing::_))
    .WillByDefault(
    [](libusb_context *ctx, libusb_device ***list) {
        struct libusb_device **ret = nullptr;
        ssize_t len = 1;

        ret = (struct libusb_device **)malloc(len * sizeof(struct libusb_device *));

        if (nullptr == ret)
        {
            std::cout << "malloc failed";
            len = 0;
        }
        else
        {
            uint8_t bus_number = MOCK_USB_DEVICE_BUS_NUMBER_1;
            uint8_t device_address = MOCK_USB_DEVICE_ADDRESS_1;
            uint8_t port_number = MOCK_USB_DEVICE_PORT_1;

            for (int index = 0; index < len; ++index)
            {
                ret[index] =  (struct libusb_device *)malloc(sizeof(struct libusb_device));

                if (nullptr == ret[index])
                {
                    std::cout << "malloc failed";
                    len = 0;
                }
                else
                {
                    ret[index]->bus_number = bus_number;
                    ret[index]->device_address = device_address;
                    ret[index]->port_number = port_number;

                    bus_number += 1;
                    device_address += 1;
                    port_number += 1;
                }
            }
            *list = ret;
        }

        return len;
    });

    ON_CALL(*p_libUSBImplMock, libusb_free_device_list(::testing::_, ::testing::_))
    .WillByDefault(
    [](libusb_device **list, int unref_devices) {
        for (int index = 0; index < 2; ++index)
        {
            free(list[index]);
        }
    });

    EXPECT_CALL(*p_libUSBImplMock, libusb_get_device_descriptor(::testing::_, ::testing::_))
    .WillRepeatedly(
    [](libusb_device *dev, struct libusb_device_descriptor *desc) {
        desc->bDeviceSubClass = LIBUSB_CLASS_MASS_STORAGE;
        desc->bDeviceClass = LIBUSB_CLASS_MASS_STORAGE;
        return LIBUSB_SUCCESS;
    });

    EXPECT_CALL(*p_libUSBImplMock, libusb_get_device_address(::testing::_))
    .WillRepeatedly(
    [](libusb_device *dev) {
        return dev->device_address;
    });

    EXPECT_CALL(*p_libUSBImplMock, libusb_get_bus_number(::testing::_))
    .WillRepeatedly(
    [](libusb_device *dev) {
        return dev->bus_number;
    });

    EXPECT_CALL(*p_libUSBImplMock, libusb_get_port_numbers(::testing::_, ::testing::_, ::testing::_))
    .WillRepeatedly([](libusb_device *dev, uint8_t *port_numbers, int port_numbers_len) {
        if((nullptr != dev) && (nullptr != port_numbers))
        {
            port_numbers[0] = dev->port_number;
            return 1;
        }
        else
        {
            return 0;
        }
    });

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getDeviceList"), _T("{}"), response));
    EXPECT_EQ(response, string("[{\"deviceClass\":8,\"deviceSubclass\":8,\"deviceName\":\"100\\/001\",\"devicePath\":\"\\/dev\\/sda\"}]"));
}

TEST_F(USBDeviceTest, GetDeviceList_EmptyList_Success)
{
    EXPECT_CALL(*p_libUSBImplMock, libusb_get_device_list(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(0));

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getDeviceList"), _T("{}"), response));
    EXPECT_EQ(response, string("[]"));
}

TEST_F(USBDeviceTest, GetDeviceList_GetDescriptorFailure)
{
    Mock_SetSerialNumberInUSBDevicePath();

    ON_CALL(*p_libUSBImplMock, libusb_get_device_list(::testing::_, ::testing::_))
        .WillByDefault([](libusb_context *ctx, libusb_device ***list) {
            struct libusb_device **ret = (struct libusb_device **)malloc(1 * sizeof(struct libusb_device *));
            ret[0] = (struct libusb_device *)malloc(sizeof(struct libusb_device));
            ret[0]->bus_number = MOCK_USB_DEVICE_BUS_NUMBER_1;
            ret[0]->device_address = MOCK_USB_DEVICE_ADDRESS_1;
            ret[0]->port_number = MOCK_USB_DEVICE_PORT_1;
            *list = ret;
            return (ssize_t)1;
        });

    ON_CALL(*p_libUSBImplMock, libusb_free_device_list(::testing::_, ::testing::_))
        .WillByDefault([](libusb_device **list, int unref_devices) {
            free(list[0]);
            free(list);
        });

    EXPECT_CALL(*p_libUSBImplMock, libusb_get_device_descriptor(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(LIBUSB_ERROR_NO_DEVICE));

    EXPECT_CALL(*p_libUSBImplMock, libusb_free_device_list(::testing::_, ::testing::_))
        .Times(1);

    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("getDeviceList"), _T("{}"), response));
    EXPECT_EQ(response, string(""));
}

TEST_F(USBDeviceTest, GetDeviceList_NonMassStorageDevice_Success)
{
    Mock_SetSerialNumberInUSBDevicePath();

    ON_CALL(*p_libUSBImplMock, libusb_get_device_list(::testing::_, ::testing::_))
        .WillByDefault([](libusb_context *ctx, libusb_device ***list) {
            struct libusb_device **ret = (struct libusb_device **)malloc(1 * sizeof(struct libusb_device *));
            ret[0] = (struct libusb_device *)malloc(sizeof(struct libusb_device));
            ret[0]->bus_number = MOCK_USB_DEVICE_BUS_NUMBER_1;
            ret[0]->device_address = MOCK_USB_DEVICE_ADDRESS_1;
            ret[0]->port_number = MOCK_USB_DEVICE_PORT_1;
            *list = ret;
            return (ssize_t)1;
        });

    ON_CALL(*p_libUSBImplMock, libusb_free_device_list(::testing::_, ::testing::_))
        .WillByDefault([](libusb_device **list, int unref_devices) {
            free(list[0]);
            free(list);
        });

    EXPECT_CALL(*p_libUSBImplMock, libusb_get_device_descriptor(::testing::_, ::testing::_))
        .WillRepeatedly([](libusb_device *dev, struct libusb_device_descriptor *desc) {
            desc->bDeviceSubClass = LIBUSB_CLASS_HID;
            desc->bDeviceClass = LIBUSB_CLASS_HID;
            return LIBUSB_SUCCESS;
        });

    EXPECT_CALL(*p_libUSBImplMock, libusb_get_device_address(::testing::_))
        .WillRepeatedly([](libusb_device *dev) {
            return dev->device_address;
        });

    EXPECT_CALL(*p_libUSBImplMock, libusb_get_bus_number(::testing::_))
        .WillRepeatedly([](libusb_device *dev) {
            return dev->bus_number;
        });

    EXPECT_CALL(*p_libUSBImplMock, libusb_get_port_numbers(::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly([](libusb_device *dev, uint8_t *port_numbers, int port_numbers_len) {
            if((nullptr != dev) && (nullptr != port_numbers)) {
                port_numbers[0] = dev->port_number;
                return 1;
            }
            return 0;
        });

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getDeviceList"), _T("{}"), response));
    EXPECT_EQ(response, string("[{\"deviceClass\":3,\"deviceSubclass\":3,\"deviceName\":\"100\\/001\",\"devicePath\":\"\"}]"));
}

/**
 * @brief : Tests getDeviceList with multiple mass storage USB devices connected
 *          Verifies correct retrieval of all devices with their respective information
 *
 * @param[out]   :  Array with multiple USB device entries
 * @return      :  ERROR_NONE with JSON array containing two devices (100/001 and 101/002)
 */
TEST_F(USBDeviceTest, getDeviceListUsingWithMultipleMassStorageUSBSuccessCase)
{
    Mock_SetSerialNumberInUSBDevicePath();
    Mock_SetDeviceDesc(MOCK_USB_DEVICE_BUS_NUMBER_1, MOCK_USB_DEVICE_ADDRESS_1);
    Mock_SetDeviceDesc(MOCK_USB_DEVICE_BUS_NUMBER_2, MOCK_USB_DEVICE_ADDRESS_2);

    /* Mock for Device List, 2 Devices */
    ON_CALL(*p_libUSBImplMock, libusb_get_device_list(::testing::_, ::testing::_))
    .WillByDefault(
    [](libusb_context *ctx, libusb_device ***list) {
        struct libusb_device **ret = nullptr;
        ssize_t len = 2;

        ret = (struct libusb_device **)malloc(len * sizeof(struct libusb_device *));

        if (nullptr == ret)
        {
            std::cout << "malloc failed";
            len = 0;
        }
        else
        {
            uint8_t bus_number = MOCK_USB_DEVICE_BUS_NUMBER_1;
            uint8_t device_address = MOCK_USB_DEVICE_ADDRESS_1;
            uint8_t port_number = MOCK_USB_DEVICE_PORT_1;

            for (int index = 0; index < len; ++index)
            {
                ret[index] =  (struct libusb_device *)malloc(sizeof(struct libusb_device));

                if (nullptr == ret[index])
                {
                    std::cout << "malloc failed";
                    len = 0;
                }
                else
                {
                    ret[index]->bus_number = bus_number;
                    ret[index]->device_address = device_address;
                    ret[index]->port_number = port_number;

                    bus_number += 1;
                    device_address += 1;
                    port_number += 1;
                }
            }
            *list = ret;
        }

        return len;
    });

    ON_CALL(*p_libUSBImplMock, libusb_free_device_list(::testing::_, ::testing::_))
    .WillByDefault(
    [](libusb_device **list, int unref_devices) {
        for (int index = 0; index < 2; ++index)
        {
        free(list[index]);
        }
    });

    EXPECT_CALL(*p_libUSBImplMock, libusb_get_device_descriptor(::testing::_, ::testing::_))
    .WillRepeatedly(
    [](libusb_device *dev, struct libusb_device_descriptor *desc) {
            desc->bDeviceSubClass = LIBUSB_CLASS_MASS_STORAGE;
            desc->bDeviceClass = LIBUSB_CLASS_MASS_STORAGE;
            return LIBUSB_SUCCESS;
    });

    EXPECT_CALL(*p_libUSBImplMock, libusb_get_device_address(::testing::_))
    .WillRepeatedly(
    [](libusb_device *dev) {
        return dev->device_address;
    });

    EXPECT_CALL(*p_libUSBImplMock, libusb_get_bus_number(::testing::_))
    .WillRepeatedly(
    [](libusb_device *dev) {
        return dev->bus_number;
    });

    EXPECT_CALL(*p_libUSBImplMock, libusb_get_port_numbers(::testing::_, ::testing::_, ::testing::_))
    .WillRepeatedly([](libusb_device *dev, uint8_t *port_numbers, int port_numbers_len) {
    if((nullptr != dev) && (nullptr != port_numbers))
    {
        port_numbers[0] = dev->port_number;
        return 1;
    }
    else
    {
        return 0;
    }
    });

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getDeviceList"), _T("{}"), response));
    EXPECT_EQ(response, string("[{\"deviceClass\":8,\"deviceSubclass\":8,\"deviceName\":\"100\\/001\",\"devicePath\":\"\\/dev\\/sda\"},{\"deviceClass\":8,\"deviceSubclass\":8,\"deviceName\":\"101\\/002\",\"devicePath\":\"\\/dev\\/sdb\"}]"));
}
/*Test cases for getDeviceList ends here*/

/*******************************************************************************************************************
 * Test function for :bindDriver
 * bindDriver :
 *                Binds the respective driver for the device.
 *
 *                @return Success status
 * Use case coverage:
 *                @Success :2 (driver bound successfully, driver already active)
 *                @Failure :5 (invalid device, open failure, kernel driver check failure, attach failure, no devices)
 ********************************************************************************************************************/

/**
 * @brief : Tests bindDriver with a valid device name
 *          Verifies that the kernel driver is successfully attached to the specified USB device
 *
 * @param[in]   :  deviceName
 * @return      :  error code: ERROR_NONE
 */
 TEST_F(USBDeviceTest, bindDriverSuccessCase)
{
    Mock_SetSerialNumberInUSBDevicePath();
    Mock_SetDeviceDesc(MOCK_USB_DEVICE_BUS_NUMBER_1, MOCK_USB_DEVICE_ADDRESS_1);
    Mock_SetDeviceDesc(MOCK_USB_DEVICE_BUS_NUMBER_2, MOCK_USB_DEVICE_ADDRESS_2);

    /* Mock for Device List, 2 Devices */
    ON_CALL(*p_libUSBImplMock, libusb_get_device_list(::testing::_, ::testing::_))
    .WillByDefault(
    [](libusb_context *ctx, libusb_device ***list) {
        struct libusb_device **ret = nullptr;
        ssize_t len = 2;

        ret = (struct libusb_device **)malloc(len * sizeof(struct libusb_device *));

        if (nullptr == ret)
        {
            std::cout << "malloc failed";
            len = 0;
        }
        else
        {
            uint8_t bus_number = MOCK_USB_DEVICE_BUS_NUMBER_1;
            uint8_t device_address = MOCK_USB_DEVICE_ADDRESS_1;
            uint8_t port_number = MOCK_USB_DEVICE_PORT_1;

            for (int index = 0; index < len; ++index)
            {
                ret[index] =  (struct libusb_device *)malloc(sizeof(struct libusb_device));

                if (nullptr == ret[index])
                {
                    std::cout << "malloc failed";
                    len = 0;
                }
                else
                {
                    ret[index]->bus_number = bus_number;
                    ret[index]->device_address = device_address;
                    ret[index]->port_number = port_number;

                    bus_number += 1;
                    device_address += 1;
                    port_number += 1;
                }
            }
            *list = ret;
        }

        return len;
    });

    ON_CALL(*p_libUSBImplMock, libusb_free_device_list(::testing::_, ::testing::_))
    .WillByDefault(
    [](libusb_device **list, int unref_devices) {
        for (int index = 0; index < 2; ++index)
        {
        free(list[index]);
        }
    });

    EXPECT_CALL(*p_libUSBImplMock, libusb_get_device_descriptor(::testing::_, ::testing::_))
    .WillRepeatedly(
    [](libusb_device *dev, struct libusb_device_descriptor *desc) {
            desc->bDeviceSubClass = LIBUSB_CLASS_MASS_STORAGE;
            desc->bDeviceClass = LIBUSB_CLASS_MASS_STORAGE;
            return LIBUSB_SUCCESS;
    });

    EXPECT_CALL(*p_libUSBImplMock, libusb_get_device_address(::testing::_))
    .WillRepeatedly(
    [](libusb_device *dev) {
        return dev->device_address;
    });

    EXPECT_CALL(*p_libUSBImplMock, libusb_get_bus_number(::testing::_))
    .WillRepeatedly(
    [](libusb_device *dev) {
        return dev->bus_number;
    });

    EXPECT_CALL(*p_libUSBImplMock, libusb_get_port_numbers(::testing::_, ::testing::_, ::testing::_))
    .WillRepeatedly([](libusb_device *dev, uint8_t *port_numbers, int port_numbers_len) {
    if((nullptr != dev) && (nullptr != port_numbers))
    {
        port_numbers[0] = dev->port_number;
        return 1;
    }
    else
    {
        return 0;
    }
    });

    /* Call bindDriver method */
    TEST_LOG("call BindDriver");
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("bindDriver"), _T("{\"deviceName\":\"100\\/001\"}"), response));
    EXPECT_EQ(response, string(""));
}

TEST_F(USBDeviceTest, BindDriver_DriverAlreadyActive)
{
    Mock_SetSerialNumberInUSBDevicePath();
    Mock_SetDeviceDesc(MOCK_USB_DEVICE_BUS_NUMBER_1, MOCK_USB_DEVICE_ADDRESS_1);

    ON_CALL(*p_libUSBImplMock, libusb_get_device_list(::testing::_, ::testing::_))
        .WillByDefault([](libusb_context *ctx, libusb_device ***list) {
            struct libusb_device **ret = (struct libusb_device **)malloc(1 * sizeof(struct libusb_device *));
            ret[0] = (struct libusb_device *)malloc(sizeof(struct libusb_device));
            ret[0]->bus_number = MOCK_USB_DEVICE_BUS_NUMBER_1;
            ret[0]->device_address = MOCK_USB_DEVICE_ADDRESS_1;
            ret[0]->port_number = MOCK_USB_DEVICE_PORT_1;
            *list = ret;
            return (ssize_t)1;
        });

    ON_CALL(*p_libUSBImplMock, libusb_free_device_list(::testing::_, ::testing::_))
        .WillByDefault([](libusb_device **list, int unref_devices) {
            free(list[0]);
            free(list);
        });

    EXPECT_CALL(*p_libUSBImplMock, libusb_kernel_driver_active(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(1));

    EXPECT_CALL(*p_libUSBImplMock, libusb_close(::testing::_))
        .Times(1);

    EXPECT_CALL(*p_libUSBImplMock, libusb_free_device_list(::testing::_, ::testing::_))
        .Times(1);

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("bindDriver"), _T("{\"deviceName\":\"100\\/001\"}"), response));
    EXPECT_EQ(response, string(""));
}

TEST_F(USBDeviceTest, BindDriver_InvalidDeviceName)
{
    Mock_SetSerialNumberInUSBDevicePath();

    ON_CALL(*p_libUSBImplMock, libusb_get_device_list(::testing::_, ::testing::_))
        .WillByDefault([](libusb_context *ctx, libusb_device ***list) {
            struct libusb_device **ret = (struct libusb_device **)malloc(1 * sizeof(struct libusb_device *));
            ret[0] = (struct libusb_device *)malloc(sizeof(struct libusb_device));
            ret[0]->bus_number = MOCK_USB_DEVICE_BUS_NUMBER_1;
            ret[0]->device_address = MOCK_USB_DEVICE_ADDRESS_1;
            ret[0]->port_number = MOCK_USB_DEVICE_PORT_1;
            *list = ret;
            return (ssize_t)1;
        });

    ON_CALL(*p_libUSBImplMock, libusb_free_device_list(::testing::_, ::testing::_))
        .WillByDefault([](libusb_device **list, int unref_devices) {
            free(list[0]);
            free(list);
        });

    EXPECT_CALL(*p_libUSBImplMock, libusb_get_device_list(::testing::_, ::testing::_))
        .Times(1);

    EXPECT_CALL(*p_libUSBImplMock, libusb_free_device_list(::testing::_, ::testing::_))
        .Times(1);

    EXPECT_CALL(*p_libUSBImplMock, libusb_open(::testing::_, ::testing::_))
        .Times(0);

    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("bindDriver"), _T("{\"deviceName\":\"999\\/999\"}"), response));
    EXPECT_EQ(response, string(""));
}

TEST_F(USBDeviceTest, BindDriver_OpenDeviceFailure)
{
    Mock_SetSerialNumberInUSBDevicePath();

    ON_CALL(*p_libUSBImplMock, libusb_get_device_list(::testing::_, ::testing::_))
        .WillByDefault([](libusb_context *ctx, libusb_device ***list) {
            struct libusb_device **ret = (struct libusb_device **)malloc(1 * sizeof(struct libusb_device *));
            ret[0] = (struct libusb_device *)malloc(sizeof(struct libusb_device));
            ret[0]->bus_number = MOCK_USB_DEVICE_BUS_NUMBER_1;
            ret[0]->device_address = MOCK_USB_DEVICE_ADDRESS_1;
            ret[0]->port_number = MOCK_USB_DEVICE_PORT_1;
            *list = ret;
            return (ssize_t)1;
        });

    ON_CALL(*p_libUSBImplMock, libusb_free_device_list(::testing::_, ::testing::_))
        .WillByDefault([](libusb_device **list, int unref_devices) {
            free(list[0]);
            free(list);
        });

    EXPECT_CALL(*p_libUSBImplMock, libusb_get_device_address(::testing::_))
        .WillRepeatedly([](libusb_device *dev) {
            return dev->device_address;
        });

    EXPECT_CALL(*p_libUSBImplMock, libusb_get_bus_number(::testing::_))
        .WillRepeatedly([](libusb_device *dev) {
            return dev->bus_number;
        });

    EXPECT_CALL(*p_libUSBImplMock, libusb_open(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(LIBUSB_ERROR_NO_DEVICE));

    EXPECT_CALL(*p_libUSBImplMock, libusb_close(::testing::_))
        .Times(0);

    EXPECT_CALL(*p_libUSBImplMock, libusb_free_device_list(::testing::_, ::testing::_))
        .Times(1);

    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("bindDriver"), _T("{\"deviceName\":\"100\\/001\"}"), response));
    EXPECT_EQ(response, string(""));
}

TEST_F(USBDeviceTest, BindDriver_KernelDriverActiveFailure)
{
    Mock_SetSerialNumberInUSBDevicePath();

    ON_CALL(*p_libUSBImplMock, libusb_get_device_list(::testing::_, ::testing::_))
        .WillByDefault([](libusb_context *ctx, libusb_device ***list) {
            struct libusb_device **ret = (struct libusb_device **)malloc(1 * sizeof(struct libusb_device *));
            ret[0] = (struct libusb_device *)malloc(sizeof(struct libusb_device));
            ret[0]->bus_number = MOCK_USB_DEVICE_BUS_NUMBER_1;
            ret[0]->device_address = MOCK_USB_DEVICE_ADDRESS_1;
            ret[0]->port_number = MOCK_USB_DEVICE_PORT_1;
            *list = ret;
            return (ssize_t)1;
        });

    ON_CALL(*p_libUSBImplMock, libusb_free_device_list(::testing::_, ::testing::_))
        .WillByDefault([](libusb_device **list, int unref_devices) {
            free(list[0]);
            free(list);
        });

    EXPECT_CALL(*p_libUSBImplMock, libusb_get_device_address(::testing::_))
        .WillRepeatedly([](libusb_device *dev) {
            return dev->device_address;
        });

    EXPECT_CALL(*p_libUSBImplMock, libusb_get_bus_number(::testing::_))
        .WillRepeatedly([](libusb_device *dev) {
            return dev->bus_number;
        });

    EXPECT_CALL(*p_libUSBImplMock, libusb_kernel_driver_active(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(LIBUSB_ERROR_NO_DEVICE));

    EXPECT_CALL(*p_libUSBImplMock, libusb_close(::testing::_))
        .Times(1);

    EXPECT_CALL(*p_libUSBImplMock, libusb_free_device_list(::testing::_, ::testing::_))
        .Times(1);

    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("bindDriver"), _T("{\"deviceName\":\"100\\/001\"}"), response));
    EXPECT_EQ(response, string(""));
}

TEST_F(USBDeviceTest, BindDriver_AttachDriverFailure)
{
    Mock_SetSerialNumberInUSBDevicePath();

    ON_CALL(*p_libUSBImplMock, libusb_get_device_list(::testing::_, ::testing::_))
        .WillByDefault([](libusb_context *ctx, libusb_device ***list) {
            struct libusb_device **ret = (struct libusb_device **)malloc(1 * sizeof(struct libusb_device *));
            ret[0] = (struct libusb_device *)malloc(sizeof(struct libusb_device));
            ret[0]->bus_number = MOCK_USB_DEVICE_BUS_NUMBER_1;
            ret[0]->device_address = MOCK_USB_DEVICE_ADDRESS_1;
            ret[0]->port_number = MOCK_USB_DEVICE_PORT_1;
            *list = ret;
            return (ssize_t)1;
        });

    ON_CALL(*p_libUSBImplMock, libusb_free_device_list(::testing::_, ::testing::_))
        .WillByDefault([](libusb_device **list, int unref_devices) {
            free(list[0]);
            free(list);
        });

    EXPECT_CALL(*p_libUSBImplMock, libusb_get_device_address(::testing::_))
        .WillRepeatedly([](libusb_device *dev) {
            return dev->device_address;
        });

    EXPECT_CALL(*p_libUSBImplMock, libusb_get_bus_number(::testing::_))
        .WillRepeatedly([](libusb_device *dev) {
            return dev->bus_number;
        });

    EXPECT_CALL(*p_libUSBImplMock, libusb_kernel_driver_active(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(0));

    EXPECT_CALL(*p_libUSBImplMock, libusb_attach_kernel_driver(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(LIBUSB_ERROR_NOT_FOUND));

    EXPECT_CALL(*p_libUSBImplMock, libusb_close(::testing::_))
        .Times(1);

    EXPECT_CALL(*p_libUSBImplMock, libusb_free_device_list(::testing::_, ::testing::_))
        .Times(1);

    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("bindDriver"), _T("{\"deviceName\":\"100\\/001\"}"), response));
    EXPECT_EQ(response, string(""));
}

TEST_F(USBDeviceTest, BindDriver_NoDevicesAvailable)
{
    EXPECT_CALL(*p_libUSBImplMock, libusb_get_device_list(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(0));

    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("bindDriver"), _T("{\"deviceName\":\"100\\/001\"}"), response));
    EXPECT_EQ(response, string(""));
}
/*Test cases for bindDriver ends here*/

/*******************************************************************************************************************
 * Test function for :unbindDriver
 * unbindDriver :
 *                UnBinds the respective driver for the device.
 *
 *                @return Success status
 * Use case coverage:
 *                @Success :2 (driver unbound successfully, no driver active)
 *                @Failure :5 (invalid device, open failure, kernel driver check failure, detach failure, no devices)
 ********************************************************************************************************************/

/**
 * @brief : Tests unbindDriver with a valid device name
 *          Verifies that the kernel driver is successfully detached from the specified USB device
 *
 * @param[in]   :  deviceName
 * @return      :  error code: ERROR_NONE
 */
 TEST_F(USBDeviceTest, unbindDriverSuccessCase)
{
    Mock_SetSerialNumberInUSBDevicePath();
    Mock_SetDeviceDesc(MOCK_USB_DEVICE_BUS_NUMBER_1, MOCK_USB_DEVICE_ADDRESS_1);
    Mock_SetDeviceDesc(MOCK_USB_DEVICE_BUS_NUMBER_2, MOCK_USB_DEVICE_ADDRESS_2);

    /* Mock for Device List, 2 Devices */
    ON_CALL(*p_libUSBImplMock, libusb_get_device_list(::testing::_, ::testing::_))
    .WillByDefault(
    [](libusb_context *ctx, libusb_device ***list) {
        struct libusb_device **ret = nullptr;
        ssize_t len = 2;

        ret = (struct libusb_device **)malloc(len * sizeof(struct libusb_device *));

        if (nullptr == ret)
        {
            std::cout << "malloc failed";
            len = 0;
        }
        else
        {
            uint8_t bus_number = MOCK_USB_DEVICE_BUS_NUMBER_1;
            uint8_t device_address = MOCK_USB_DEVICE_ADDRESS_1;
            uint8_t port_number = MOCK_USB_DEVICE_PORT_1;

            for (int index = 0; index < len; ++index)
            {
                ret[index] =  (struct libusb_device *)malloc(sizeof(struct libusb_device));

                if (nullptr == ret[index])
                {
                    std::cout << "malloc failed";
                    len = 0;
                }
                else
                {
                    ret[index]->bus_number = bus_number;
                    ret[index]->device_address = device_address;
                    ret[index]->port_number = port_number;

                    bus_number += 1;
                    device_address += 1;
                    port_number += 1;
                }
            }
            *list = ret;
        }

        return len;
    });

    ON_CALL(*p_libUSBImplMock, libusb_free_device_list(::testing::_, ::testing::_))
    .WillByDefault(
    [](libusb_device **list, int unref_devices) {
        for (int index = 0; index < 2; ++index)
        {
        free(list[index]);
        }
    });

    EXPECT_CALL(*p_libUSBImplMock, libusb_get_device_descriptor(::testing::_, ::testing::_))
    .WillRepeatedly(
    [](libusb_device *dev, struct libusb_device_descriptor *desc) {
            desc->bDeviceSubClass = LIBUSB_CLASS_MASS_STORAGE;
            desc->bDeviceClass = LIBUSB_CLASS_MASS_STORAGE;
            return LIBUSB_SUCCESS;
    });

    EXPECT_CALL(*p_libUSBImplMock, libusb_get_device_address(::testing::_))
    .WillRepeatedly(
    [](libusb_device *dev) {
        return dev->device_address;
    });

    EXPECT_CALL(*p_libUSBImplMock, libusb_get_bus_number(::testing::_))
    .WillRepeatedly(
    [](libusb_device *dev) {
        return dev->bus_number;
    });

    EXPECT_CALL(*p_libUSBImplMock, libusb_get_port_numbers(::testing::_, ::testing::_, ::testing::_))
    .WillRepeatedly([](libusb_device *dev, uint8_t *port_numbers, int port_numbers_len) {
    if((nullptr != dev) && (nullptr != port_numbers))
    {
        port_numbers[0] = dev->port_number;
        return 1;
    }
    else
    {
        return 0;
    }
    });

    /* Call unbindDriver method */
    TEST_LOG("call UnBindDriver");
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("unbindDriver"), _T("{\"deviceName\":\"100\\/001\"}"), response));
    EXPECT_EQ(response, string(""));
}

TEST_F(USBDeviceTest, UnbindDriver_NoDriverActive)
{
    Mock_SetSerialNumberInUSBDevicePath();
    Mock_SetDeviceDesc(MOCK_USB_DEVICE_BUS_NUMBER_1, MOCK_USB_DEVICE_ADDRESS_1);

    ON_CALL(*p_libUSBImplMock, libusb_get_device_list(::testing::_, ::testing::_))
        .WillByDefault([](libusb_context *ctx, libusb_device ***list) {
            struct libusb_device **ret = (struct libusb_device **)malloc(1 * sizeof(struct libusb_device *));
            ret[0] = (struct libusb_device *)malloc(sizeof(struct libusb_device));
            ret[0]->bus_number = MOCK_USB_DEVICE_BUS_NUMBER_1;
            ret[0]->device_address = MOCK_USB_DEVICE_ADDRESS_1;
            ret[0]->port_number = MOCK_USB_DEVICE_PORT_1;
            *list = ret;
            return (ssize_t)1;
        });

    ON_CALL(*p_libUSBImplMock, libusb_free_device_list(::testing::_, ::testing::_))
        .WillByDefault([](libusb_device **list, int unref_devices) {
            free(list[0]);
            free(list);
        });

    EXPECT_CALL(*p_libUSBImplMock, libusb_kernel_driver_active(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(0));

    EXPECT_CALL(*p_libUSBImplMock, libusb_close(::testing::_))
        .Times(1);

    EXPECT_CALL(*p_libUSBImplMock, libusb_free_device_list(::testing::_, ::testing::_))
        .Times(1);

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("unbindDriver"), _T("{\"deviceName\":\"100\\/001\"}"), response));
    EXPECT_EQ(response, string(""));
}

TEST_F(USBDeviceTest, UnbindDriver_InvalidDeviceName)
{
    Mock_SetSerialNumberInUSBDevicePath();

    ON_CALL(*p_libUSBImplMock, libusb_get_device_list(::testing::_, ::testing::_))
        .WillByDefault([](libusb_context *ctx, libusb_device ***list) {
            struct libusb_device **ret = (struct libusb_device **)malloc(1 * sizeof(struct libusb_device *));
            ret[0] = (struct libusb_device *)malloc(sizeof(struct libusb_device));
            ret[0]->bus_number = MOCK_USB_DEVICE_BUS_NUMBER_1;
            ret[0]->device_address = MOCK_USB_DEVICE_ADDRESS_1;
            ret[0]->port_number = MOCK_USB_DEVICE_PORT_1;
            *list = ret;
            return (ssize_t)1;
        });

    ON_CALL(*p_libUSBImplMock, libusb_free_device_list(::testing::_, ::testing::_))
        .WillByDefault([](libusb_device **list, int unref_devices) {
            free(list[0]);
            free(list);
        });

    EXPECT_CALL(*p_libUSBImplMock, libusb_get_device_list(::testing::_, ::testing::_))
        .Times(1);

    EXPECT_CALL(*p_libUSBImplMock, libusb_free_device_list(::testing::_, ::testing::_))
        .Times(1);

    EXPECT_CALL(*p_libUSBImplMock, libusb_open(::testing::_, ::testing::_))
        .Times(0);

    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("unbindDriver"), _T("{\"deviceName\":\"999\\/999\"}"), response));
    EXPECT_EQ(response, string(""));
}

TEST_F(USBDeviceTest, UnbindDriver_OpenDeviceFailure)
{
    Mock_SetSerialNumberInUSBDevicePath();

    ON_CALL(*p_libUSBImplMock, libusb_get_device_list(::testing::_, ::testing::_))
        .WillByDefault([](libusb_context *ctx, libusb_device ***list) {
            struct libusb_device **ret = (struct libusb_device **)malloc(1 * sizeof(struct libusb_device *));
            ret[0] = (struct libusb_device *)malloc(sizeof(struct libusb_device));
            ret[0]->bus_number = MOCK_USB_DEVICE_BUS_NUMBER_1;
            ret[0]->device_address = MOCK_USB_DEVICE_ADDRESS_1;
            ret[0]->port_number = MOCK_USB_DEVICE_PORT_1;
            *list = ret;
            return (ssize_t)1;
        });

    ON_CALL(*p_libUSBImplMock, libusb_free_device_list(::testing::_, ::testing::_))
        .WillByDefault([](libusb_device **list, int unref_devices) {
            free(list[0]);
            free(list);
        });

    EXPECT_CALL(*p_libUSBImplMock, libusb_get_device_address(::testing::_))
        .WillRepeatedly([](libusb_device *dev) {
            return dev->device_address;
        });

    EXPECT_CALL(*p_libUSBImplMock, libusb_get_bus_number(::testing::_))
        .WillRepeatedly([](libusb_device *dev) {
            return dev->bus_number;
        });

    EXPECT_CALL(*p_libUSBImplMock, libusb_open(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(LIBUSB_ERROR_ACCESS));

    EXPECT_CALL(*p_libUSBImplMock, libusb_close(::testing::_))
        .Times(0);

    EXPECT_CALL(*p_libUSBImplMock, libusb_free_device_list(::testing::_, ::testing::_))
        .Times(1);

    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("unbindDriver"), _T("{\"deviceName\":\"100\\/001\"}"), response));
    EXPECT_EQ(response, string(""));
}

TEST_F(USBDeviceTest, UnbindDriver_KernelDriverActiveFailure)
{
    Mock_SetSerialNumberInUSBDevicePath();

    ON_CALL(*p_libUSBImplMock, libusb_get_device_list(::testing::_, ::testing::_))
        .WillByDefault([](libusb_context *ctx, libusb_device ***list) {
            struct libusb_device **ret = (struct libusb_device **)malloc(1 * sizeof(struct libusb_device *));
            ret[0] = (struct libusb_device *)malloc(sizeof(struct libusb_device));
            ret[0]->bus_number = MOCK_USB_DEVICE_BUS_NUMBER_1;
            ret[0]->device_address = MOCK_USB_DEVICE_ADDRESS_1;
            ret[0]->port_number = MOCK_USB_DEVICE_PORT_1;
            *list = ret;
            return (ssize_t)1;
        });

    ON_CALL(*p_libUSBImplMock, libusb_free_device_list(::testing::_, ::testing::_))
        .WillByDefault([](libusb_device **list, int unref_devices) {
            free(list[0]);
            free(list);
        });

    EXPECT_CALL(*p_libUSBImplMock, libusb_get_device_address(::testing::_))
        .WillRepeatedly([](libusb_device *dev) {
            return dev->device_address;
        });

    EXPECT_CALL(*p_libUSBImplMock, libusb_get_bus_number(::testing::_))
        .WillRepeatedly([](libusb_device *dev) {
            return dev->bus_number;
        });

    EXPECT_CALL(*p_libUSBImplMock, libusb_kernel_driver_active(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(LIBUSB_ERROR_NO_DEVICE));

    EXPECT_CALL(*p_libUSBImplMock, libusb_close(::testing::_))
        .Times(1);

    EXPECT_CALL(*p_libUSBImplMock, libusb_free_device_list(::testing::_, ::testing::_))
        .Times(1);

    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("unbindDriver"), _T("{\"deviceName\":\"100\\/001\"}"), response));
    EXPECT_EQ(response, string(""));
}

TEST_F(USBDeviceTest, UnbindDriver_DetachDriverFailure)
{
    Mock_SetSerialNumberInUSBDevicePath();

    ON_CALL(*p_libUSBImplMock, libusb_get_device_list(::testing::_, ::testing::_))
        .WillByDefault([](libusb_context *ctx, libusb_device ***list) {
            struct libusb_device **ret = (struct libusb_device **)malloc(1 * sizeof(struct libusb_device *));
            ret[0] = (struct libusb_device *)malloc(sizeof(struct libusb_device));
            ret[0]->bus_number = MOCK_USB_DEVICE_BUS_NUMBER_1;
            ret[0]->device_address = MOCK_USB_DEVICE_ADDRESS_1;
            ret[0]->port_number = MOCK_USB_DEVICE_PORT_1;
            *list = ret;
            return (ssize_t)1;
        });

    ON_CALL(*p_libUSBImplMock, libusb_free_device_list(::testing::_, ::testing::_))
        .WillByDefault([](libusb_device **list, int unref_devices) {
            free(list[0]);
            free(list);
        });

    EXPECT_CALL(*p_libUSBImplMock, libusb_get_device_address(::testing::_))
        .WillRepeatedly([](libusb_device *dev) {
            return dev->device_address;
        });

    EXPECT_CALL(*p_libUSBImplMock, libusb_get_bus_number(::testing::_))
        .WillRepeatedly([](libusb_device *dev) {
            return dev->bus_number;
        });

    EXPECT_CALL(*p_libUSBImplMock, libusb_kernel_driver_active(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(1));

    EXPECT_CALL(*p_libUSBImplMock, libusb_detach_kernel_driver(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(LIBUSB_ERROR_NOT_FOUND));

    EXPECT_CALL(*p_libUSBImplMock, libusb_close(::testing::_))
        .Times(1);

    EXPECT_CALL(*p_libUSBImplMock, libusb_free_device_list(::testing::_, ::testing::_))
        .Times(1);

    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("unbindDriver"), _T("{\"deviceName\":\"100\\/001\"}"), response));
    EXPECT_EQ(response, string(""));
}

TEST_F(USBDeviceTest, UnbindDriver_NoDevicesAvailable)
{
    EXPECT_CALL(*p_libUSBImplMock, libusb_get_device_list(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(0));

    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("unbindDriver"), _T("{\"deviceName\":\"100\\/001\"}"), response));
    EXPECT_EQ(response, string(""));
}
/*Test cases for unbindDriver ends here*/

/*******************************************************************************************************************
 * Test function for :getDeviceInfo
 * getDeviceInfo :
 *                Gets a usb device details
 *
 *                @return Response object containing the usb device info
 * Use case coverage:
 *                @Success :10 (various string descriptor scenarios including missing/empty descriptors)
 *                @Failure :3 (invalid device name, no devices, descriptor retrieval failures)
 ********************************************************************************************************************/
/**
 * @brief : Basic getDeviceInfo tests covering error conditions
 *          Tests invalid device names, missing devices, and descriptor failures
 *
 * @param[out]   :  Extended USB Device Info with vendor, product, serial number details
 * @return      :  ERROR_GENERAL for failures, ERROR_NONE for success cases
 */

 /* Basic tests */

TEST_F(USBDeviceTest, GetDeviceInfo_InvalidDeviceName)
{
    Mock_SetSerialNumberInUSBDevicePath();
    Mock_SetDeviceDesc(MOCK_USB_DEVICE_BUS_NUMBER_1, MOCK_USB_DEVICE_ADDRESS_1);

    ON_CALL(*p_libUSBImplMock, libusb_get_device_list(::testing::_, ::testing::_))
        .WillByDefault([](libusb_context *ctx, libusb_device ***list) {
            struct libusb_device **ret = (struct libusb_device **)malloc(1 * sizeof(struct libusb_device *));
            ret[0] = (struct libusb_device *)malloc(sizeof(struct libusb_device));
            ret[0]->bus_number = MOCK_USB_DEVICE_BUS_NUMBER_1;
            ret[0]->device_address = MOCK_USB_DEVICE_ADDRESS_1;
            ret[0]->port_number = MOCK_USB_DEVICE_PORT_1;
            *list = ret;
            return (ssize_t)1;
        });

    ON_CALL(*p_libUSBImplMock, libusb_free_device_list(::testing::_, ::testing::_))
        .WillByDefault([](libusb_device **list, int unref_devices) {
            free(list[0]);
            free(list);
        });

    EXPECT_CALL(*p_libUSBImplMock, libusb_get_device_list(::testing::_, ::testing::_))
        .Times(1);

    EXPECT_CALL(*p_libUSBImplMock, libusb_free_device_list(::testing::_, ::testing::_))
        .Times(1);

    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("getDeviceInfo"), _T("{\"deviceName\":\"999\\/999\"}"), response));
    EXPECT_EQ(response, string(""));
}

TEST_F(USBDeviceTest, GetDeviceInfo_NoDevicesAvailable)
{
    EXPECT_CALL(*p_libUSBImplMock, libusb_get_device_list(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(0));

    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("getDeviceInfo"), _T("{\"deviceName\":\"100\\/001\"}"), response));
    EXPECT_EQ(response, string(""));
}

TEST_F(USBDeviceTest, GetDeviceInfo_GetDescriptorFailure)
{
    Mock_SetSerialNumberInUSBDevicePath();

    ON_CALL(*p_libUSBImplMock, libusb_get_device_list(::testing::_, ::testing::_))
        .WillByDefault([](libusb_context *ctx, libusb_device ***list) {
            struct libusb_device **ret = (struct libusb_device **)malloc(1 * sizeof(struct libusb_device *));
            ret[0] = (struct libusb_device *)malloc(sizeof(struct libusb_device));
            ret[0]->bus_number = MOCK_USB_DEVICE_BUS_NUMBER_1;
            ret[0]->device_address = MOCK_USB_DEVICE_ADDRESS_1;
            ret[0]->port_number = MOCK_USB_DEVICE_PORT_1;
            *list = ret;
            return (ssize_t)1;
        });

    ON_CALL(*p_libUSBImplMock, libusb_free_device_list(::testing::_, ::testing::_))
        .WillByDefault([](libusb_device **list, int unref_devices) {
            free(list[0]);
            free(list);
        });

    EXPECT_CALL(*p_libUSBImplMock, libusb_get_device_descriptor(::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(LIBUSB_ERROR_NO_DEVICE));

    EXPECT_CALL(*p_libUSBImplMock, libusb_get_device_address(::testing::_))
        .WillRepeatedly([](libusb_device *dev) {
            return dev->device_address;
        });

    EXPECT_CALL(*p_libUSBImplMock, libusb_get_bus_number(::testing::_))
        .WillRepeatedly([](libusb_device *dev) {
            return dev->bus_number;
        });

    EXPECT_CALL(*p_libUSBImplMock, libusb_free_device_list(::testing::_, ::testing::_))
        .Times(1);

    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("getDeviceInfo"), _T("{\"deviceName\":\"100\\/001\"}"), response));
    EXPECT_EQ(response, string(""));
}

/* Advanced tests with specific setup */
class USBDeviceInfoTestFixture : public USBDeviceTest {
protected:
    struct libusb_config_descriptor *temp_config_desc = nullptr;

    enum class StringDescriptorBehavior {
        SUCCESS_ALL_DESCRIPTORS,
        NO_MANUFACTURER,
        NO_PRODUCT,
        NO_SERIAL_NUMBER,
        NEGATIVE_RETURN_WITH_ASCII_FALLBACK,
        NEGATIVE_RETURN_ASCII_ALSO_FAILS,
        WRONG_DESCRIPTOR_TYPE,
        INVALID_DESCRIPTOR_LENGTH,
        EMPTY_STRING_DESCRIPTOR,
        ASCII_ZERO_LENGTH
    };

    void SetupBasicDeviceForInfo() {
        Mock_SetSerialNumberInUSBDevicePath();
        Mock_SetDeviceDesc(MOCK_USB_DEVICE_BUS_NUMBER_1, MOCK_USB_DEVICE_ADDRESS_1);

        ON_CALL(*p_libUSBImplMock, libusb_get_device_list(::testing::_, ::testing::_))
            .WillByDefault([](libusb_context *ctx, libusb_device ***list) {
                struct libusb_device **ret = (struct libusb_device **)malloc(1 * sizeof(struct libusb_device *));
                ret[0] = (struct libusb_device *)malloc(sizeof(struct libusb_device));
                ret[0]->bus_number = MOCK_USB_DEVICE_BUS_NUMBER_1;
                ret[0]->device_address = MOCK_USB_DEVICE_ADDRESS_1;
                ret[0]->port_number = MOCK_USB_DEVICE_PORT_1;
                *list = ret;
                return (ssize_t)1;
            });

        ON_CALL(*p_libUSBImplMock, libusb_free_device_list(::testing::_, ::testing::_))
            .WillByDefault([](libusb_device **list, int unref_devices) {
                free(list[0]);
                free(list);
            });

        EXPECT_CALL(*p_libUSBImplMock, libusb_get_device_address(::testing::_))
            .WillRepeatedly([](libusb_device *dev) {
                return dev->device_address;
            });

        EXPECT_CALL(*p_libUSBImplMock, libusb_get_bus_number(::testing::_))
            .WillRepeatedly([](libusb_device *dev) {
                return dev->bus_number;
            });

        EXPECT_CALL(*p_libUSBImplMock, libusb_get_port_numbers(::testing::_, ::testing::_, ::testing::_))
            .WillRepeatedly([](libusb_device *dev, uint8_t *port_numbers, int port_numbers_len) {
                if((nullptr != dev) && (nullptr != port_numbers)) {
                    port_numbers[0] = dev->port_number;
                    return 1;
                }
                return 0;
            });
    }

    void SetupDeviceDescriptor(bool includeManufacturer = true, bool includeProduct = true, bool includeSerialNumber = true) {
        EXPECT_CALL(*p_libUSBImplMock, libusb_get_device_descriptor(::testing::_, ::testing::_))
            .WillRepeatedly([includeManufacturer, includeProduct, includeSerialNumber](libusb_device *dev, struct libusb_device_descriptor *desc) {
                desc->bDeviceSubClass = LIBUSB_CLASS_MASS_STORAGE;
                desc->bDeviceClass = LIBUSB_CLASS_MASS_STORAGE;
                desc->idVendor = 0x1234;
                desc->idProduct = 0x5678;
                desc->iManufacturer = includeManufacturer ? 1 : 0;
                desc->iProduct = includeProduct ? 2 : 0;
                desc->iSerialNumber = includeSerialNumber ? 3 : 0;
                return LIBUSB_SUCCESS;
            });
    }

    void SetupConfigDescriptor() {
        ON_CALL(*p_libUSBImplMock, libusb_get_active_config_descriptor(::testing::_, ::testing::_))
            .WillByDefault([this](libusb_device* pDev, struct libusb_config_descriptor** config_desc) {
                *config_desc = (libusb_config_descriptor *)malloc(sizeof(libusb_config_descriptor));
                temp_config_desc = *config_desc;
                (*config_desc)->bmAttributes = LIBUSB_CONFIG_ATT_BUS_POWERED;
                return (int)LIBUSB_SUCCESS;
            });
    }

    void SetupStringDescriptorBehavior(StringDescriptorBehavior behavior) {
        switch (behavior) {
            case StringDescriptorBehavior::SUCCESS_ALL_DESCRIPTORS:
                ON_CALL(*p_libUSBImplMock, libusb_get_string_descriptor(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
                    .WillByDefault([](libusb_device_handle *dev_handle, uint8_t desc_index, uint16_t langid, unsigned char *data, int length) {
                        data[1] = LIBUSB_DT_STRING;
                        if (desc_index == 0) {
                            data[0] = 4;
                            data[3] = 0x04;
                            data[2] = 0x09;
                        } else if (desc_index == 1) {
                            const char *buf = MOCK_USB_DEVICE_MANUFACTURER;
                            int buffer_len = strlen(buf) * 2, j = 0, index = 2;
                            memset(&data[2], 0, length - 2);
                            while((data[index] = buf[j++]) != '\0') {
                                index += 2;
                            }
                            data[0] = buffer_len + 2;
                        } else if (desc_index == 2) {
                            const char *buf = MOCK_USB_DEVICE_PRODUCT;
                            int buffer_len = strlen(buf) * 2, j = 0, index = 2;
                            memset(&data[2], 0, length - 2);
                            while((data[index] = buf[j++]) != '\0') {
                                index += 2;
                            }
                            data[0] = buffer_len + 2;
                        } else if (desc_index == 3) {
                            const char *buf = MOCK_USB_DEVICE_SERIAL_NO;
                            int buffer_len = strlen(buf) * 2, j = 0, index = 2;
                            memset(&data[2], 0, length - 2);
                            while((data[index] = buf[j++]) != '\0') {
                                index += 2;
                            }
                            data[0] = buffer_len + 2;
                        }
                        return (int)data[0];
                    });
                break;

            case StringDescriptorBehavior::NO_MANUFACTURER:
            case StringDescriptorBehavior::NO_PRODUCT:
            case StringDescriptorBehavior::NO_SERIAL_NUMBER:
                ON_CALL(*p_libUSBImplMock, libusb_get_string_descriptor(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
                    .WillByDefault([behavior](libusb_device_handle *dev_handle, uint8_t desc_index, uint16_t langid, unsigned char *data, int length) {
                        data[1] = LIBUSB_DT_STRING;
                        if (desc_index == 0) {
                            data[0] = 4;
                            data[3] = 0x04;
                            data[2] = 0x09;
                        } else if (desc_index == 2 && behavior != StringDescriptorBehavior::NO_PRODUCT) {
                            const char *buf = MOCK_USB_DEVICE_PRODUCT;
                            int buffer_len = strlen(buf) * 2, j = 0, index = 2;
                            memset(&data[2], 0, length - 2);
                            while((data[index] = buf[j++]) != '\0') {
                                index += 2;
                            }
                            data[0] = buffer_len + 2;
                        } else if (desc_index == 3 && behavior != StringDescriptorBehavior::NO_SERIAL_NUMBER) {
                            const char *buf = MOCK_USB_DEVICE_SERIAL_NO;
                            int buffer_len = strlen(buf) * 2, j = 0, index = 2;
                            memset(&data[2], 0, length - 2);
                            while((data[index] = buf[j++]) != '\0') {
                                index += 2;
                            }
                            data[0] = buffer_len + 2;
                        } else if (desc_index == 1 && behavior != StringDescriptorBehavior::NO_MANUFACTURER) {
                            const char *buf = MOCK_USB_DEVICE_MANUFACTURER;
                            int buffer_len = strlen(buf) * 2, j = 0, index = 2;
                            memset(&data[2], 0, length - 2);
                            while((data[index] = buf[j++]) != '\0') {
                                index += 2;
                            }
                            data[0] = buffer_len + 2;
                        }
                        return (int)data[0];
                    });
                break;

            case StringDescriptorBehavior::NEGATIVE_RETURN_WITH_ASCII_FALLBACK:
                EXPECT_CALL(*p_libUSBImplMock, libusb_get_string_descriptor(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
                    .WillRepeatedly(::testing::Return(LIBUSB_ERROR_PIPE));

                EXPECT_CALL(*p_libUSBImplMock, libusb_get_string_descriptor_ascii(::testing::_, ::testing::_, ::testing::_, ::testing::_))
                    .WillRepeatedly([](libusb_device_handle *dev_handle, uint8_t desc_index, unsigned char *data, int length) {
                        if (desc_index == 1) {
                            strcpy((char*)data, MOCK_USB_DEVICE_MANUFACTURER);
                        } else if (desc_index == 2) {
                            strcpy((char*)data, MOCK_USB_DEVICE_PRODUCT);
                        } else if (desc_index == 3) {
                            strcpy((char*)data, MOCK_USB_DEVICE_SERIAL_NO);
                        }
                        return strlen((char*)data);
                    });
                break;

            case StringDescriptorBehavior::NEGATIVE_RETURN_ASCII_ALSO_FAILS:
                EXPECT_CALL(*p_libUSBImplMock, libusb_get_string_descriptor(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
                    .WillRepeatedly(::testing::Return(LIBUSB_ERROR_PIPE));

                EXPECT_CALL(*p_libUSBImplMock, libusb_get_string_descriptor_ascii(::testing::_, ::testing::_, ::testing::_, ::testing::_))
                    .WillRepeatedly(::testing::Return(LIBUSB_ERROR_NO_DEVICE));
                break;

            case StringDescriptorBehavior::WRONG_DESCRIPTOR_TYPE:
                EXPECT_CALL(*p_libUSBImplMock, libusb_get_string_descriptor(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
                    .WillRepeatedly([](libusb_device_handle *dev_handle, uint8_t desc_index, uint16_t langid, unsigned char *data, int length) {
                        data[0] = 10;
                        data[1] = LIBUSB_DT_DEVICE;
                        return (int)data[0];
                    });
                break;

            case StringDescriptorBehavior::INVALID_DESCRIPTOR_LENGTH:
                EXPECT_CALL(*p_libUSBImplMock, libusb_get_string_descriptor(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
                    .WillRepeatedly([](libusb_device_handle *dev_handle, uint8_t desc_index, uint16_t langid, unsigned char *data, int length) {
                        data[0] = 20;
                        data[1] = LIBUSB_DT_STRING;
                        return 10;
                    });
                break;

            case StringDescriptorBehavior::EMPTY_STRING_DESCRIPTOR:
                ON_CALL(*p_libUSBImplMock, libusb_get_string_descriptor(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
                    .WillByDefault([](libusb_device_handle *dev_handle, uint8_t desc_index, uint16_t langid, unsigned char *data, int length) {
                        data[1] = LIBUSB_DT_STRING;
                        if (desc_index == 0) {
                            data[0] = 4;
                            data[3] = 0x04;
                            data[2] = 0x09;
                        } else {
                            data[0] = 2;
                        }
                        return (int)data[0];
                    });
                break;

            case StringDescriptorBehavior::ASCII_ZERO_LENGTH:
                EXPECT_CALL(*p_libUSBImplMock, libusb_get_string_descriptor(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
                    .WillRepeatedly(::testing::Return(LIBUSB_ERROR_PIPE));

                EXPECT_CALL(*p_libUSBImplMock, libusb_get_string_descriptor_ascii(::testing::_, ::testing::_, ::testing::_, ::testing::_))
                    .WillRepeatedly(::testing::Return(0));
                break;
        }
    }

    void SetupGetDeviceInfoTest(StringDescriptorBehavior behavior, 
                                bool includeManufacturer = true, 
                                bool includeProduct = true, 
                                bool includeSerialNumber = true) {
        SetupBasicDeviceForInfo();
        SetupDeviceDescriptor(includeManufacturer, includeProduct, includeSerialNumber);
        SetupConfigDescriptor();
        SetupStringDescriptorBehavior(behavior);
    }
};

TEST_F(USBDeviceInfoTestFixture, GetDeviceInfo_GetUSBExtInfoStruct_Success_AllDescriptors)
{
    SetupGetDeviceInfoTest(StringDescriptorBehavior::SUCCESS_ALL_DESCRIPTORS);

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getDeviceInfo"), _T("{\"deviceName\":\"100\\/001\"}"), response));
    EXPECT_TRUE(response.find(MOCK_USB_DEVICE_MANUFACTURER) != string::npos);
    EXPECT_TRUE(response.find(MOCK_USB_DEVICE_PRODUCT) != string::npos);
    EXPECT_TRUE(response.find(MOCK_USB_DEVICE_SERIAL_NO) != string::npos);

    string expectedResponse = 
        R"({"parentId":0,"deviceStatus":1,"deviceLevel":0,"portNumber":1,)"
        R"("vendorId":4660,"productId":22136,"protocol":0,"serialNumber":"",)"
        R"("device":{"deviceClass":8,"deviceSubclass":8,"deviceName":"100\/001","devicePath":""},)"
        R"("flags":"AVAILABLE","features":0,"busSpeed":"High","numLanguageIds":1,)"
        R"("productInfo1":{"languageId":1033,"serialNumber":"0401805e4532973503374df52a239c898397d348",)"
        R"("manufacturer":"USB","product":"SanDisk 3.2Gen1"},)"
        R"("productInfo2":{"languageId":0,"serialNumber":"","manufacturer":"","product":""},)"
        R"("productInfo3":{"languageId":0,"serialNumber":"","manufacturer":"","product":""},)"
        R"("productInfo4":{"languageId":0,"serialNumber":"","manufacturer":"","product":""}})";
    
    EXPECT_EQ(response, expectedResponse);
}

TEST_F(USBDeviceInfoTestFixture, GetDeviceInfo_GetUSBExtInfoStruct_NoManufacturerDescriptor)
{
    SetupGetDeviceInfoTest(StringDescriptorBehavior::NO_MANUFACTURER, false, true, true);

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getDeviceInfo"), _T("{\"deviceName\":\"100\\/001\"}"), response));
    EXPECT_TRUE(response.find("\"manufacturer\":\"\"") != string::npos);
}

TEST_F(USBDeviceInfoTestFixture, GetDeviceInfo_GetUSBExtInfoStruct_NoProductDescriptor)
{
    SetupGetDeviceInfoTest(StringDescriptorBehavior::NO_PRODUCT, true, false, true);

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getDeviceInfo"), _T("{\"deviceName\":\"100\\/001\"}"), response));
    EXPECT_TRUE(response.find("\"product\":\"\"") != string::npos);
}

TEST_F(USBDeviceInfoTestFixture, GetDeviceInfo_GetUSBExtInfoStruct_NoSerialNumberDescriptor)
{
    SetupGetDeviceInfoTest(StringDescriptorBehavior::NO_SERIAL_NUMBER, true, true, false);

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getDeviceInfo"), _T("{\"deviceName\":\"100\\/001\"}"), response));
    EXPECT_TRUE(response.find("\"serialNumber\":\"\"") != string::npos);
}

TEST_F(USBDeviceInfoTestFixture, GetDeviceInfo_GetUSBExtInfoStruct_GetStringDescriptorNegativeReturn)
{
    SetupGetDeviceInfoTest(StringDescriptorBehavior::NEGATIVE_RETURN_WITH_ASCII_FALLBACK);

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getDeviceInfo"), _T("{\"deviceName\":\"100\\/001\"}"), response));
    EXPECT_TRUE(response.find(MOCK_USB_DEVICE_MANUFACTURER) != string::npos);
}

TEST_F(USBDeviceInfoTestFixture, GetDeviceInfo_GetUSBExtInfoStruct_GetStringDescriptorAsciiAlsoFails)
{
    SetupGetDeviceInfoTest(StringDescriptorBehavior::NEGATIVE_RETURN_ASCII_ALSO_FAILS);

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getDeviceInfo"), _T("{\"deviceName\":\"100\\/001\"}"), response));
    EXPECT_TRUE(response.find("\"manufacturer\":\"\"") != string::npos);
    EXPECT_TRUE(response.find("\"product\":\"\"") != string::npos);
    EXPECT_TRUE(response.find("\"serialNumber\":\"\"") != string::npos);
}

TEST_F(USBDeviceInfoTestFixture, GetDeviceInfo_GetUSBExtInfoStruct_WrongDescriptorType)
{
    SetupGetDeviceInfoTest(StringDescriptorBehavior::WRONG_DESCRIPTOR_TYPE);

    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("getDeviceInfo"), _T("{\"deviceName\":\"100\\/001\"}"), response));
    EXPECT_EQ(response, string(""));
}

TEST_F(USBDeviceInfoTestFixture, GetDeviceInfo_GetUSBExtInfoStruct_InvalidDescriptorLength)
{
    SetupGetDeviceInfoTest(StringDescriptorBehavior::INVALID_DESCRIPTOR_LENGTH);

    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("getDeviceInfo"), _T("{\"deviceName\":\"100\\/001\"}"), response));
    EXPECT_EQ(response, string(""));
}

TEST_F(USBDeviceInfoTestFixture, GetDeviceInfo_GetUSBExtInfoStruct_EmptyStringDescriptor)
{
    SetupGetDeviceInfoTest(StringDescriptorBehavior::EMPTY_STRING_DESCRIPTOR);

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getDeviceInfo"), _T("{\"deviceName\":\"100\\/001\"}"), response));
    EXPECT_TRUE(response.find("\"manufacturer\":\"\"") != string::npos);
    EXPECT_TRUE(response.find("\"product\":\"\"") != string::npos);
    EXPECT_TRUE(response.find("\"serialNumber\":\"\"") != string::npos);
}

TEST_F(USBDeviceInfoTestFixture, GetDeviceInfo_GetUSBExtInfoStruct_AsciiDescriptorZeroLength)
{
    SetupGetDeviceInfoTest(StringDescriptorBehavior::ASCII_ZERO_LENGTH);

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getDeviceInfo"), _T("{\"deviceName\":\"100\\/001\"}"), response));
    EXPECT_TRUE(response.find("\"manufacturer\":\"\"") != string::npos);
    EXPECT_TRUE(response.find("\"product\":\"\"") != string::npos);
    EXPECT_TRUE(response.find("\"serialNumber\":\"\"") != string::npos);
}

/* End of getDeviceInfo tests */

/*******************************************************************************************************************
 * Test function for :getDeviceInfo and getDeviceList with per-interface class devices
 * Per-interface class devices:
 *                Tests devices that specify their class at the interface level rather than device level
 *                These devices have bDeviceClass = LIBUSB_CLASS_PER_INTERFACE (0)
 *
 *                @return Device information with class/subclass determined from first mass storage interface
 * Use case coverage:
 *                @Success :4 (mass storage interface, no mass storage, multiple interfaces, getDeviceInfo)
 *                @Failure :1 (config descriptor retrieval failure)
 ********************************************************************************************************************/
/**
 * @brief : Tests getDeviceList and getDeviceInfo with per-interface class USB devices
 *          Verifies correct class/subclass detection when device class is defined at interface level
 *          Tests single and multiple interface scenarios, with and without mass storage interfaces
 *
 * @param[out]   :  USB Device Info with class determined from interface descriptors
 * @return      :  ERROR_NONE with appropriate device class based on interfaces
 */

class USBDevicePerInterfaceTestFixture : public USBDeviceTest {
protected:
    void SetupPerInterfaceDeviceBase(uint8_t num_interfaces, const std::vector<std::pair<uint8_t, uint8_t>>& interface_classes) {
        Mock_SetSerialNumberInUSBDevicePath();
        
        ON_CALL(*p_libUSBImplMock, libusb_get_device_list(::testing::_, ::testing::_))
            .WillByDefault([](libusb_context *ctx, libusb_device ***list) {
                libusb_device **ret = (libusb_device **)malloc(1 * sizeof(libusb_device *));
                ret[0] = (libusb_device *)malloc(sizeof(libusb_device));
                ret[0]->bus_number = MOCK_USB_DEVICE_BUS_NUMBER_1;
                ret[0]->device_address = MOCK_USB_DEVICE_ADDRESS_1;
                ret[0]->port_number = MOCK_USB_DEVICE_PORT_1;
                *list = ret;
                return (ssize_t)1;
            });

        ON_CALL(*p_libUSBImplMock, libusb_free_device_list(::testing::_, ::testing::_))
            .WillByDefault([](libusb_device **list, int unref_devices) {
                free(list[0]);
                free(list);
            });

        EXPECT_CALL(*p_libUSBImplMock, libusb_get_device_descriptor(::testing::_, ::testing::_))
            .WillRepeatedly([](libusb_device *dev, struct libusb_device_descriptor *desc) {
                desc->bDeviceClass = LIBUSB_CLASS_PER_INTERFACE;
                desc->bDeviceSubClass = 0;
                desc->idVendor = 0x1234;
                desc->idProduct = 0x5678;
                desc->iManufacturer = 1;
                desc->iProduct = 2;
                desc->iSerialNumber = 3;
                desc->bDeviceProtocol = 0;
                return LIBUSB_SUCCESS;
            });

        EXPECT_CALL(*p_libUSBImplMock, libusb_get_device_address(::testing::_))
            .WillRepeatedly([](libusb_device *dev) {
                return dev->device_address;
            });

        EXPECT_CALL(*p_libUSBImplMock, libusb_get_bus_number(::testing::_))
            .WillRepeatedly([](libusb_device *dev) {
                return dev->bus_number;
            });

        EXPECT_CALL(*p_libUSBImplMock, libusb_get_port_numbers(::testing::_, ::testing::_, ::testing::_))
            .WillRepeatedly([](libusb_device *dev, uint8_t *port_numbers, int port_numbers_len) {
                if((nullptr != dev) && (nullptr != port_numbers)) {
                    port_numbers[0] = dev->port_number;
                    return 1;
                }
                return 0;
            });

        EXPECT_CALL(*p_libUSBImplMock, libusb_get_config_descriptor(::testing::_, ::testing::_, ::testing::_))
            .WillOnce([num_interfaces, interface_classes](libusb_device *dev, uint8_t config_index, libusb_config_descriptor **config) {
                libusb_config_descriptor *cfg = (libusb_config_descriptor *)malloc(sizeof(libusb_config_descriptor));
                cfg->bNumInterfaces = num_interfaces;
                
                libusb_interface *interfaces = (libusb_interface *)malloc(num_interfaces * sizeof(libusb_interface));
                
                for (uint8_t i = 0; i < num_interfaces; i++) {
                    interfaces[i].num_altsetting = 1;
                    
                    libusb_interface_descriptor *altsetting = (libusb_interface_descriptor *)malloc(sizeof(libusb_interface_descriptor));
                    
                    if (i < interface_classes.size()) {
                        altsetting->bInterfaceClass = interface_classes[i].first;
                        altsetting->bInterfaceSubClass = interface_classes[i].second;
                    } else {
                        altsetting->bInterfaceClass = LIBUSB_CLASS_HID;
                        altsetting->bInterfaceSubClass = 0;
                    }
                    
                    interfaces[i].altsetting = altsetting;
                }
                
                cfg->interface = interfaces;
                *config = cfg;
                
                return LIBUSB_SUCCESS;
            });

        EXPECT_CALL(*p_libUSBImplMock, libusb_free_config_descriptor(::testing::_))
            .WillOnce([num_interfaces](libusb_config_descriptor *config) {
                if (config) {
                    if (config->interface) {
                        for (uint8_t i = 0; i < num_interfaces; i++) {
                            if (config->interface[i].altsetting) {
                                free((void*)config->interface[i].altsetting);
                            }
                        }
                        free((void*)config->interface);
                    }
                    free(config);
                }
            });
    }

    void SetupGetDeviceInfoMocks() {
        Mock_SetDeviceDesc(MOCK_USB_DEVICE_BUS_NUMBER_1, MOCK_USB_DEVICE_ADDRESS_1);

        ON_CALL(*p_libUSBImplMock, libusb_get_active_config_descriptor(::testing::_, ::testing::_))
            .WillByDefault([](libusb_device* pDev, libusb_config_descriptor** config_desc) {
                *config_desc = (libusb_config_descriptor *)malloc(sizeof(libusb_config_descriptor));
                (*config_desc)->bmAttributes = LIBUSB_CONFIG_ATT_BUS_POWERED;
                return (int)LIBUSB_SUCCESS;
            });

        ON_CALL(*p_libUSBImplMock, libusb_get_string_descriptor(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
            .WillByDefault([](libusb_device_handle *dev_handle, uint8_t desc_index, uint16_t langid, unsigned char *data, int length) {
                data[1] = LIBUSB_DT_STRING;
                if (desc_index == 0) {
                    data[0] = 4;
                    data[2] = 0x09;
                    data[3] = 0x04;
                } else {
                    data[0] = 10;
                    for (int i = 2; i < 10; i += 2) {
                        data[i] = 'T';
                        data[i+1] = 0;
                    }
                }
                return (int)data[0];
            });
    }

    void SetupPerInterfaceDevice(uint8_t num_interfaces, const std::vector<std::pair<uint8_t, uint8_t>>& interface_classes) {
        SetupPerInterfaceDeviceBase(num_interfaces, interface_classes);
    }

    void SetupPerInterfaceDeviceConfigFailure() {
        Mock_SetSerialNumberInUSBDevicePath();
        
        ON_CALL(*p_libUSBImplMock, libusb_get_device_list(::testing::_, ::testing::_))
            .WillByDefault([](libusb_context *ctx, libusb_device ***list) {
                libusb_device **ret = (libusb_device **)malloc(1 * sizeof(libusb_device *));
                ret[0] = (libusb_device *)malloc(sizeof(libusb_device));
                ret[0]->bus_number = MOCK_USB_DEVICE_BUS_NUMBER_1;
                ret[0]->device_address = MOCK_USB_DEVICE_ADDRESS_1;
                ret[0]->port_number = MOCK_USB_DEVICE_PORT_1;
                *list = ret;
                return (ssize_t)1;
            });

        ON_CALL(*p_libUSBImplMock, libusb_free_device_list(::testing::_, ::testing::_))
            .WillByDefault([](libusb_device **list, int unref_devices) {
                free(list[0]);
                free(list);
            });

        EXPECT_CALL(*p_libUSBImplMock, libusb_get_device_descriptor(::testing::_, ::testing::_))
            .WillRepeatedly([](libusb_device *dev, struct libusb_device_descriptor *desc) {
                desc->bDeviceClass = LIBUSB_CLASS_PER_INTERFACE;
                desc->bDeviceSubClass = 0;
                return LIBUSB_SUCCESS;
            });

        EXPECT_CALL(*p_libUSBImplMock, libusb_get_device_address(::testing::_))
            .WillRepeatedly([](libusb_device *dev) {
                return dev->device_address;
            });

        EXPECT_CALL(*p_libUSBImplMock, libusb_get_bus_number(::testing::_))
            .WillRepeatedly([](libusb_device *dev) {
                return dev->bus_number;
            });

        EXPECT_CALL(*p_libUSBImplMock, libusb_get_port_numbers(::testing::_, ::testing::_, ::testing::_))
            .WillRepeatedly([](libusb_device *dev, uint8_t *port_numbers, int port_numbers_len) {
                if((nullptr != dev) && (nullptr != port_numbers)) {
                    port_numbers[0] = dev->port_number;
                    return 1;
                }
                return 0;
            });

        EXPECT_CALL(*p_libUSBImplMock, libusb_get_config_descriptor(::testing::_, ::testing::_, ::testing::_))
            .WillOnce(::testing::Return(LIBUSB_ERROR_NOT_FOUND));
    }

    void SetupPerInterfaceDeviceForGetDeviceInfo(uint8_t num_interfaces, const std::vector<std::pair<uint8_t, uint8_t>>& interface_classes) {
        SetupPerInterfaceDeviceBase(num_interfaces, interface_classes);
        SetupGetDeviceInfoMocks();
    }
};

TEST_F(USBDevicePerInterfaceTestFixture, GetDeviceList_PerInterfaceClass_MassStorage_Success)
{
    std::vector<std::pair<uint8_t, uint8_t>> interfaces = {{LIBUSB_CLASS_MASS_STORAGE, 6}};
    SetupPerInterfaceDevice(1, interfaces);

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getDeviceList"), _T("{}"), response));
    EXPECT_TRUE(response.find("\"deviceClass\":8") != string::npos);
    EXPECT_TRUE(response.find("\"deviceSubclass\":6") != string::npos);
}

TEST_F(USBDevicePerInterfaceTestFixture, GetDeviceList_PerInterfaceClass_NoMassStorage_Success)
{
    std::vector<std::pair<uint8_t, uint8_t>> interfaces = {{LIBUSB_CLASS_HID, 0}};
    SetupPerInterfaceDevice(1, interfaces);

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getDeviceList"), _T("{}"), response));
    EXPECT_TRUE(response.find("\"deviceClass\":0") != string::npos);
    EXPECT_TRUE(response.find("\"devicePath\":\"\"") != string::npos);
}

TEST_F(USBDevicePerInterfaceTestFixture, GetDeviceList_PerInterfaceClass_GetConfigDescriptorFailure)
{
    SetupPerInterfaceDeviceConfigFailure();

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getDeviceList"), _T("{}"), response));
    EXPECT_TRUE(response.find("\"deviceClass\":0") != string::npos);
}

TEST_F(USBDevicePerInterfaceTestFixture, GetDeviceInfo_PerInterfaceClass_MassStorage_Success)
{
    std::vector<std::pair<uint8_t, uint8_t>> interfaces = {{LIBUSB_CLASS_MASS_STORAGE, 6}};
    SetupPerInterfaceDeviceForGetDeviceInfo(1, interfaces);

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getDeviceInfo"), _T("{\"deviceName\":\"100\\/001\"}"), response));
    EXPECT_TRUE(response.find("\"deviceClass\":8") != string::npos);
    EXPECT_TRUE(response.find("\"deviceSubclass\":6") != string::npos);
}

TEST_F(USBDevicePerInterfaceTestFixture, GetDeviceList_PerInterfaceClass_MultipleInterfaces_Success)
{
    std::vector<std::pair<uint8_t, uint8_t>> interfaces = {
        {LIBUSB_CLASS_HID, 0},
        {LIBUSB_CLASS_MASS_STORAGE, 6}
    };
    SetupPerInterfaceDevice(2, interfaces);

    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("getDeviceList"), _T("{}"), response));
    EXPECT_TRUE(response.find("\"deviceClass\":8") != string::npos);
}
