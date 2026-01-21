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

#include <stdlib.h>
#include <errno.h>
#include <string>
#include <iomanip>

#include <sys/prctl.h>
#include <mutex>
#include <thread>
#include <fstream>

#include "USBMassStorageImplementation.h"
#include "UtilsLogging.h"

#define MEDIA_PATH             "/tmp/media"
#define MOUNT_PATH             "/tmp/media/usb"
#define FILE_SYSTEM_VFAT       "vfat"
#define FILE_SYSTEM_EXFAT      "exfat"

using namespace std;

namespace WPEFramework {
namespace Plugin {

    SERVICE_REGISTRATION(USBMassStorageImplementation, 1, 0);
    USBMassStorageImplementation* USBMassStorageImplementation::_instance = nullptr;
    USBMassStorageImplementation::USBMassStorageImplementation()
    : _adminLock()
    , _remoteUSBDeviceObject(nullptr)
    , _USB_DeviceNotification(*this)
    , _registeredEventHandlers(false)
    , _service(nullptr)
    {
        LOGINFO("Create USBMassStorageImplementation Instance");
        USBMassStorageImplementation::_instance = this;
    }

    uint32_t USBMassStorageImplementation::Configure(PluginHost::IShell* service)
    {
        uint32_t result = Core::ERROR_GENERAL;
        if (service != nullptr)
        {
            _service = service;
            _service->AddRef();
            result = Core::ERROR_NONE;
            _remoteUSBDeviceObject = _service->QueryInterfaceByCallsign<WPEFramework::Exchange::IUSBDevice>("org.rdk.UsbDevice");
            if (_remoteUSBDeviceObject != nullptr)
            {
                registerEventHandlers();

                /* Getting USB Device list and mounting available USB devices */
                MountDevicesOnBootUp();
            }
            else
            {
                LOGERR("_remoteUSBDeviceObject is null \n");
            }
        }
        else
        {
            LOGERR("service is null \n");
        }

        return result;
    }

    USBMassStorageImplementation::~USBMassStorageImplementation()
    {
        if(_remoteUSBDeviceObject)
        {
            _remoteUSBDeviceObject->Release();
            _remoteUSBDeviceObject = nullptr;
        }
        _registeredEventHandlers = false;

        if (_service != nullptr)
        {
           _service->Release();
           _service = nullptr;
        }
    }

    void USBMassStorageImplementation::registerEventHandlers()
    {
        if(!_registeredEventHandlers && _remoteUSBDeviceObject)
        {
            _registeredEventHandlers = true;
            _remoteUSBDeviceObject->Register(&_USB_DeviceNotification);
        }
        else
        {
            LOGERR("_remoteUSBDeviceObject is null \n");
        }
    }

    Core::hresult USBMassStorageImplementation::Register(Exchange::IUSBMassStorage::INotification *notification)
    {
        _adminLock.Lock();
        if (nullptr != notification)
        {
            // Make sure we can't register the same notification callback multiple times
            if (std::find(_usbStorageNotification.begin(), _usbStorageNotification.end(), notification) == _usbStorageNotification.end())
            {
                _usbStorageNotification.push_back(notification);
                notification->AddRef();
            }
            else
            {
                LOGERR("same notification is registered already");
            }
        }
        _adminLock.Unlock();

        return Core::ERROR_NONE;
    }

    Core::hresult USBMassStorageImplementation::Unregister(Exchange::IUSBMassStorage::INotification *notification )
    {
        uint32_t status = Core::ERROR_GENERAL;

        _adminLock.Lock();
        if (nullptr != notification)
        {
            // we just unregister one notification once
            auto itr = std::find(_usbStorageNotification.begin(), _usbStorageNotification.end(), notification);
            if (itr != _usbStorageNotification.end())
            {
                (*itr)->Release();
                _usbStorageNotification.erase(itr);
                status = Core::ERROR_NONE;
            }
            else
            {
                LOGERR("notification not found");
            }
        }
        _adminLock.Unlock();

        return status;
    }

    void USBMassStorageImplementation::Dispatch(Event event, USBStorageDeviceInfo storageDeviceInfo)
    {
        _adminLock.Lock();

        switch(event)
        {
            case USB_STORAGE_EVENT_MOUNT:
                DispatchMountEvent(storageDeviceInfo);
                break;
            case USB_STORAGE_EVENT_UNMOUNT:
                DispatchUnMountEvent(storageDeviceInfo);
                break;
            default:
                LOGERR("Unexpected event is received %u",event);
                break;
        }

        _adminLock.Unlock();
    }

