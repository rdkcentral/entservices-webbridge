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
#include <interfaces/json/JsonData_USBMassStorage.h>
#include <interfaces/json/JUSBMassStorage.h>
#include <interfaces/IUSBMassStorage.h>
#include <interfaces/IConfiguration.h>
#include <core/JSON.h>

#include "tracing/Logging.h"
#include <mutex>

namespace WPEFramework {
namespace Plugin {

    class USBMassStorage: public PluginHost::IPlugin, public PluginHost::JSONRPCErrorAssessor<PluginHost::JSONRPCErrorAssessorTypes::FunctionCallbackType> 
    {
     private:
        class Notification : public RPC::IRemoteConnection::INotification,
                             public Exchange::IUSBMassStorage::INotification
        {
            private:
                Notification() = delete;
                Notification(const Notification&) = delete;
                Notification& operator=(const Notification&) = delete;

            public:
            explicit Notification(USBMassStorage* parent)
                : _parent(*parent)
                {
                    ASSERT(parent != nullptr);
                }

                virtual ~Notification()
                {
                }

                BEGIN_INTERFACE_MAP(Notification)
                INTERFACE_ENTRY(Exchange::IUSBMassStorage::INotification)
                INTERFACE_ENTRY(RPC::IRemoteConnection::INotification)
                END_INTERFACE_MAP

                void Activated(RPC::IRemoteConnection*) override;
                void Deactivated(RPC::IRemoteConnection *connection) override;
                void OnDeviceMounted(const Exchange::IUSBMassStorage::USBStorageDeviceInfo &deviceInfo, Exchange::IUSBMassStorage::IUSBStorageMountInfoIterator* const mountPoints) override;
                void OnDeviceUnmounted(const Exchange::IUSBMassStorage::USBStorageDeviceInfo &deviceInfo, Exchange::IUSBMassStorage::IUSBStorageMountInfoIterator* const mountPoints) override;

            private:
                USBMassStorage& _parent;
        };

        public:
            // We do not allow this plugin to be copied !!
            USBMassStorage(const USBMassStorage&) = delete;
            USBMassStorage& operator=(const USBMassStorage&) = delete;

            USBMassStorage();
            virtual ~USBMassStorage();

            BEGIN_INTERFACE_MAP(USBMassStorage)
            INTERFACE_ENTRY(PluginHost::IPlugin)
            INTERFACE_ENTRY(PluginHost::IDispatcher)
            INTERFACE_AGGREGATE(Exchange::IUSBMassStorage, _usbMassStorage)
            END_INTERFACE_MAP

            //  IPlugin methods
            // -------------------------------------------------------------------------------------------------------
            const string Initialize(PluginHost::IShell* service) override;
            void Deinitialize(PluginHost::IShell* service) override;
            string Information() const override;

            static uint32_t OnJSONRPCError(const Core::JSONRPC::Context& context, const string& method, const string& parameters, const uint32_t errorcode, string& errormessage);

        private:
            void Deactivated(RPC::IRemoteConnection* connection);

        private:
            PluginHost::IShell* _service{};
            uint32_t _connectionId{};
            Exchange::IUSBMassStorage* _usbMassStorage{};
            Core::Sink<Notification> _usbStoragesNotification;
            Exchange::IConfiguration* configure{nullptr};
    };

} // namespace Plugin
} // namespace WPEFramework
