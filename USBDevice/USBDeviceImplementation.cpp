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

#include "USBDeviceImplementation.h"
#include <sys/prctl.h>
#include "UtilsJsonRpc.h"
#include <mutex>
#include "tracing/Logging.h"
#include <boost/filesystem.hpp>
#include <boost/format.hpp>

#if defined(RDK_SERVICE_L2_TEST) || defined(RDK_SERVICES_L1_TEST)
#define PLUGIN_USBDEVICE_PATH   "/tmp/bus/usb/devices"
#define PLUGIN_USBDEVICE_BLOCKPATH    "/tmp/block"
#define PLUGIN_USBDEVICE_BUS_NUM "busnum"
#define PLUGIN_USBDEVICE_DEV_ADDR "devnum"
#else
#define PLUGIN_USBDEVICE_PATH   "/sys/bus/usb/devices"
#define PLUGIN_USBDEVICE_BLOCKPATH    "/sys/block"
#define PLUGIN_USBDEVICE_BUS_NUM "busnum"
#define PLUGIN_USBDEVICE_DEV_ADDR "devnum"
#endif /* defined(RDK_SERVICE_L2_TEST) || defined(RDK_SERVICES_L1_TEST) */

#define PLUGIN_USBDEVICE_DEV_DISK_PATH "/dev/disk/by-id/"
#define PLUGIN_USBDEVICE_VENDOR_PATH "device/vendor"
#define PLUGIN_USBDEVICE_MODEL_PATH "device/model"