    bool USBMassStorageImplementation::DeviceMount(const USBStorageDeviceInfo &storageDeviceInfo)
    {
        ifstream partitionsFile("/proc/partitions");
        std::vector<std::string> partitions;
        USBStorageMountFlags mountFlags = USBStorageMountFlags::READ_WRITE;
        string line;
        bool success = false;
        size_t num_partitions = 0;
        string prefix = "/dev/";
        string partitionPath;

        size_t pos = storageDeviceInfo.devicePath.find(prefix);
        if (pos != std::string::npos)
        {
            partitionPath = storageDeviceInfo.devicePath.substr(pos + prefix.length());
        }
        else
        {
            partitionPath = storageDeviceInfo.devicePath;
        }
        LOGINFO("partitionPath [%s]", partitionPath.c_str());

        while (getline(partitionsFile, line))
        {
            if (line.find(partitionPath) != std::string::npos && line != partitionPath)
            {
                string partition = "/dev/" + line.substr(line.find_last_of(' ') + 1); 
                LOGINFO("Device path [%s], partition [%s]", storageDeviceInfo.devicePath.c_str(),partition.c_str());
                partitions.push_back(partition);
            }
        }
        num_partitions = partitions.size();
        LOGINFO("Device path[%s] Device Name[%s] num_partitions [%zd]",storageDeviceInfo.devicePath.c_str(),storageDeviceInfo.deviceName.c_str(),num_partitions-1);

        if (!directoryExists(MEDIA_PATH))
        {
            if (mkdir(MEDIA_PATH, 0755) == -1)
            {
                LOGERR("Failed to create /tmp/media : %s",strerror(errno));
            }
        }

        for (size_t i = 0; i < num_partitions; ++i)
        {
            string partition = partitions[i];
            string mountPoint;
            Exchange::IUSBMassStorage::USBStorageMountInfo mountInfo = {};
            int index = 1;

            /* Skipping the mount path creation for partitions[0], as this is always device path(/dev/sdx). If there
            is only one partition then we are creating the mount path on the device path (partitions[0]). */
            if((num_partitions > 1) && (i == 0))
            continue;

            while (directoryExists(MOUNT_PATH + std::to_string(index)))
            {
                ++index;
            }

            mountPoint = MOUNT_PATH + std::to_string(index);
            LOGINFO("MountPoint [%s]", mountPoint.c_str());
            if (mkdir(mountPoint.c_str(), 0755) == 0)
            {
                if ((mount(partition.c_str(), mountPoint.c_str(), FILE_SYSTEM_VFAT, 0, nullptr)) == 0)
                {
                    mountInfo.fileSystem = VFAT;
                    LOGINFO("filetype is vfat");
                }
                else if ((mount(partition.c_str(), mountPoint.c_str(), FILE_SYSTEM_EXFAT, 0, nullptr)) == 0)
                {
                    LOGINFO("filetype is exfat");
                    mountInfo.fileSystem = EXFAT;
                }
                else
                {
                    LOGERR("Mount is failed for [%s]. Error[%s]", partition.c_str(), strerror(errno));
                    if (rmdir(mountPoint.c_str()) != 0)
                    {
                        LOGERR("Directory[%s] removal is failed.", mountPoint.c_str());
                    }
                    continue;
                }
                LOGINFO("[%s] is mounted successfully.",partition.c_str());
                mountInfo.partitionName = partition;
                mountInfo.mountFlags = mountFlags;
                mountInfo.mountPath = mountPoint;

                usbStorageMountInfo[storageDeviceInfo.deviceName].push_back(mountInfo);
                success = true;
            }
            else
            {
                LOGERR("Failed to create folder[%s] to mount device. Error[%s]", mountPoint.c_str(), strerror(errno));
            }
        }
        return success;
    }

