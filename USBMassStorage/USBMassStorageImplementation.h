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
#include <interfaces/IUSBMassStorage.h>
#include <interfaces/IConfiguration.h>

#include "tracing/Logging.h"
#include <vector>

#include <com/com.h>
#include <core/core.h>
#include <plugins/plugins.h>
#include "libusb.h"

#include <cstring>
#include <sys/ioctl.h>
#include <unistd.h>
using std::ofstream;
#include <cstdlib>
#include <iostream>
#include <sys/vfs.h>

#undef MS_RDONLY
#include <sys/mount.h>

using USBDevice = WPEFramework::Exchange::IUSBDevice::USBDevice;
using USBStorageFileSystem = WPEFramework::Exchange::IUSBMassStorage::USBStorageFileSystem;
using USBStorageDeviceInfo = WPEFramework::Exchange::IUSBMassStorage::USBStorageDeviceInfo;
using USBStorageMountFlags = WPEFramework::Exchange::IUSBMassStorage::USBStorageMountFlags;

namespace WPEFramework {
namespace Plugin {
    class USBMassStorageImplementation : public Exchange::IUSBMassStorage,
                                         public Exchange::IConfiguration {
    private:
        class USBDeviceNotification : public Exchange::IUSBDevice::INotification {
        private:
            USBDeviceNotification(const USBDeviceNotification&) = delete;
            USBDeviceNotification& operator=(const USBDeviceNotification&) = delete;

        public:
            explicit USBDeviceNotification(USBMassStorageImplementation& parent)
                : _parent(parent)
            {
            }
            ~USBDeviceNotification() override = default;

            void OnDevicePluggedIn(const USBDevice &device)
            {
                _parent.OnDevicePluggedIn(device);
            }

            void OnDevicePluggedOut(const USBDevice &device)
            {
                _parent.OnDevicePluggedOut(device);
            }

            BEGIN_INTERFACE_MAP(USBDeviceNotification)
            INTERFACE_ENTRY(Exchange::IUSBDevice::INotification)
            END_INTERFACE_MAP

        private:
            USBMassStorageImplementation& _parent;
        };
    public:
        // We do not allow this plugin to be copied !!
        USBMassStorageImplementation();
        ~USBMassStorageImplementation() override;

        static USBMassStorageImplementation* instance(USBMassStorageImplementation *USBMassStorageImpl = nullptr);

        // We do not allow this plugin to be copied !!
        USBMassStorageImplementation(const USBMassStorageImplementation&) = delete;
        USBMassStorageImplementation& operator=(const USBMassStorageImplementation&) = delete;

        BEGIN_INTERFACE_MAP(USBMassStorageImplementation)
        INTERFACE_ENTRY(Exchange::IUSBMassStorage)
        INTERFACE_ENTRY(Exchange::IConfiguration)
        END_INTERFACE_MAP

    public:
        enum Event
        {
            USB_STORAGE_EVENT_MOUNT,
            USB_STORAGE_EVENT_UNMOUNT
        };

        class EXTERNAL Job : public Core::IDispatch {
        protected:
            Job(USBMassStorageImplementation* usbMass_StorageImplementation, Event event,
            USBStorageDeviceInfo params)
                : _usbMass_StorageImplementation(usbMass_StorageImplementation)
                , _event(event)
                , _params(params) {
                if (_usbMass_StorageImplementation != nullptr) {
                    _usbMass_StorageImplementation->AddRef();
                }
            }

       public:
            Job() = delete;
            Job(const Job&) = delete;
            Job& operator=(const Job&) = delete;
            ~Job() {
                if (_usbMass_StorageImplementation != nullptr) {
                    _usbMass_StorageImplementation->Release();
                }
            }

       public:
            static Core::ProxyType<Core::IDispatch> Create(USBMassStorageImplementation* usb_mass_storageImplementation, Event event, USBStorageDeviceInfo params ) {
#ifndef USE_THUNDER_R4
                return (Core::proxy_cast<Core::IDispatch>(Core::ProxyType<Job>::Create(usb_mass_storageImplementation, event, params)));
#else
                return (Core::ProxyType<Core::IDispatch>(Core::ProxyType<Job>::Create(usb_mass_storageImplementation, event, params)));
#endif
            }

            virtual void Dispatch() {
                _usbMass_StorageImplementation->Dispatch(_event, _params);
            }
        private:
            USBMassStorageImplementation *_usbMass_StorageImplementation;
            const Event _event;
            USBStorageDeviceInfo  _params;
        };
    public:
        virtual Core::hresult Register(Exchange::IUSBMassStorage::INotification *notification ) override ;
        virtual Core::hresult Unregister(Exchange::IUSBMassStorage::INotification *notification ) override;

        Core::hresult GetDeviceList(Exchange::IUSBMassStorage::IUSBStorageDeviceInfoIterator*& deviceInfo) const override;
        Core::hresult GetMountPoints(const string &deviceName, Exchange::IUSBMassStorage::IUSBStorageMountInfoIterator*& mountPoints) const override;
        Core::hresult GetPartitionInfo(const string &mountPath , Exchange::IUSBMassStorage::USBStoragePartitionInfo& partitionInfo ) const override;

        // IConfiguration methods
        uint32_t Configure(PluginHost::IShell* service) override;

        static USBMassStorageImplementation* _instance;
        static bool directoryExists(const string& path);

        void OnDevicePluggedIn(const USBDevice &device);
        void OnDevicePluggedOut(const USBDevice &device);
        void registerEventHandlers();

    private:
        std::list<USBStorageDeviceInfo> usbStorageDeviceInfo;
        std::map<string /*<device name>*/,std::list<Exchange::IUSBMassStorage::USBStorageMountInfo>> usbStorageMountInfo;

        mutable Core::CriticalSection _adminLock;
        Exchange::IUSBDevice* _remoteUSBDeviceObject;
        std::list<Exchange::IUSBMassStorage::INotification*> _usbStorageNotification;
        Core::Sink<USBDeviceNotification> _USB_DeviceNotification;
        bool _registeredEventHandlers;
        PluginHost::IShell* _service;

        void MountDevicesOnBootUp();
        bool DeviceMount(const USBStorageDeviceInfo &storageDeviceInfo);
        void Dispatch(Event event, USBStorageDeviceInfo params);
        void DispatchMountEvent(const USBStorageDeviceInfo &storageDeviceInfo);
        void DispatchUnMountEvent(USBStorageDeviceInfo &storageDeviceInfo);
        USBStorageFileSystem GetFileSystemType(__fsword_t f_type) const;

        friend class Job;
    };
} // namespace Plugin
} // namespace WPEFramework
