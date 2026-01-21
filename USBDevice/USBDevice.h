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
#include <interfaces/json/JsonData_USBDevice.h>
#include <interfaces/json/JUSBDevice.h>
#include <interfaces/IUSBDevice.h>
#include "UtilsLogging.h"
#include "tracing/Logging.h"
#include <mutex>

namespace WPEFramework {
namespace Plugin {

    class USBDevice: public PluginHost::IPlugin, public PluginHost::JSONRPC
    {
     private:
        class Notification : public RPC::IRemoteConnection::INotification,
                             public Exchange::IUSBDevice::INotification
        {
            private:
                Notification() = delete;
                Notification(const Notification&) = delete;
                Notification& operator=(const Notification&) = delete;

            public:
            explicit Notification(USBDevice* parent)
                : _parent(*parent)
                {
                    ASSERT(parent != nullptr);
                }

                virtual ~Notification()
                {
                }

                BEGIN_INTERFACE_MAP(Notification)
                INTERFACE_ENTRY(Exchange::IUSBDevice::INotification)
                INTERFACE_ENTRY(RPC::IRemoteConnection::INotification)
                END_INTERFACE_MAP

                void Activated(RPC::IRemoteConnection*) override
                {
                }

                void Deactivated(RPC::IRemoteConnection *connection) override
                {
                   _parent.Deactivated(connection);
                }

                void OnDevicePluggedIn(const Exchange::IUSBDevice::USBDevice &device) override
                {
                    LOGINFO("OnDevicePluggedIn");
                    Exchange::JUSBDevice::Event::OnDevicePluggedIn(_parent, device);
                }

                void OnDevicePluggedOut(const Exchange::IUSBDevice::USBDevice &device) override
                {
                    LOGINFO("OnDevicePluggedOut");
                    Exchange::JUSBDevice::Event::OnDevicePluggedOut(_parent, device);
                }

            private:
                USBDevice& _parent;
        };

        public:
            // We do not allow this plugin to be copied !!
            USBDevice(const USBDevice&) = delete;
            USBDevice& operator=(const USBDevice&) = delete;

            USBDevice();
            virtual ~USBDevice();

            BEGIN_INTERFACE_MAP(USBDevice)
            INTERFACE_ENTRY(PluginHost::IPlugin)
            INTERFACE_ENTRY(PluginHost::IDispatcher)
            INTERFACE_AGGREGATE(Exchange::IUSBDevice, _usbDeviceImpl)
            END_INTERFACE_MAP

            //  IPlugin methods
            // -------------------------------------------------------------------------------------------------------
            const string Initialize(PluginHost::IShell* service) override;
            void Deinitialize(PluginHost::IShell* service) override;
            string Information() const override;

        private:
            void Deactivated(RPC::IRemoteConnection* connection);

        private:
            PluginHost::IShell* _service{};
            uint32_t _connectionId{};
            Exchange::IUSBDevice* _usbDeviceImpl{};
            Core::Sink<Notification> _usbDeviceNotification;
    };

} // namespace Plugin
} // namespace WPEFramework