    void USBMassStorageImplementation::DispatchMountEvent(const USBStorageDeviceInfo &storageDeviceInfo)
    {
        Exchange::IUSBMassStorage::IUSBStorageMountInfoIterator* mountPoints = nullptr;

        if ( true == DeviceMount(storageDeviceInfo))
        {
            if (usbStorageMountInfo[storageDeviceInfo.deviceName].empty())
            {
                LOGERR("Empty Mount info list");
            }
            else
            {
                usbStorageDeviceInfo.push_back(storageDeviceInfo);
                /* mount info list updated, and Iterator create to send to clients */
                mountPoints = (Core::Service<RPC::IteratorType<Exchange::IUSBMassStorage::IUSBStorageMountInfoIterator>> \
                                ::Create<Exchange::IUSBMassStorage::IUSBStorageMountInfoIterator>(usbStorageMountInfo[storageDeviceInfo.deviceName]));
                LOGINFO("Device mounted");
            }

            if (mountPoints != nullptr)
            {
                std::list<Exchange::IUSBMassStorage::INotification*>::const_iterator index(_usbStorageNotification.begin());
                while (index != _usbStorageNotification.end())
                {
                    mountPoints->Reset(0);
                    (*index)->OnDeviceMounted(storageDeviceInfo, mountPoints);
                    ++index;
                }
                mountPoints->Release();
            }
        }
        else
        {
            LOGERR("Failed to mount device %s",storageDeviceInfo.deviceName.c_str());
        }
    }

    void USBMassStorageImplementation::DispatchUnMountEvent(USBStorageDeviceInfo &storageDeviceInfo)
    {
        Exchange::IUSBMassStorage::IUSBStorageMountInfoIterator* mountPoints = nullptr;
        auto mountInfoList = usbStorageMountInfo.find(storageDeviceInfo.deviceName);
        int returnValue;
        bool success = false;

        if (mountInfoList == usbStorageMountInfo.end())
        {
            string deviceName = storageDeviceInfo.deviceName;

            auto it = std::find_if(usbStorageDeviceInfo.begin(), usbStorageDeviceInfo.end(), [deviceName](const USBStorageDeviceInfo& item){
                        return item.deviceName == deviceName;
            });
            if (it != usbStorageDeviceInfo.end())
            {
                usbStorageDeviceInfo.erase(it);
            }

            LOGINFO("StorageInfo removed, but Mount Info is empty");
        }
        else
        {
            for (const auto& mountInfo : mountInfoList->second)
            {
                if ((returnValue = umount(mountInfo.mountPath.c_str())) == 0)
                {
                    LOGINFO("Unmount successful");
                    if (rmdir(mountInfo.mountPath.c_str()) != 0)
                    {
                        LOGERR("Failed to remove mount path: %s",mountInfo.mountPath.c_str());
                    }
                    success = true;
                }
                else
                {
                    LOGERR("Failed to unmount: %s, error %d",mountInfo.mountPath.c_str(),returnValue);
                }
            }
        }

        if ( success )
        {
            mountPoints = Core::Service<RPC::IteratorType<Exchange::IUSBMassStorage::IUSBStorageMountInfoIterator>> \
				::Create<Exchange::IUSBMassStorage::IUSBStorageMountInfoIterator>(mountInfoList->second);

            if (mountPoints != nullptr)
            {
                std::list<Exchange::IUSBMassStorage::INotification*>::const_iterator index(_usbStorageNotification.begin());
                while (index != _usbStorageNotification.end())
                {
                    mountPoints->Reset(0);
                    (*index)->OnDeviceUnmounted(storageDeviceInfo, mountPoints);
                    ++index;
                }
                mountPoints->Release();
            }
            usbStorageMountInfo.erase(mountInfoList);

            string deviceName = storageDeviceInfo.deviceName;

            auto it = std::find_if(usbStorageDeviceInfo.begin(), usbStorageDeviceInfo.end(), [deviceName](const USBStorageDeviceInfo& item){
                        return item.deviceName == deviceName;
            });
            if (it != usbStorageDeviceInfo.end())
            {
                usbStorageDeviceInfo.erase(it);
            }
        }

    }

