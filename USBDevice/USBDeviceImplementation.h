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

#pragma once

#include "Module.h"
#include <interfaces/Ids.h>
#include <interfaces/IUSBDevice.h>
#include "tracing/Logging.h"
#include <vector>
#include <thread>
#include <fstream>
#include <com/com.h>
#include <core/core.h>
#include <plugins/plugins.h>
#include "libusb.h"

/**
*   @brief Bit in the port status to check if device is connected or not
*/
#define STATUS_BIT_PORT_CONNECTION          0x0001

/**
*   @brief Bit in the port status to check if port is enabled or not
*/
#define STATUS_BIT_PORT_ENABLED             0x0002

/**
*   @brief  Bit in the port status to check if device connected to the port is suspended or not
*/
#define STATUS_BIT_PORT_DEVICE_SUSPENDED    0x0004

/**
*   @brief  Bit in the port status to check if there is over current in the port
*/
#define STATUS_BIT_PORT_OVER_CURRENT        0x0008

/**
*   @brief Bit in the device status to check if the device is self powered or bus powered
*/
#define STATUS_BIT_DEVICE_SELF_POWERED      0x01

/*
*   Configuration characteristics
*   D7: Reserved (set to one)
*   D6: Self-powered // If the D6 not set, the device is bus powered
*   D5: Remote Wakeup
*   D4...0: Reserved (reset to zero)
*   D7 is reserved and must be set to one for historical reasons.
*/
#define LIBUSB_CONFIG_ATT_SELF_POWERED        0x40
namespace WPEFramework {
namespace Plugin {
    class USBDeviceImplementation : public Exchange::IUSBDevice{
    public:
        // We do not allow this plugin to be copied !!
        USBDeviceImplementation();
        ~USBDeviceImplementation() override;

        static USBDeviceImplementation* instance(USBDeviceImplementation *USBDeviceImpl = nullptr);

        // We do not allow this plugin to be copied !!
        USBDeviceImplementation(const USBDeviceImplementation&) = delete;
        USBDeviceImplementation& operator=(const USBDeviceImplementation&) = delete;

        BEGIN_INTERFACE_MAP(USBDeviceImplementation)
        INTERFACE_ENTRY(Exchange::IUSBDevice)
        END_INTERFACE_MAP

    public:
        enum Event {
                USBDEVICE_HOTPLUG_EVENT_DEVICE_ARRIVED,
                USBDEVICE_HOTPLUG_EVENT_DEVICE_LEFT
            };

        class EXTERNAL Job : public Core::IDispatch {
        protected:
             Job(USBDeviceImplementation *usbDeviceImplementation, Event event, Exchange::IUSBDevice::USBDevice &usbDevice)
                : _usbDeviceImplementation(usbDeviceImplementation)
                , _event(event)
                , _usbDevice(usbDevice) {
                if (_usbDeviceImplementation != nullptr) {
                    _usbDeviceImplementation->AddRef();
                }
            }

       public:
            Job() = delete;
            Job(const Job&) = delete;
            Job& operator=(const Job&) = delete;
            ~Job() {
                if (_usbDeviceImplementation != nullptr) {
                    _usbDeviceImplementation->Release();
                }
            }

       public:
            static Core::ProxyType<Core::IDispatch> Create(USBDeviceImplementation *usbDeviceImplementation, Event event, Exchange::IUSBDevice::USBDevice usbDevice) {
#ifndef USE_THUNDER_R4
                return (Core::proxy_cast<Core::IDispatch>(Core::ProxyType<Job>::Create(usbDeviceImplementation, event, usbDevice)));
#else
                return (Core::ProxyType<Core::IDispatch>(Core::ProxyType<Job>::Create(usbDeviceImplementation, event, usbDevice)));
#endif
            }

            virtual void Dispatch() {
                _usbDeviceImplementation->Dispatch(_event, _usbDevice);
            }
        private:
            USBDeviceImplementation *_usbDeviceImplementation;
            const Event _event;
            const Exchange::IUSBDevice::USBDevice _usbDevice;
        };

    public:
        virtual Core::hresult Register(Exchange::IUSBDevice::INotification *notification ) override ;
        virtual Core::hresult Unregister(Exchange::IUSBDevice::INotification *notification ) override ;
        Core::hresult GetDeviceList(IUSBDeviceIterator*& devices) const override;
        Core::hresult GetDeviceInfo(const string &deviceName, USBDeviceInfo& deviceInfo) const override;
        Core::hresult BindDriver(const string &deviceName) const override;
        Core::hresult UnbindDriver(const string &deviceName) const override;
  
    private:
        void dispatchEvent(Event, const Exchange::IUSBDevice::USBDevice usbDevice);
        void Dispatch(Event event, const Exchange::IUSBDevice::USBDevice usbDevice);
        uint32_t libUSBInit(void);
        void libUSBClose(void);
        void getUSBDevicClassFromInterfaceDescriptor(libusb_device *pDev, uint8_t &bDeviceClass, uint8_t &bDeviceSubClass);
        void getDevicePathFromDevice(libusb_device *pDev, string &devPath, string& deviceSerialNumber);
        void getDeviceSerialNumber(libusb_device *pDev, string &serialNumber);
        uint32_t getUSBDescriptorValue(libusb_device_handle *handle, uint16_t languageID, int descriptorIndex, std::string &stringDescriptor);
        uint32_t getUSBExtInfoStructFromDeviceDescriptor(libusb_device *pDev,
                                                         struct libusb_device_descriptor *pDesc,
                                                         Exchange::IUSBDevice::USBDeviceInfo *pUSBDeviceInfo);
        uint32_t getUSBDeviceInfoStructFromDeviceDescriptor(libusb_device *pDev, Exchange::IUSBDevice::USBDeviceInfo *pUSBDeviceInfo);
        uint32_t getUSBDeviceStructFromDeviceDescriptor(libusb_device *pDev, Exchange::IUSBDevice::USBDevice *pusbDevice);
        static int libUSBHotPlugCallbackDeviceAttached(libusb_context *ctx, libusb_device *dev, libusb_hotplug_event event, void *user_data);
        static int libUSBHotPlugCallbackDeviceDetached(libusb_context *ctx, libusb_device *dev, libusb_hotplug_event event, void *user_data);
        void libUSBEventsHandlingThread(void);

    private:
        mutable Core::CriticalSection _adminLock;
        std::thread *_libUSBDeviceThread;
        std::list<Exchange::IUSBDevice::INotification*> _usbDeviceNotification;
        bool _handlingUSBDeviceEvents;
        libusb_hotplug_callback_handle _hotPlugHandle[2];

        friend class Job;
    };
} // namespace Plugin
} // namespace WPEFramework