/**
*   @brief length of the usb string descriptor header
*/
#define USB_STRING_DESC_HEADER_LENGTH 2
namespace WPEFramework {
namespace Plugin {

SERVICE_REGISTRATION(USBDeviceImplementation, 1, 0);

USBDeviceImplementation::USBDeviceImplementation()
: _adminLock()
, _libUSBDeviceThread(nullptr)
,_handlingUSBDeviceEvents(false)
{
    LOGINFO("Create USBDeviceImplementation Instance");
    USBDeviceImplementation::instance(this);

    if (Core::ERROR_NONE != libUSBInit())
    {
        LOGERR("libUSBInit Failed");
    }
}

USBDeviceImplementation* USBDeviceImplementation::instance(USBDeviceImplementation *USBDeviceImpl)
{
   static USBDeviceImplementation *USBDeviceImpl_instance = nullptr;

   if (USBDeviceImpl != nullptr)
   {
      USBDeviceImpl_instance = USBDeviceImpl;
   }

   return(USBDeviceImpl_instance);
}

USBDeviceImplementation::~USBDeviceImplementation()
{
    libUSBClose();
}

uint32_t USBDeviceImplementation::libUSBInit(void)
{
    uint32_t status = Core::ERROR_GENERAL;
    int retValue = LIBUSB_SUCCESS;
    LOGINFO("Entry");

    retValue = libusb_init_context(/*ctx=*/NULL, /*options=*/NULL, /*num_options=*/0);

    if (LIBUSB_SUCCESS != retValue)
    {
        LOGERR ("failed to initialise libusb: %s\n", libusb_strerror((enum libusb_error)retValue));
    }
    else if(!libusb_has_capability (LIBUSB_CAP_HAS_HOTPLUG))
    {
        LOGERR ("Hotplug capabilities are not supported on this platform\n");
        libusb_exit (NULL);
    }
    else if (LIBUSB_SUCCESS !=  (retValue = libusb_hotplug_register_callback(NULL,
                                                                             LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED, 
                                                                             0, 
                                                                             LIBUSB_HOTPLUG_MATCH_ANY, 
                                                                             LIBUSB_HOTPLUG_MATCH_ANY, 
                                                                             LIBUSB_HOTPLUG_MATCH_ANY, 
                                                                             USBDeviceImplementation::libUSBHotPlugCallbackDeviceAttached, 
                                                                             NULL, 
                                                                             &_hotPlugHandle[0])))
    {
        LOGERR ("Error registering libUSBHotPlugCallbackDeviceAttached: %s\n", libusb_strerror((enum libusb_error)retValue));
        libusb_exit (NULL);
    }
    else if (LIBUSB_SUCCESS !=  (retValue = libusb_hotplug_register_callback(NULL,
                                                                             LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT,
                                                                             0,
                                                                             LIBUSB_HOTPLUG_MATCH_ANY,
                                                                             LIBUSB_HOTPLUG_MATCH_ANY,
                                                                             LIBUSB_HOTPLUG_MATCH_ANY,
                                                                             USBDeviceImplementation::libUSBHotPlugCallbackDeviceDetached,
                                                                             NULL,
                                                                             &_hotPlugHandle[1])))
    {
        LOGERR ("Error registering libUSBHotPlugCallbackDeviceDetached: %s\n", libusb_strerror((enum libusb_error)retValue));
        libusb_exit (NULL);
    }
    else if (nullptr == _libUSBDeviceThread)
    {
        _handlingUSBDeviceEvents = true;

        _libUSBDeviceThread = new std::thread(&USBDeviceImplementation::libUSBEventsHandlingThread, USBDeviceImplementation::instance());

        if (nullptr != _libUSBDeviceThread)
        {
            LOGINFO("libUSBInit Successed");
            status = Core::ERROR_NONE;
        }
        else
        {
            LOGERR("libUSBEventsHandlingThread Failed");
            _handlingUSBDeviceEvents = false;
        }
    }
    else
    {
        LOGINFO("libUSBInit Successed");
        status = Core::ERROR_NONE;
    }

    return status;
}

void USBDeviceImplementation::libUSBClose(void)
{
    _handlingUSBDeviceEvents = false;

    (void)libusb_hotplug_deregister_callback(NULL, _hotPlugHandle[0]);
    (void)libusb_hotplug_deregister_callback(NULL, _hotPlugHandle[1]);

    if (_libUSBDeviceThread)
    {
        if (_libUSBDeviceThread->joinable())
        {
            (void)_libUSBDeviceThread->join();
        }
        delete _libUSBDeviceThread;
        _libUSBDeviceThread = nullptr;
    }

    (void)libusb_exit(NULL);
    LOGINFO("libUSBClose Successed");
}

void USBDeviceImplementation::libUSBEventsHandlingThread(void)
{
    int retValue = LIBUSB_SUCCESS;

    while (_handlingUSBDeviceEvents)
    {
        retValue = libusb_handle_events (NULL);
    
        if (LIBUSB_SUCCESS != retValue)
        {
            LOGERR ("libusb_handle_events() failed: %s\n", libusb_strerror((enum libusb_error)retValue));
        }
    }
}

int USBDeviceImplementation::libUSBHotPlugCallbackDeviceAttached(libusb_context *ctx, libusb_device *dev, libusb_hotplug_event event, void *user_data)
{
      Exchange::IUSBDevice::USBDevice usbDevice = {0};

      (void)ctx;
      (void)event;
      (void)user_data;

      ASSERT (nullptr != dev);

      if (nullptr != dev)
      {
          if (Core::ERROR_NONE == USBDeviceImplementation::instance()->getUSBDeviceStructFromDeviceDescriptor(dev, &usbDevice))
          {
              JsonObject params, device;

              device["deviceclass"] = usbDevice.deviceClass;
              device["devicesubclass"] = usbDevice.deviceSubclass;
              device["devicename"] = usbDevice.deviceName;
              device["devicepath"] = usbDevice.devicePath;

              params["device"] = device;

              LOGINFO ("usbDevice.deviceClass: %u usbDevice.deviceSubclass:%u usbDevice.deviceName:%s devicePath:%s", 
			  	                       usbDevice.deviceClass,
			  	                       usbDevice.deviceSubclass,
			  	                       usbDevice.deviceName.c_str(),
			  	                       usbDevice.devicePath.c_str());

              USBDeviceImplementation::instance()->dispatchEvent(USBDeviceImplementation::Event::USBDEVICE_HOTPLUG_EVENT_DEVICE_ARRIVED, usbDevice);
          }
          else
          {
              LOGERR ("getUSBDeviceStructFromDeviceDescriptor Failed");
          }
      }
      else
      {
          LOGERR ("libusb_device is null");
      }
      
      return 0;
}

int USBDeviceImplementation::libUSBHotPlugCallbackDeviceDetached(libusb_context *ctx, libusb_device *dev, libusb_hotplug_event event, void *user_data)
{
      Exchange::IUSBDevice::USBDevice  usbDevice = {0};

      (void)ctx;
      (void)event;
      (void)user_data;

      ASSERT (nullptr != dev);

      if (nullptr != dev)
      {
          if (Core::ERROR_NONE == USBDeviceImplementation::instance()->getUSBDeviceStructFromDeviceDescriptor(dev, &usbDevice))
          {
              LOGINFO ("usbDevice.deviceClass: %u usbDevice.deviceSubclass:%u usbDevice.deviceName:%s devicePath:%s", 
			  	                       usbDevice.deviceClass,
			  	                       usbDevice.deviceSubclass,
			  	                       usbDevice.deviceName.c_str(),
			  	                       usbDevice.devicePath.c_str());

              USBDeviceImplementation::instance()->dispatchEvent(USBDeviceImplementation::Event::USBDEVICE_HOTPLUG_EVENT_DEVICE_LEFT, usbDevice);
          }
          else
          {
              LOGERR ("getUSBDeviceStructFromDeviceDescriptor Failed");
          }
      }
      else
      {
          LOGERR ("libusb_device is null");
      }
      
      return 0;
}

void USBDeviceImplementation::getDeviceSerialNumber(libusb_device *pDev, string &serialNumber)
{
    uint8_t devPath[8] = {0};
    char path[256] = {0};
    string sysfsPath = "";
    int numbOfElements = 0;
    uint8_t busNumber = 0;
    int indexPath = 0;

    // Get bus number from libusb device
    busNumber = libusb_get_bus_number(pDev);

    // Construct sysfs path
    sysfsPath = std::to_string(busNumber) + "-";

    numbOfElements = libusb_get_port_numbers(pDev, devPath, sizeof(devPath));

    if (numbOfElements > 0)
    {
        sysfsPath += std::to_string(devPath[0]);

        for (indexPath = 1; indexPath < numbOfElements; indexPath++)
        {
            sysfsPath += "." + std::to_string(devPath[indexPath]);
        }
    }

    std::snprintf(path, sizeof(path), "%s/%s/serial", PLUGIN_USBDEVICE_PATH, sysfsPath.c_str());

    string filePath = path;

    if (!filePath.empty())
    {
        // Open the file using std::ifstream
        std::ifstream file(filePath);

        if (!file.is_open())
        {
            LOGERR("Error opening file: %s", filePath.c_str());
        }
        else
        {
            // Read the file content into a string
            std::getline(file, serialNumber);
            if (serialNumber.empty())
            {
                LOGERR("Error opening file: %s", filePath.c_str());
            }
            // Close the file
            file.close();
        }
    }
}

void USBDeviceImplementation::getDevicePathFromDevice(libusb_device *pDev, string &devPath, string& deviceSerialNumber)
{
    const string dirPath = string(PLUGIN_USBDEVICE_BLOCKPATH);
    DIR* dir = nullptr;
    bool pathFound = false;

    // Get the device serial number
    getDeviceSerialNumber(pDev, deviceSerialNumber);

    if (deviceSerialNumber.empty())
    {
        LOGERR(" Failed read Serail Number");
    }
    else
    {
        dir = opendir(dirPath.c_str());

        if (!dir)
        {
            LOGERR("Error opening directory: %s", dirPath.c_str());
        }
        else
        {
            // Iterate through directory entries
            struct dirent* blockDirEntry = nullptr;

            while ((false == pathFound) && (((blockDirEntry = readdir(dir)) != nullptr)))
            {
                if (string(blockDirEntry->d_name) != "." && string(blockDirEntry->d_name) != "..")
                {
                    string deviceName(blockDirEntry->d_name);

                    // Check if the directory name starts with 'sd'
                    if (deviceName.rfind("sd", 0) == 0)
                    {
                        // Construct the path to the vendor and model file
                        string vendorPath = dirPath + "/" + deviceName + "/" + PLUGIN_USBDEVICE_VENDOR_PATH;
                        string modelPath = dirPath + "/" + deviceName + "/" + PLUGIN_USBDEVICE_MODEL_PATH;
                        string vendorName;
                        string modelName;
                        string blockDeviceName;

                        // Open the vendor file
                        std::ifstream vendorFile(vendorPath);

                        if (!vendorFile.is_open())
                        {
                            LOGERR("Error opening file: %s", vendorPath.c_str());
                            continue;
                        }

                        std::getline(vendorFile, vendorName);
                        if (vendorFile.fail())
                        {
                            LOGERR("Error reading file: %s", vendorPath.c_str());
                            vendorFile.close();
                            continue;
                        }

                        auto endString = vendorName.find_last_not_of(' ');
                        if (endString != std::string::npos)
                        {
                            vendorName.erase(endString + 1);
                        }

                        vendorFile.close();

                        // Open the model file
                        std::ifstream modelFile(modelPath);

                        if (!modelFile.is_open())
                        {
                            LOGERR("Error opening file: %s", modelPath.c_str());
                            continue;
                        }

                        std::getline(modelFile, modelName);
                        if (modelFile.fail())
                        {
                            LOGERR("Error reading file: %s", modelPath.c_str());
                            modelFile.close();
                            continue;
                        }

                        modelFile.close();

                        std::replace(modelName.begin(), modelName.end(), ' ', '_');
                        auto end = modelName.find_last_not_of('_');
                        if (end != std::string::npos)
                        {
                            modelName.erase(end + 1);
                        }

                        blockDeviceName = string("usb-") + vendorName + string("_") + modelName + string("_");

                        const string diskDirPath = string(PLUGIN_USBDEVICE_DEV_DISK_PATH);
                        DIR* diskDir = nullptr;

                        diskDir = opendir(diskDirPath.c_str());

                        if (!diskDir)
                        {
                            LOGERR("Error opening directory: %s", diskDirPath.c_str());
                        }
                        else
                        {
                            // Iterate through directory entries
                            struct dirent* diskDirEntry = nullptr;

                            while ((diskDirEntry = readdir(diskDir)) != nullptr)
                            {
                                if (diskDirEntry->d_type == DT_LNK && string(diskDirEntry->d_name) != "." && string(diskDirEntry->d_name) != "..")
                                {
                                    string usbDeviceName(diskDirEntry->d_name);

                                    // Check  the directory name starts with blockDeviceName Ex: usb-Generic_Flash_Disk_
                                    if (usbDeviceName.rfind("usb", 0) == 0)
                                    {
                                        string symlinkFile = diskDirPath + usbDeviceName;
                                        boost::filesystem::path symlink_path(symlinkFile);

                                        boost::filesystem::path target_path = boost::filesystem::read_symlink(symlink_path);

                                        if ((target_path.string().find(deviceName) != std::string::npos) &&
                                            (usbDeviceName.find(deviceSerialNumber) != std::string::npos))
                                        {
                                            devPath = "/dev/" + deviceName;
                                            pathFound = true;
                                            break;
                                        }
                                    }
                                 }
                           }
                           closedir(diskDir);
                         }
                    }
                }
            }
            // Close the directory
            closedir(dir);
        }
    }
}

uint32_t USBDeviceImplementation::getUSBDescriptorValue(libusb_device_handle *handle, uint16_t languageID, int descriptorIndex, std::string &stringDescriptor)
{
    uint32_t status = Core::ERROR_GENERAL;
    int retValue = LIBUSB_SUCCESS;
    unsigned char descBuf[256];

    // Check that descriptorIndex is valid
    if (descriptorIndex > 0)
    {
        memset(descBuf, 0, sizeof(descBuf));
        retValue = libusb_get_string_descriptor(handle, descriptorIndex, languageID, descBuf, sizeof(descBuf));
        if (retValue < 0)
        {
            retValue = libusb_get_string_descriptor_ascii(handle, descriptorIndex, descBuf, sizeof(descBuf));
            if (retValue < 0) 
            {
                LOGERR("libusb_get_string_descriptor_ascii failed: %s", libusb_strerror((enum libusb_error)retValue));
            }
            else
            {
                stringDescriptor = string(reinterpret_cast<char*>(descBuf));
                status = Core::ERROR_NONE;
            }
            LOGERR("languageID: %u, libusb_get_string_descriptor failed: %s", languageID, libusb_strerror((enum libusb_error)retValue));
        }
        else if (descBuf[1] != LIBUSB_DT_STRING)
        {
            LOGERR("String descriptor not received");
        }
        else if (descBuf[0] > static_cast<unsigned>(retValue))
        {
            LOGERR("String descriptor not received correctly");
        }
        else
        {
            size_t bufStrLen = descBuf[0];
            uint16_t descValue;
            // Copy the string descriptor information

            for (size_t i = USB_STRING_DESC_HEADER_LENGTH; i < bufStrLen; i+=2)
            {
                descValue = (uint16_t)((uint16_t)(descBuf[i]) | (uint16_t)((uint16_t)(descBuf[i + 1]) << 8));
                if (descValue < 0x80)
                {
                    stringDescriptor += static_cast<char>(descValue);
                }
                else
                {
                    stringDescriptor += static_cast<wchar_t>(descValue);
                }
            }
            LOGINFO("stringDescriptor: %s, size:%d",stringDescriptor.c_str(),bufStrLen);
            status = Core::ERROR_NONE;
        }
    }
    else
    {
        LOGERR("Invalid descriptorIndex");
    }
    return status;
}

uint32_t USBDeviceImplementation::getUSBExtInfoStructFromDeviceDescriptor(libusb_device *pDev,
                                                                                                                                   struct libusb_device_descriptor *pDesc,
                                                                                                                                   Exchange::IUSBDevice::USBDeviceInfo *pUSBDeviceInfo)
{
    uint32_t status = Core::ERROR_GENERAL;
    int retValue = LIBUSB_SUCCESS;
    libusb_device_handle *devHandle = nullptr;
    uint8_t langBuff[256] = {0};

    ASSERT (nullptr != pDesc);
    ASSERT (nullptr != pDev);
    ASSERT (nullptr != pUSBDeviceInfo);

    if ((nullptr == pDev) || (nullptr == pDesc) || (nullptr == pUSBDeviceInfo))
    {
        LOGERR ("pDev or pDesc or p_numLanguageIds is nullptr");
    }
    else if ((retValue = libusb_open(pDev, &devHandle)) != LIBUSB_SUCCESS)
    {
        LOGERR ("Error libusb_open: %s", libusb_strerror((enum libusb_error)retValue));
    }
    else if (4 < (retValue = libusb_get_string_descriptor(devHandle, 0, 0, (unsigned char*)langBuff, sizeof(langBuff))))
    {
        LOGINFO("SerialNumber:%d,Manufacturer:%d,Product:%d", pDesc->iSerialNumber,pDesc->iManufacturer,pDesc->iProduct);
        if (Core::ERROR_NONE != (status = getUSBDescriptorValue(devHandle, 0, pDesc->iManufacturer, pUSBDeviceInfo->productInfo1.manufacturer)))
    {
            LOGERR ("Error getUSBDescriptorValue: %s", libusb_strerror((enum libusb_error)retValue));
    }
        if (Core::ERROR_NONE != (status = getUSBDescriptorValue(devHandle, 0, pDesc->iProduct, pUSBDeviceInfo->productInfo1.product)))
        {
            LOGERR ("Error getUSBDescriptorValue: %s", libusb_strerror((enum libusb_error)retValue));
        }
        if (Core::ERROR_NONE != (status = getUSBDescriptorValue(devHandle, 0, pDesc->iSerialNumber, pUSBDeviceInfo->productInfo1.serialNumber)))
    {
            LOGERR ("Error getUSBDescriptorValue: %s", libusb_strerror((enum libusb_error)retValue));
        }
        LOGERR ("Error libusb_get_string_descriptor: %s", libusb_strerror((enum libusb_error)retValue));
    }
    else
    {
        int devIndex = 2;

        pUSBDeviceInfo->numLanguageIds = (langBuff[0] - 2) / 2;

        LOGINFO("numLanguageIDs = %d", pUSBDeviceInfo->numLanguageIds);

        for (int languageCount = 0; languageCount< pUSBDeviceInfo->numLanguageIds; ++languageCount)
        {
            if (0 == languageCount)
            {
                pUSBDeviceInfo->productInfo1.languageId = (uint16_t)((uint16_t)(langBuff[devIndex]) | (uint16_t)((uint16_t)(langBuff[devIndex + 1]) << 8));
        
                if (Core::ERROR_NONE != (status = getUSBDescriptorValue(devHandle, pUSBDeviceInfo->productInfo1.languageId, pDesc->iSerialNumber, pUSBDeviceInfo->productInfo1.serialNumber)))
                {
                    LOGERR ("Error getUSBDescriptorValue: %s", libusb_strerror((enum libusb_error)retValue));
                }
        
                if (Core::ERROR_NONE != (status = getUSBDescriptorValue(devHandle, pUSBDeviceInfo->productInfo1.languageId, pDesc->iManufacturer, pUSBDeviceInfo->productInfo1.manufacturer)))
                {
                    LOGERR ("Error getUSBDescriptorValue: %s", libusb_strerror((enum libusb_error)retValue));
                }
        
                if (Core::ERROR_NONE != (status = getUSBDescriptorValue(devHandle, pUSBDeviceInfo->productInfo1.languageId, pDesc->iProduct, pUSBDeviceInfo->productInfo1.product)))
                {
                    LOGERR ("Error getUSBDescriptorValue: %s", libusb_strerror((enum libusb_error)retValue));
                }
            }
	      else if (1 == languageCount)
	      {
                pUSBDeviceInfo->productInfo2.languageId = (uint16_t)((uint16_t)(langBuff[devIndex]) | (uint16_t)((uint16_t)(langBuff[devIndex + 1]) << 8));
        
                if (Core::ERROR_NONE != (status = getUSBDescriptorValue(devHandle, pUSBDeviceInfo->productInfo2.languageId, pDesc->iSerialNumber, pUSBDeviceInfo->productInfo2.serialNumber)))
                {
                    LOGERR ("Error getUSBDescriptorValue: %s", libusb_strerror((enum libusb_error)retValue));
                }
        
                if (Core::ERROR_NONE != (status = getUSBDescriptorValue(devHandle, pUSBDeviceInfo->productInfo2.languageId, pDesc->iManufacturer, pUSBDeviceInfo->productInfo2.manufacturer)))
                {
                    LOGERR ("Error getUSBDescriptorValue: %s", libusb_strerror((enum libusb_error)retValue));
                }
        
                if (Core::ERROR_NONE != (status = getUSBDescriptorValue(devHandle, pUSBDeviceInfo->productInfo2.languageId, pDesc->iProduct, pUSBDeviceInfo->productInfo2.product)))
                {
                    LOGERR ("Error getUSBDescriptorValue: %s", libusb_strerror((enum libusb_error)retValue));
                }
            }
    	      else if (2 == languageCount)
	      {
                pUSBDeviceInfo->productInfo3.languageId = (uint16_t)((uint16_t)(langBuff[devIndex]) | (uint16_t)((uint16_t)(langBuff[devIndex + 1]) << 8));
        
                if (Core::ERROR_NONE != (status = getUSBDescriptorValue(devHandle, pUSBDeviceInfo->productInfo3.languageId, pDesc->iSerialNumber, pUSBDeviceInfo->productInfo3.serialNumber)))
                {
                    LOGERR ("Error getUSBDescriptorValue: %s", libusb_strerror((enum libusb_error)retValue));
                }
        
                if (Core::ERROR_NONE != (status = getUSBDescriptorValue(devHandle, pUSBDeviceInfo->productInfo3.languageId, pDesc->iManufacturer, pUSBDeviceInfo->productInfo3.manufacturer)))
                {
                    LOGERR ("Error getUSBDescriptorValue: %s", libusb_strerror((enum libusb_error)retValue));
                }
        
                if (Core::ERROR_NONE != (status = getUSBDescriptorValue(devHandle, pUSBDeviceInfo->productInfo3.languageId, pDesc->iProduct, pUSBDeviceInfo->productInfo3.product)))
                {
                    LOGERR ("Error getUSBDescriptorValue: %s", libusb_strerror((enum libusb_error)retValue));
                }
    	      }
    	      else if (3 == languageCount)
	      {
                pUSBDeviceInfo->productInfo4.languageId = (uint16_t)((uint16_t)(langBuff[devIndex]) | (uint16_t)((uint16_t)(langBuff[devIndex + 1]) << 8));
        
                if (Core::ERROR_NONE != (status = getUSBDescriptorValue(devHandle, pUSBDeviceInfo->productInfo4.languageId, pDesc->iSerialNumber, pUSBDeviceInfo->productInfo4.serialNumber)))
                {
                    LOGERR ("Error getUSBDescriptorValue: %s", libusb_strerror((enum libusb_error)retValue));
                }
        
                if (Core::ERROR_NONE != (status = getUSBDescriptorValue(devHandle, pUSBDeviceInfo->productInfo4.languageId, pDesc->iManufacturer, pUSBDeviceInfo->productInfo4.manufacturer)))
                {
                    LOGERR ("Error getUSBDescriptorValue: %s", libusb_strerror((enum libusb_error)retValue));
                }
        
                if (Core::ERROR_NONE != (status = getUSBDescriptorValue(devHandle, pUSBDeviceInfo->productInfo4.languageId, pDesc->iProduct, pUSBDeviceInfo->productInfo4.product)))
                {
                    LOGERR ("Error getUSBDescriptorValue: %s", libusb_strerror((enum libusb_error)retValue));
                }
    	      }
	      else
	      {
                LOGWARN ("Maximum four prdouct info only supported");
	      }
            devIndex += 2; // Move to the next language ID
        }
        status = Core::ERROR_NONE;
    }

    if (devHandle)
    {
        libusb_close(devHandle);
    }

    return status;
}

uint32_t USBDeviceImplementation::getUSBDeviceInfoStructFromDeviceDescriptor(libusb_device *pDev, Exchange::IUSBDevice::USBDeviceInfo *pUSBDeviceInfo)
{
    struct libusb_device_descriptor desc = {0};
    libusb_config_descriptor *config_desc;
    char deviceName[16] = {0}; 
    uint32_t status = Core::ERROR_GENERAL;
    int retValue = LIBUSB_SUCCESS;

    ASSERT (nullptr != pDev);
    ASSERT (nullptr != pUSBDeviceInfo);
    
    if ((nullptr != pDev) && (nullptr != pUSBDeviceInfo))
    {
        if (LIBUSB_SUCCESS != (retValue = libusb_get_device_descriptor(pDev, &desc)))
        {
            LOGERR("Error getting device descriptor: %s", libusb_strerror((enum libusb_error)retValue));
        }
        else
        {
            pUSBDeviceInfo->vendorId = desc.idVendor;
            pUSBDeviceInfo->productId = desc.idProduct;



            sprintf(deviceName, "%03d/%03d", libusb_get_bus_number(pDev), libusb_get_device_address(pDev));
            pUSBDeviceInfo->device.deviceName = std::string(deviceName);

            if (LIBUSB_CLASS_PER_INTERFACE == desc.bDeviceClass)
            {
                getUSBDevicClassFromInterfaceDescriptor(pDev, pUSBDeviceInfo->device.deviceClass, pUSBDeviceInfo->device.deviceSubclass);
            }

            if (LIBUSB_CLASS_MASS_STORAGE != pUSBDeviceInfo->device.deviceClass)
            {
                pUSBDeviceInfo->device.deviceClass = desc.bDeviceClass;
                pUSBDeviceInfo->device.deviceSubclass = desc.bDeviceSubClass;
            }
            else
            {
                string serialNumber;

                /* Getting the device path and device serialNumber */
                getDevicePathFromDevice(pDev, pUSBDeviceInfo->device.devicePath, pUSBDeviceInfo->serialNumber);
            }

            /* Need to check how to get flag */
            pUSBDeviceInfo->flags = DEVICE_FLAGS_DRIVER_AVAILABLE;
            pUSBDeviceInfo->portNumber = libusb_get_port_number(pDev);
            pUSBDeviceInfo->protocol = desc.bDeviceProtocol;
            pUSBDeviceInfo->busSpeed = (Exchange::IUSBDevice::USBDeviceSpeed)libusb_get_device_speed(pDev);

            if (LIBUSB_SUCCESS == (retValue = libusb_get_active_config_descriptor ( pDev, &config_desc )))
            {
                if (config_desc->bmAttributes & LIBUSB_CONFIG_ATT_SELF_POWERED )
                {
                    pUSBDeviceInfo->deviceStatus = WPEFramework::Exchange::IUSBDevice::USBDeviceStatus::DEVICE_STATUS_SELF_POWERED | \
                                        WPEFramework::Exchange::IUSBDevice::USBDeviceStatus::DEVICE_STATUS_ACTIVE;
                }
                else
                {
                    pUSBDeviceInfo->deviceStatus = WPEFramework::Exchange::IUSBDevice::USBDeviceStatus::DEVICE_STATUS_ACTIVE;
                }
                LOGINFO("bmAttributes: %u",config_desc->bmAttributes);
            }
            else
            {
                pUSBDeviceInfo->deviceStatus = WPEFramework::Exchange::IUSBDevice::USBDeviceStatus::DEVICE_STATUS_NO_DEVICE_CONNECTED;
                LOGERR("Error libusb_get_active_config_descriptor: %s", libusb_strerror((enum libusb_error)retValue));
            }
            if (Core::ERROR_NONE != (status = getUSBExtInfoStructFromDeviceDescriptor(pDev, &desc, pUSBDeviceInfo)))
            {
                LOGERR("Error getUSBExtInfoStructFromDeviceDescriptor");
            }
            else
            {
                status = Core::ERROR_NONE;
            }
        }
    }
    else
    {
        LOGERR("pDev or pUSBDeviceInfo is nullptr");
    }















    return status;
}

void USBDeviceImplementation::getUSBDevicClassFromInterfaceDescriptor(libusb_device *pDev, uint8_t &bDeviceClass, uint8_t &bDeviceSubClass)
{
    // Get configuration descriptor
    libusb_config_descriptor *config = nullptr;
    if (libusb_get_config_descriptor(pDev, 0, &config) == LIBUSB_SUCCESS)
    {
        if (config)
        {
            // Iterate over interfaces
            for (int j = 0; j < config->bNumInterfaces; j++)
            {
                const libusb_interface *interface = &config->interface[j];
                for (int k = 0; k < interface->num_altsetting; k++)
                {
                    const libusb_interface_descriptor *interface_desc = &interface->altsetting[k];

                    // Check if the interface class is Mass Storage
                    if (LIBUSB_CLASS_MASS_STORAGE == interface_desc->bInterfaceClass)
                    {
                        LOGINFO("Interface %d is a mass storage interface", j);
                        bDeviceClass = interface_desc->bInterfaceClass;
                        bDeviceSubClass = interface_desc->bInterfaceSubClass;
                    }
                }
            }
            libusb_free_config_descriptor(config);
        }
    }
}

uint32_t USBDeviceImplementation::getUSBDeviceStructFromDeviceDescriptor(libusb_device *pDev, Exchange::IUSBDevice::USBDevice *pusbDevice)
{
      struct libusb_device_descriptor desc = {0};
      uint32_t status = Core::ERROR_GENERAL;
      int retValue = LIBUSB_SUCCESS;

      ASSERT (nullptr != pDev);
      ASSERT (nullptr != pusbDevice);

      if ((nullptr != pDev) && (nullptr != pusbDevice))
      {
          retValue = libusb_get_device_descriptor(pDev, &desc);

          if (LIBUSB_SUCCESS == retValue)
          {
              pusbDevice->deviceName = (boost::format("%03d/%03d") % (int)libusb_get_bus_number(pDev) % (int)libusb_get_device_address(pDev)).str();

              if (LIBUSB_CLASS_PER_INTERFACE == desc.bDeviceClass)
              {
                  getUSBDevicClassFromInterfaceDescriptor(pDev, pusbDevice->deviceClass, pusbDevice->deviceSubclass);
              }
		 else
              {
                  pusbDevice->deviceClass = desc.bDeviceClass;
                  pusbDevice->deviceSubclass = desc.bDeviceSubClass;
              }

              if (LIBUSB_CLASS_MASS_STORAGE == pusbDevice->deviceClass)
              {
                  string serialNumber;
                  int retryCount = 2;

                  while( retryCount )
                  {
                    /* Getting the device path */
                    getDevicePathFromDevice(pDev, pusbDevice->devicePath, serialNumber);

                    if(pusbDevice->devicePath.empty())
                    {
                      LOGWARN ("device Path not found retrying it");
                      --retryCount;
                      sleep(2);
                    }
                    else
                    {
                       break;
                    }
                  }
              }

              status = Core::ERROR_NONE;
          }
          else
          {
              LOGERR ("Error getting device descriptor: %s",libusb_strerror((enum libusb_error)retValue));
          }
      }

      return status;
}

/**
 * Register a notification callback
 */
Core::hresult USBDeviceImplementation::Register(Exchange::IUSBDevice::INotification *notification)
{
    ASSERT (nullptr != notification);

    _adminLock.Lock();

    // Make sure we can't register the same notification callback multiple times
    if (std::find(_usbDeviceNotification.begin(), _usbDeviceNotification.end(), notification) == _usbDeviceNotification.end())
    {
        LOGINFO("Register notification");
        _usbDeviceNotification.push_back(notification);
        notification->AddRef();
    }

    _adminLock.Unlock();

    return Core::ERROR_NONE;
}

/**
 * Unregister a notification callback
 */
Core::hresult USBDeviceImplementation::Unregister(Exchange::IUSBDevice::INotification *notification )
{
    uint32_t status = Core::ERROR_GENERAL;

    ASSERT (nullptr != notification);

    _adminLock.Lock();

    // Make sure we can't unregister the same notification callback multiple times
    auto itr = std::find(_usbDeviceNotification.begin(), _usbDeviceNotification.end(), notification);
    if (itr != _usbDeviceNotification.end())
    {
        (*itr)->Release();
        LOGINFO("Unregister notification");
        _usbDeviceNotification.erase(itr);
        status = Core::ERROR_NONE;
    }
    else
    {
        LOGERR("notification not found");
    }

    _adminLock.Unlock();

    return status;
}

void USBDeviceImplementation::dispatchEvent(Event event, Exchange::IUSBDevice::USBDevice usbDevice)
{
    Core::IWorkerPool::Instance().Submit(Job::Create(this, event, usbDevice));
}

void USBDeviceImplementation::Dispatch(Event event, const Exchange::IUSBDevice::USBDevice usbDevice)
{
     _adminLock.Lock();

     std::list<Exchange::IUSBDevice::INotification*>::const_iterator index(_usbDeviceNotification.begin());

     switch(event) {
         case USBDEVICE_HOTPLUG_EVENT_DEVICE_ARRIVED:
            LOGINFO("USBDEVICE_HOTPLUG_EVENT_DEVICE_ARRIVED Received");
             while (index != _usbDeviceNotification.end())
             {
                 LOGINFO("Call OnDevicePluggedIn");

                 (*index)->OnDevicePluggedIn(usbDevice);
                 ++index;
             }
        break;

         case USBDEVICE_HOTPLUG_EVENT_DEVICE_LEFT:
            LOGINFO("USBDEVICE_HOTPLUG_EVENT_DEVICE_LEFT Received");
             while (index != _usbDeviceNotification.end())
             {
                 LOGINFO("Call OnDevicePluggedOut");
                 (*index)->OnDevicePluggedOut(usbDevice);
                 ++index;
             }
        break;

        default:
             break;
     }

     _adminLock.Unlock();
}

Core::hresult USBDeviceImplementation::GetDeviceList(IUSBDeviceIterator*& devices) const
{
    uint32_t status = Core::ERROR_GENERAL;
    std::list<Exchange::IUSBDevice::USBDevice> usbDeviceList;
    libusb_device **devs = nullptr;
    ssize_t devCount = 0;

    _adminLock.Lock();

    LOGINFO("GetDeviceList");

    devCount = libusb_get_device_list(NULL, &devs);

    LOGINFO("devCount:%zd", devCount);

    if (devCount > 0) 
    {
        for (int index = 0; index < devCount; index++)
        {
            Exchange::IUSBDevice::USBDevice usbDevice = {0};

            status = USBDeviceImplementation::instance()->getUSBDeviceStructFromDeviceDescriptor(devs[index], &usbDevice);

            if (Core::ERROR_NONE == status)
            {
                LOGINFO ("usbDevice.deviceClass: %u usbDevice.deviceSubclass:%u usbDevice.deviceName:%s devicePath:%s", 
                                       usbDevice.deviceClass,
                                       usbDevice.deviceSubclass,
                                       usbDevice.deviceName.c_str(),
                                       usbDevice.devicePath.c_str());

                usbDeviceList.emplace_back(usbDevice);
            }
          else
            {
                LOGWARN ("getUSBDeviceStructFromDeviceDescriptor Failed");
            }
        }

        libusb_free_device_list(devs, 1);
        if (Core::ERROR_NONE == status)
        {
            devices = (Core::Service<RPC::IteratorType<Exchange::IUSBDevice::IUSBDeviceIterator>>::Create<Exchange::IUSBDevice::IUSBDeviceIterator>(usbDeviceList));
        }
    }
    else
    {
        LOGWARN("USBDevice Not found");
        status = Core::ERROR_NONE;
    }
    _adminLock.Unlock();

    return status;
}

Core::hresult USBDeviceImplementation::GetDeviceInfo(const string &deviceName, USBDeviceInfo& deviceInfo) const
{
    uint32_t status = Core::ERROR_GENERAL;
    libusb_device **devs = nullptr;
    ssize_t devCount = 0;

    _adminLock.Lock();

    LOGINFO("GetDeviceInfo");

    devCount = libusb_get_device_list(NULL, &devs);

    LOGINFO("devCount:%zd", devCount);

    if (devCount > 0)
    {
        for (int index = 0; index < devCount; index++)
        {
            char usbDeviceName[10] = {0};

            (void)sprintf(usbDeviceName, "%03d/%03d", libusb_get_bus_number(devs[index]), libusb_get_device_address(devs[index]));

            if (deviceName.compare(string(usbDeviceName)) != 0)
            {
                continue;
            }
            else
            {
                uint8_t portPath[8] = {0}; // Maximum 8 levels (depends on USB architecture)

                status = USBDeviceImplementation::instance()->getUSBDeviceInfoStructFromDeviceDescriptor(devs[index], &deviceInfo);
                if (Core::ERROR_NONE != status)
                {
                    deviceInfo.deviceLevel = libusb_get_port_numbers(devs[index], portPath, sizeof(portPath));
                    if ( 1 < deviceInfo.deviceLevel )
                    {
                        for (ssize_t j = 0; j < devCount; j++)
                        {
                            libusb_device *device = devs[j];
                            uint8_t portNo = libusb_get_port_number(device);
                            if (portPath[deviceInfo.deviceLevel - 2] == portNo)
                            {
                                deviceInfo.parentId = j;
                            }
                        }
                    }
                    else
                    {
                        deviceInfo.parentId = 0;
                    }
                }
                else
                {
                    LOGWARN ("getUSBDeviceInfoStructFromDeviceDescriptor Failed");
                }
            }
        }
        libusb_free_device_list(devs, 1);
    }
    else
    {
        LOGWARN("USBDevice Not found");
    }
    _adminLock.Unlock();

    return status;
}

Core::hresult USBDeviceImplementation::BindDriver(const string &deviceName) const
{
    uint32_t status = Core::ERROR_GENERAL;
    libusb_device **devs = nullptr;
    ssize_t devCount = 0;
    int retValue = LIBUSB_SUCCESS;
    libusb_device_handle *devHandle = nullptr;

    _adminLock.Lock();

    LOGINFO("BindDriver");

    devCount = libusb_get_device_list(NULL, &devs);

    LOGINFO("devCount:%zd", devCount);

    if (devCount > 0)
    {
        for (int index = 0; index < devCount; index++)
        {
            char usbDeviceName[10] = {0};

            (void)sprintf(usbDeviceName, "%03d/%03d", libusb_get_bus_number(devs[index]), libusb_get_device_address(devs[index]));

            if (deviceName.compare(string(usbDeviceName)) != 0)
            {
                continue; /*Not matched continue to check next dev struct */
            }
            else
            {
                if (nullptr == devs[index])
                {
                    LOGERR ("devs is nullptr");
                }
                else if ((retValue = libusb_open(devs[index], &devHandle)) != LIBUSB_SUCCESS)
                {
                    LOGERR ("Error libusb_open: %s", libusb_strerror((enum libusb_error)retValue));
                }
                else
                {
                    int kernelDriverStatus = libusb_kernel_driver_active(devHandle, 0);

                    if (kernelDriverStatus < 0)
                    {
                        LOGERR("Error libusb_kernel_driver_active: %s", libusb_strerror((enum libusb_error)kernelDriverStatus));
                    }
                    else if (kernelDriverStatus == 0)
                    {
                        // No driver is active, so attach it
                        if ((retValue = libusb_attach_kernel_driver(devHandle, 0)) != LIBUSB_SUCCESS)
                        {
                            LOGERR("Error libusb_attach_kernel_driver: %s", libusb_strerror((enum libusb_error)retValue));
                        }
                        else
                        {
                            status = Core::ERROR_NONE;
                            LOGINFO("BindDriver succeeded");
                        }
                    }
                    else // kernelDriverStatus == 1
                    {
                        LOGINFO("libusb kernel driver is already active");
                        status = Core::ERROR_NONE;
                        LOGINFO("BindDriver succeeded");
                    }

                    // Always close the device handle
                    libusb_close(devHandle);
                    devHandle = nullptr;
                }
                break;
            }
        }

        libusb_free_device_list(devs, 1);
    }
    else
    {
        LOGWARN("USBDevice Not found");
    }
    _adminLock.Unlock();

    return status;
}

Core::hresult USBDeviceImplementation::UnbindDriver(const string &deviceName) const
{
    uint32_t status = Core::ERROR_GENERAL;
    libusb_device **devs = nullptr;
    ssize_t devCount = 0;
    int retValue = LIBUSB_SUCCESS;
    libusb_device_handle *devHandle = nullptr;

    _adminLock.Lock();

    LOGINFO("UnbindDriver");

    devCount = libusb_get_device_list(NULL, &devs);

    LOGINFO("devCount:%zd", devCount);

    if (devCount > 0)
    {
        for (int index = 0; index < devCount; index++)
        {
            char usbDeviceName[10] = {0};

            (void)sprintf(usbDeviceName, "%03d/%03d", libusb_get_bus_number(devs[index]), libusb_get_device_address(devs[index]));

            if (deviceName.compare(string(usbDeviceName)) != 0)
            {
                continue;
            }
            else
            {
                if (nullptr == devs[index])
                {
                    LOGERR ("devs is nullptr");
                }
                else if ((retValue = libusb_open(devs[index], &devHandle)) != LIBUSB_SUCCESS)
                {
                    LOGERR ("Error libusb_open: %s", libusb_strerror((enum libusb_error)retValue));
                }
                else
                {
                    int kernelDriverStatus = libusb_kernel_driver_active(devHandle, 0);

                    if (kernelDriverStatus < 0)
                    {
                        LOGERR("Error libusb_kernel_driver_active: %s", libusb_strerror((enum libusb_error)kernelDriverStatus));
                    }
                    else if (kernelDriverStatus == 0)
                    {
                        // No driver is active
                        status = Core::ERROR_NONE;
                        LOGINFO("No kernel driver is active");
                    }
                    else // kernelDriverStatus == 1
                    {
                        // A driver is active, try to detach it
                        if ((retValue = libusb_detach_kernel_driver(devHandle, 0)) != LIBUSB_SUCCESS)
                        {
                            LOGERR("Error libusb_detach_kernel_driver: %s", libusb_strerror((enum libusb_error)retValue));
                        }
                        else
                        {
                            status = Core::ERROR_NONE;
                            LOGINFO("Kernel driver successfully detached");
                        }
                    }

                    // Always close the device handle
                    libusb_close(devHandle);
                    devHandle = nullptr;
                }
                break;
            }
        }

        libusb_free_device_list(devs, 1);
    }
    else
    {
        LOGWARN("USBDevice Not found");
    }
    _adminLock.Unlock();

    return status;
}
} // namespace Plugin
} // namespace WPEFramework