    void USBMassStorageImplementation::MountDevicesOnBootUp()
    {
        Exchange::IUSBDevice::IUSBDeviceIterator* devices = nullptr;
        Exchange::IUSBDevice::USBDevice actual_usbDevice_dev_list = {0};

        if (nullptr == _remoteUSBDeviceObject)
        {
            LOGERR("USBDeviceObject is null");
        }
        else if ( Core::ERROR_NONE != _remoteUSBDeviceObject->GetDeviceList(devices))
        {
            LOGERR("Device list is empty or GetDeviceList() got failed");
        }
        else if (nullptr != devices)
        {
            while (devices->Next(actual_usbDevice_dev_list) == true)
            {
                if (LIBUSB_CLASS_MASS_STORAGE == actual_usbDevice_dev_list.deviceClass)
                {
                    USBStorageDeviceInfo storageDeviceInfo = {};
                    if (actual_usbDevice_dev_list.devicePath.empty())
                    {
                        LOGERR("device path is empty");
                        continue;
                    }
                    storageDeviceInfo.deviceName = actual_usbDevice_dev_list.deviceName;
                    storageDeviceInfo.devicePath = actual_usbDevice_dev_list.devicePath;
                    LOGINFO("Device path[%s] Name[%s]",storageDeviceInfo.devicePath.c_str(),storageDeviceInfo.deviceName.c_str());

                    /* storage info list updated */
                    usbStorageDeviceInfo.push_back(storageDeviceInfo);
                    if (true == DeviceMount(storageDeviceInfo))
                    {
                        LOGINFO("Device is mounted Successfully");
                    }
                }
                else
                {
                    LOGWARN("[%s] is not a storage device",actual_usbDevice_dev_list.devicePath.c_str());
                }
            }
        }
    }

    Core::hresult USBMassStorageImplementation::GetDeviceList(Exchange::IUSBMassStorage::IUSBStorageDeviceInfoIterator*& deviceInfo) const
    {
        uint32_t errorCode = Core::ERROR_GENERAL;
        bool emptyList = false;

        if (usbStorageDeviceInfo.empty())
        {
            Exchange::IUSBDevice::IUSBDeviceIterator* devices = nullptr;
            Exchange::IUSBDevice::USBDevice actual_usbDevice_dev_list = {0};
            emptyList = true;

            LOGINFO("usbStorageDeviceInfo list is Empty, Getting from USBDevice Plugin");

            if (nullptr == _remoteUSBDeviceObject)
            {
                LOGERR("USBDeviceObject is null");
            }
            else if ( Core::ERROR_NONE != _remoteUSBDeviceObject->GetDeviceList(devices))
            {
                LOGERR("Device list is Empty or get failed");
            }
            else if (nullptr != devices)
            {
                while (devices->Next(actual_usbDevice_dev_list) == true)
                {
                    if (LIBUSB_CLASS_MASS_STORAGE == actual_usbDevice_dev_list.deviceClass)
                    {
                        USBStorageDeviceInfo storageDeviceInfo = {};

                        if (actual_usbDevice_dev_list.devicePath.empty())
                        {
                            LOGERR("device path is empty");
							continue;
                        }

                        if ( nullptr == USBMassStorageImplementation::_instance)
                        {
                            LOGERR("instance is null");
                        }
                        else
                        {
                            storageDeviceInfo.deviceName = actual_usbDevice_dev_list.deviceName;
                            storageDeviceInfo.devicePath = actual_usbDevice_dev_list.devicePath;
                            LOGINFO("Device path[%s] Name[%s]",storageDeviceInfo.devicePath.c_str(),storageDeviceInfo.deviceName.c_str());

                            /* storage info list updated */
                            USBMassStorageImplementation::_instance->usbStorageDeviceInfo.push_back(storageDeviceInfo);
                            emptyList = false;
                        }
                    }
                    else
                    {
                        LOGWARN("%s is not a storage device",actual_usbDevice_dev_list.devicePath.c_str());
                    }
                }

                errorCode = Core::ERROR_NONE;
                devices->Release();
            }
        }

        if ( false == emptyList )
        {
            deviceInfo = Core::Service<RPC::IteratorType<Exchange::IUSBMassStorage::IUSBStorageDeviceInfoIterator>> \
				::Create<Exchange::IUSBMassStorage::IUSBStorageDeviceInfoIterator>(usbStorageDeviceInfo);
            errorCode = Core::ERROR_NONE;
        }
        return errorCode;
    }

    Core::hresult USBMassStorageImplementation::GetMountPoints(const string &deviceName, Exchange::IUSBMassStorage::IUSBStorageMountInfoIterator*& mountPoints) const
    {
        uint32_t errorCode = Core::ERROR_GENERAL;
        if (deviceName.empty())
        {
            errorCode = Core::ERROR_INVALID_PARAMETER;
        }
        else
        {
            auto it = USBMassStorageImplementation::_instance->usbStorageMountInfo.find(deviceName);
            if (it == USBMassStorageImplementation::_instance->usbStorageMountInfo.end())
            {
                auto itr = std::find_if(usbStorageDeviceInfo.begin(), usbStorageDeviceInfo.end(), [deviceName](const USBStorageDeviceInfo& item){
                            return item.deviceName == deviceName;
                });

                if (itr != usbStorageDeviceInfo.end())
                {
                    USBStorageDeviceInfo storageDeviceInfo = *itr;
                    if ( nullptr == USBMassStorageImplementation::_instance )
                    {
                        LOGERR("instance is null");
                    }
                    else if ( true == USBMassStorageImplementation::_instance->DeviceMount(storageDeviceInfo))
                    {
                        if (USBMassStorageImplementation::_instance->usbStorageMountInfo[storageDeviceInfo.deviceName].empty())
                        {
                            LOGERR("Empty Mount info list");
                        }
                        else
                        {
                            /* mount info list updated, and Iterator create to send to clients */
                            mountPoints = (Core::Service<RPC::IteratorType<Exchange::IUSBMassStorage::IUSBStorageMountInfoIterator>> \
                                            ::Create<Exchange::IUSBMassStorage::IUSBStorageMountInfoIterator>(USBMassStorageImplementation::_instance->usbStorageMountInfo[storageDeviceInfo.deviceName]));

                            LOGINFO("Device mount info list");
                        }
                        errorCode = Core::ERROR_NONE;
                    }
                }
                else
                {
                    LOGERR("Mount info not found for device %s",deviceName.c_str());
                    errorCode = ERROR_INVALID_DEVICENAME;
                }
            }
            else
            {
                mountPoints = Core::Service<RPC::IteratorType<Exchange::IUSBMassStorage::IUSBStorageMountInfoIterator>> \
                                ::Create<Exchange::IUSBMassStorage::IUSBStorageMountInfoIterator>(it->second);
                errorCode = Core::ERROR_NONE;
            }
        }
        return errorCode;
    }

    Core::hresult USBMassStorageImplementation::GetPartitionInfo(const string &mountPath , Exchange::IUSBMassStorage::USBStoragePartitionInfo& partitionInfo ) const
    {
        string devicePath = "";
        uint32_t errorCode = Core::ERROR_GENERAL;
        int fd = -1;
        struct statvfs stat;
        struct statfs buffer;

        if (mountPath.empty())
        {
            errorCode = Core::ERROR_INVALID_PARAMETER;
        }
        else
        {
            for (const auto& pair : USBMassStorageImplementation::_instance->usbStorageMountInfo)
            {
                const auto& mountInfoList = pair.second;

                for (const auto& mountInfo : mountInfoList)
                {
                    if (mountInfo.mountPath == mountPath)
                    {
                        devicePath = mountInfo.partitionName;
                        partitionInfo.fileSystem = mountInfo.fileSystem;
                        break;
                    }
                }
            }

            if (devicePath.empty())
            {
                errorCode = ERROR_INVALID_MOUNTPOINT;
                LOGERR("device path is not found in list");
            }
            else
            {
                LOGINFO("Partition path: %s, Mount path: %s",devicePath.c_str(),mountPath.c_str());

                if (statfs(mountPath.c_str(), &buffer) != 0)
                {
                    LOGERR("statfs failed, error %s",strerror(errno));
                }
                else if (statvfs(mountPath.c_str(), &stat) != 0)
                {
                    LOGERR("statfs failed, error %s",strerror(errno));
                }
                else if ((fd = open(devicePath.c_str(), O_RDONLY)) == -1)
                {
                    LOGERR( "Failed to open device:%s" , mountPath.c_str());
                }
                else
                {
                    uint64_t size = 0;

                    partitionInfo.startSector = 0;

                    if (ioctl(fd, BLKGETSIZE64, &size) != 0)
                    {
                        LOGERR("Failed to get partition size, error %s",strerror(errno));
                    }
                    else if (ioctl(fd, BLKGETSIZE, &partitionInfo.numSectors) != 0) {
                         LOGERR("Failed to get number of sector, error %s",strerror(errno));
                    }
                    else if (ioctl(fd, BLKSSZGET, &partitionInfo.sectorSize) != 0) {
                        LOGERR("Failed to get sector size, error %s",strerror(errno));
                    }
                    else
                    {
                        partitionInfo.totalSpace = ((uint64_t)stat.f_blocks * stat.f_frsize) / ((uint64_t)1024 * 1024); // MB
                        partitionInfo.availableSpace = ((uint64_t)stat.f_bfree * stat.f_frsize) / ((uint64_t)1024 * 1024); // MB
                        partitionInfo.usedSpace = partitionInfo.totalSpace - partitionInfo.availableSpace;
                        size /= (1024 * 1024); // Convert to MB
                        partitionInfo.size = size;

                        LOGINFO("f_blocks %lu. f_frsize %lu",(long)stat.f_blocks, (long)stat.f_frsize);

                        LOGINFO("PartitionInfo, fileSystem[%u],partition size[%lu MB] numSectors[%lu] sectorSize[%u]totalSpace[%u]usedSpace[%u]availableSpace[%u]",
                                        partitionInfo.fileSystem,(long)size, (long)partitionInfo.numSectors, partitionInfo.sectorSize,partitionInfo.totalSpace,partitionInfo.usedSpace,partitionInfo.availableSpace);

                        errorCode = Core::ERROR_NONE;
                    }
                    close(fd);
                }
            }
        }
        return errorCode;
    }

    void USBMassStorageImplementation::OnDevicePluggedIn(const USBDevice &device)
    {
        if ( LIBUSB_CLASS_MASS_STORAGE == device.deviceClass )
        {
            USBStorageDeviceInfo storageDeviceInfo = {};
            bool alreadyMounted = true;

            if (device.devicePath.empty())
            {
                LOGERR("device path is empty");
            }
            else
            {
                auto mountInfoList = USBMassStorageImplementation::_instance->usbStorageMountInfo.find(storageDeviceInfo.deviceName);

                if (mountInfoList != USBMassStorageImplementation::_instance->usbStorageMountInfo.end())
                {
                    if (mountInfoList->second.empty())
                    {
                        alreadyMounted = false;
                    }
                }
                else
                {
                    alreadyMounted = false;
                }
            }

            if ( false == alreadyMounted )
            {
                if ( nullptr == USBMassStorageImplementation::_instance)
                {
                    LOGERR("instance is null");
                }
                else
                {
                    storageDeviceInfo.devicePath = device.devicePath;
                    storageDeviceInfo.deviceName = device.deviceName;
                    LOGINFO("Device path[%s] Name[%s]",storageDeviceInfo.devicePath.c_str(),storageDeviceInfo.deviceName.c_str());
                    Core::IWorkerPool::Instance().Submit(USBMassStorageImplementation::Job::Create(USBMassStorageImplementation::_instance,
                                    USBMassStorageImplementation::USB_STORAGE_EVENT_MOUNT,
                                    storageDeviceInfo));
                }
            }
            else
            {
                LOGERR("Device is already mounted");
            }
        }
        else
        {
            LOGWARN("%s is not a storage device",device.devicePath.c_str());
        }
    }

    void USBMassStorageImplementation::OnDevicePluggedOut(const USBDevice &device)
    {
        if ( LIBUSB_CLASS_MASS_STORAGE == device.deviceClass )
        {
            USBStorageDeviceInfo storageDeviceInfo = {};

            storageDeviceInfo.devicePath = device.devicePath;
            storageDeviceInfo.deviceName = device.deviceName;

            LOGINFO("Device path[%s] Name[%s]",storageDeviceInfo.devicePath.c_str(),storageDeviceInfo.deviceName.c_str());

            Core::IWorkerPool::Instance().Submit(USBMassStorageImplementation::Job::Create(USBMassStorageImplementation::_instance,
                        USBMassStorageImplementation::USB_STORAGE_EVENT_UNMOUNT,
                        storageDeviceInfo));
        }
        else
        {
            LOGWARN("%s is not a storage device",device.devicePath.c_str());
        }
    }

    bool USBMassStorageImplementation::directoryExists(const string& path)
    {
        struct stat info;
        int ret = stat(path.c_str(), &info);
        return ((ret == 0 ) && (info.st_mode & S_IFDIR));
    }

    }
}

