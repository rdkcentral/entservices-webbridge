
#include <ftw.h>
#include <mutex>
#include "RequestHandler.h"
#include "UtilsLogging.h"
#include <interfaces/IStore2.h>
#include <sys/statvfs.h>

#define MAX_NUM_OF_FILE_DESCRIPTORS     256
#define DEFAULT_STORAGE_DEV_BLOCK_SIZE  512
#define STORAGE_DIR_PERMISSION          0755


namespace WPEFramework
{
    namespace Plugin
    {
        std::mutex mInstanceMutex;
        static RequestHandler::StorageSize gStorageSize;
        std::mutex RequestHandler::mStorageSizeLock;

        RequestHandler& RequestHandler::getInstance()
        {
            static RequestHandler instance;
            return instance;
        }

        RequestHandler::RequestHandler(): mService(nullptr)
        ,mStorageManagerImplLock()
        ,mPersistentStoreRemoteStoreObject(nullptr)
        {
            LOGINFO("Create RequestHandler Instance");
        }

        RequestHandler::~RequestHandler()
        {
            LOGINFO("Delete RequestHandler Instance");
        }

        void RequestHandler::setCurrentService(PluginHost::IShell* service)
        {
            if (service != nullptr)
            {
                mService = service;
            }
            else
            {
                LOGERR("mService is null \n");
            }
        }

        Core::hresult RequestHandler::populateAppInfoCacheFromStoragePath()
        {
            Core::hresult status = Core::ERROR_GENERAL;
            std::string errorReason;

            DIR* dir = opendir(mBaseStoragePath.c_str());
            if (!dir)
            {
                LOGERR("Failed to open storage directory: %s. Error: %s", mBaseStoragePath.c_str(), strerror(errno));
            }
            else
            {
                struct dirent* entry;
                while ((entry = readdir(dir)) != nullptr)
                {
                    LOGINFO("entry->d_name: %s", entry->d_name);
                    if (entry->d_type != DT_DIR || strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0 || !isValidAppStorageDirectory(entry->d_name))
                    {
                        LOGERR("entry->d_name: %s d_type : %d - not a directory, it is [.] or [..], or invalid appId - SKIPPED\n", entry->d_name, entry->d_type);
                        continue;
                    }

                    std::string appId = entry->d_name;

                    //capture the path
                    std::string appFilePath = mBaseStoragePath + '/' + appId;

                    //get the uid, gid by stat
                    struct stat dirStat;
                    if (stat(appFilePath.c_str(), &dirStat) == -1)
                    {
                        LOGERR("unable to get the status of app directory. appFilePath: %s", appFilePath.c_str());
                        continue;
                    }

                    //update the StorageAppInfo object and populate the Map
                    StorageAppInfo storageInfo;
                    storageInfo.path    = std::move(appFilePath);
                    storageInfo.uid     = static_cast<uint32_t>(dirStat.st_uid);
                    storageInfo.gid     = static_cast<uint32_t>(dirStat.st_gid);

                    //get the quotaKB
                    status = appQuotaSizeProperty(GET, appId, &storageInfo.quotaKB);
                    if (Core::ERROR_NONE != status)
                    {
                        LOGERR("appQuotaSizeProperty Failed for App Quota size retrivel, appId:%s!!!", appId.c_str());
                        storageInfo.quotaKB = 0;
                    }

                    storageInfo.usedKB  = static_cast<uint32_t>(getDirectorySizeInBytes(storageInfo.path) / 1024); //get the used size

                    LOGINFO("Retrieved storageInfo for appId: %s " \
                    "userId: %d groupId: %d quotaKB: %u usedKB: %u path: %s",
                    appId.c_str(), storageInfo.uid, storageInfo.gid, storageInfo.quotaKB, storageInfo.usedKB, storageInfo.path.c_str());

                    if(!createAppStorageInfoByAppID(appId,storageInfo))
                    {
                        LOGERR("Failed to create storage at mStorageAppInfo\n");
                        status = Core::ERROR_GENERAL;
                    }
                }
                closedir(dir);
            }
        return status;
        }

        Core::hresult RequestHandler::createPersistentStoreRemoteStoreObject()
        {
            Core::hresult status = Core::ERROR_GENERAL;
            LOGINFO("Entered createPersistentStoreRemoteStoreObject");
            LOGINFO("mService: %p", mService);
            if (nullptr == mService)
            {
                LOGERR("mService is null \n");
            }
            else if (nullptr == (mPersistentStoreRemoteStoreObject = mService->QueryInterfaceByCallsign<WPEFramework::Exchange::IStore2>("org.rdk.PersistentStore")))
            {
                LOGERR("mPersistentStoreRemoteStoreObject is null \n");
            }
            else
            {
                LOGINFO("created PersistentStore Object\n");
                status = Core::ERROR_NONE;
            }
            return status;
        }

        void RequestHandler::releasePersistentStoreRemoteStoreObject()
        {
            ASSERT(nullptr != mPersistentStoreRemoteStoreObject);
            if(mPersistentStoreRemoteStoreObject)
            {
                mPersistentStoreRemoteStoreObject->Release();
                mPersistentStoreRemoteStoreObject = nullptr;
            }
        }


        /***
        * @brief: Validate the given directory name is a proper AppID.
        *
        * The rules are that only dots (.), dashes (-), underscore (_) and
        * alphanumeric characters are allow, with the following additional restrictions:
        *      - the first and last characters must be alphanumeric
        *      - double dots (..) are not allowed
        *
        * @param[in] dirName                   : App identifier for the application
        *
        * @return                              : bool
        ***/
        bool RequestHandler::isValidAppStorageDirectory(const std::string& dirName)
        {
            if (dirName.empty())
                return false;

            auto it = dirName.begin();
            char ch = *it++;
            if (!isalnum(ch))
            {
                return false;
            }

            for (; it != dirName.end(); ++it)
            {
                char prev = ch;
                ch = *it;

                // check for double dots
                if (ch == '.')
                {
                    if (prev == '.')
                        return false;
                }
                else if (!isalnum(ch) && (ch != '-') && (ch != '_'))
                {
                    return false;
                }
            }

            // check the last character is alphanumeric
            return isalnum(ch);
        }


        /***
        * @brief Get / Set / Delete the QuotaSize property for the given app.
        * Handles persistent property for a given app based on the appId and key.
        *
        * @param[in] appId                   : App identifier for the application
        * @param[in] StorageActionType       : SET/ GET / DELETE Action to be performed on the property
        * @param[in/out] value               : value of the QuataSize, based on the ActionType value set or retrieved.
        *
        * @return                            : Core::<StatusCode>
        */

        Core::hresult RequestHandler::appQuotaSizeProperty(RequestHandler::StorageActionType actionType, const std::string& appId, uint32_t* quotaValue)
        {
            Core::hresult status = Core::ERROR_GENERAL;
            const std::string key = "quotaSize";
            uint32_t ttl = 0;

            /* Check if appId is empty */
            if (appId.empty())
            {
                LOGERR("App ID is empty");
            }
            else
            {
                /* Ensure the persistent store object is created if necessary */
                if (mPersistentStoreRemoteStoreObject == nullptr && Core::ERROR_NONE != createPersistentStoreRemoteStoreObject()) 
                {
                    LOGERR("Failed to create PersistentStoreRemoteStoreObject");
                    status = Core::ERROR_GENERAL;  // Set status to indicate failure
                }
                else
                {
                    ASSERT(mPersistentStoreRemoteStoreObject != nullptr);

                    /* Perform action based on the StorageActionType */
                    if (mPersistentStoreRemoteStoreObject != nullptr)
                    {
                        switch (actionType)
                        {
                            case SET:
                            {
                                status = mPersistentStoreRemoteStoreObject->SetValue(Exchange::IStore2::ScopeType::DEVICE, appId, key, std::to_string(*quotaValue), 0);
                                if (Core::ERROR_NONE != status)
                                {
                                    LOGERR("SetValue Failed: appId[%s] Key[%s] status[%d]", appId.c_str(), key.c_str(), status);
                                }
                                else
                                {
                                    LOGINFO("SetValue: appId[%s] Key[%s] value[%d]", appId.c_str(), key.c_str(), *quotaValue);
                                }
                            }
                            break;

                            case GET:
                            {
                                std::string quotaSize;
                                status = mPersistentStoreRemoteStoreObject->GetValue(Exchange::IStore2::ScopeType::DEVICE, appId, key, quotaSize, ttl);
                                if (Core::ERROR_NONE != status)
                                {
                                    LOGERR("GetValue Failed: appId[%s] Key[%s] status[%d]", appId.c_str(), key.c_str(), status);
                                }
                                else
                                {
                                    LOGINFO("appId[%s] Key[%s] value retrived[%s]", appId.c_str(), key.c_str(), quotaSize.c_str());
                                    *quotaValue = static_cast<uint32_t>(std::stoul(quotaSize,nullptr,0));
                                }
                            }
                            break;

                            case DELETE:
                            {
                                status = mPersistentStoreRemoteStoreObject->DeleteKey(Exchange::IStore2::ScopeType::DEVICE, appId, key);
                                if (Core::ERROR_NONE != status)
                                {
                                    LOGERR("DeleteKey Failed: appId[%s] Key[%s] status[%d]", appId.c_str(), key.c_str(), status);
                                }
                                else
                                {
                                    LOGINFO("appId[%s] Deleted Key[%s] ", appId.c_str(), key.c_str());
                                }
                            }
                            break;

                            default:
                            {
                                LOGERR("Invalid storage type : %d", actionType);
                            }
                            break;
                        }
                    }
                }
            }

            return status;
        }

        /**
        * @brief : Adds file sizes to calculate total storage usage.
        */
        int RequestHandler::getSize(const char *path, const struct stat *statPtr, int currentFlag, struct FTW *internalFtwUsage )
        {
            if (currentFlag == FTW_F && statPtr != nullptr)
            {
                std::lock_guard<std::mutex> storageSizelock(mStorageSizeLock);
                if (!gStorageSize.blockSize)
                {
                    /* Check and Store the current storage dev block size */
                    gStorageSize.blockSize = (0 != statPtr->st_blksize) ? statPtr->st_blksize : DEFAULT_STORAGE_DEV_BLOCK_SIZE;
                    LOGINFO("path: %s dev blksize:%lu blockSize is set to %llu", path, statPtr->st_blksize, gStorageSize.blockSize);
                }

                // Calculate used bytes
                uint64_t usedBytesForFile = ((uint64_t)statPtr->st_size > ((uint64_t)statPtr->st_blocks * (uint64_t)gStorageSize.blockSize)) ?
                                                            ((uint64_t)statPtr->st_blocks *(uint64_t)gStorageSize.blockSize) :
                                                            (uint64_t)statPtr->st_size;
                gStorageSize.usedBytes += usedBytesForFile;
                LOGINFO("path: %s usedBytes: %llu blockSize: %llu", path, gStorageSize.usedBytes, gStorageSize.blockSize);
            }
            (void)internalFtwUsage;
            return 0;
        }

        /**
        * @brief : Get the directory size traversing through the given directory path
        */
        uint64_t RequestHandler::getDirectorySizeInBytes(const std::string &path)
        {
            const int flags = FTW_DEPTH | FTW_MOUNT | FTW_PHYS;

            std::unique_lock<std::mutex> storageSizelock(mStorageSizeLock);
            gStorageSize.usedBytes = 0;
            storageSizelock.unlock();
            const int result = nftw(path.c_str(), getSize, MAX_NUM_OF_FILE_DESCRIPTORS, flags);

            if (result == -1)
            {
                LOGERR("nftw returned with [%d]", result);
            }
            storageSizelock.lock();
            return gStorageSize.usedBytes;
        }

        /**
        * @brief : Create or Update App storageInfo map entry for the given appId
        */
        bool RequestHandler::createAppStorageInfoByAppID(const std::string& appId, StorageAppInfo &storageInfo)
        {
            bool result = false;

            if (storageInfo.path.empty())
            {
                LOGWARN("AppId[%s] storage path is empty!", appId.c_str());
            }
            else
            {
                /* Check if the appId exists */
                auto it = mStorageAppInfo.find(appId);
                if (it != mStorageAppInfo.end())
                {
                    /* Update existing entry */
                    it->second.path     = storageInfo.path;
                    it->second.uid      = storageInfo.uid;
                    it->second.gid      = storageInfo.gid;
                    it->second.quotaKB  = storageInfo.quotaKB;
                    LOGINFO("Existing storage entry updated for appId: %s " \
                                "userId: %d groupId: %d quotaKB: %u usedKB: %u path: %s",
                                appId.c_str(), it->second.uid, it->second.gid, it->second.quotaKB, it->second.usedKB, it->second.path.c_str());
                }
                else
                {
                    /* Create new entry */
                    mStorageAppInfo[appId].path    = storageInfo.path;
                    mStorageAppInfo[appId].uid     = storageInfo.uid;
                    mStorageAppInfo[appId].gid     = storageInfo.gid;
                    mStorageAppInfo[appId].quotaKB = storageInfo.quotaKB;
                    mStorageAppInfo[appId].usedKB  = storageInfo.usedKB;
                    LOGINFO("Created new storage entry for appId: %s " \
                                "userId: %d groupId: %d quotaKB: %u usedKB: %u path: %s",
                                appId.c_str(), storageInfo.uid, storageInfo.gid, storageInfo.quotaKB, storageInfo.usedKB, storageInfo.path.c_str());
                }
                result = true;
            }
            return result;
        }

        /**
        * @brief : c App StorageInfo for the given appId
        *
        * @return : True if App StorageInfo is available, false otherwise.
        */
        bool RequestHandler::retrieveAppStorageInfoByAppID(const string &appId, StorageAppInfo &storageInfo)
        {
            bool result = false;

            /* Check if the appId exists */
            auto it = mStorageAppInfo.find(appId);
            if (it != mStorageAppInfo.end())
            {
                /* Check if the existing storage directory is accessible */
                if (access(it->second.path.c_str(), F_OK) == 0)
                {
                    /* Updating the usedKB based on the directory size */
                    it->second.usedKB = static_cast<uint32_t>(getDirectorySizeInBytes(it->second.path) / 1024);
                    storageInfo.path    = it->second.path;
                    storageInfo.uid     = it->second.uid;
                    storageInfo.gid     = it->second.gid;
                    storageInfo.quotaKB = it->second.quotaKB;
                    storageInfo.usedKB  = it->second.usedKB;
                    LOGINFO("App storage entry found for appId: %s " \
                        "userId: %d groupId: %d quotaKB: %u usedKB: %u path: %s",
                        appId.c_str(), storageInfo.uid, storageInfo.gid, storageInfo.quotaKB, storageInfo.usedKB, storageInfo.path.c_str());
                    result = true;
                }
                else
                {
                    /* Not accessible, need to remove existing storage entry forcibly, recreate it */
                    if (removeAppStorageInfoByAppID(appId))
                    {
                        LOGWARN("Storage path for appID[%s] not accessible - forcibly removed!", it->second.path.c_str());
                    }
                    else
                    {
                        LOGERR("Failed to remove app storage entry forcibly: %s", it->second.path.c_str());
                    }
                }
            }
            else
            {
                LOGWARN("AppId[%s] storage entry not found/created", appId.c_str());
            }
            return result;
        }

        /**
        * @brief : Remove App StorageInfo for the given appId
        *
        * @return : True if App StorageInfo map is available and erased, false otherwise.
        */
        bool RequestHandler::removeAppStorageInfoByAppID(const string &appId)
        {
            bool result = false;

            /* Check if appId StorageInfo is found, erase it */
            auto it = mStorageAppInfo.find(appId);
            if (it != mStorageAppInfo.end())
            {
                LOGINFO("App storage entry erased for appId: %s " \
                            "userId: %d groupId: %d quotaKB: %u usedKB: %u path: %s",
                            appId.c_str(), it->second.uid, it->second.gid, it->second.quotaKB, it->second.usedKB, it->second.path.c_str());
                mStorageAppInfo.erase(appId);
                appQuotaSizeProperty(DELETE, appId, nullptr); //Remove the persistent store entry
                result = true;
            }
            else
            {
                LOGWARN("AppId[%s] StorageInfo not found", appId.c_str());
            }
            return result;
        }

        /**
        * @brief : Check whether enough storag space is to create a new app Directory.
        *
        * @return : True if enough space is available, false otherwise.
        */
        bool RequestHandler::hasEnoughStorageFreeSpace(const std::string& baseDir, uint32_t requiredSpaceKB)
        {
            bool hasEnoughSpace = false;
            struct statvfs statFs;
            uint64_t curStorageFreeSpaceKB = 0;
            uint64_t existingAppsReservationSpaceKB = 0;
            int64_t availableSizeKB = 0;

            if (baseDir.empty())
            {
                LOGERR("Invalid baseDir: path cannot be empty");
            }
            else
            {
                if (statvfs(baseDir.c_str(), &statFs) == 0)
                {
                    /* Store the current storage dev block size */
                    {
                        std::lock_guard<std::mutex> storageSizelock(mStorageSizeLock);

                        gStorageSize.blockSize = (statFs.f_bsize != 0) ? statFs.f_bsize : DEFAULT_STORAGE_DEV_BLOCK_SIZE; /* Fallback to default block size */
                        LOGINFO("path: %s f_bsize:%lu f_frsize:%lu, blockSize is set to %llu", baseDir.c_str(), statFs.f_bsize, statFs.f_frsize, gStorageSize.blockSize);
                    }

                    /* Calculate the current available storage in KB */
                    curStorageFreeSpaceKB = (static_cast<uint64_t>(statFs.f_bfree) * statFs.f_frsize) / 1024;

                    /* Compute total reserved space for existing applications */
                    for (auto& entry : mStorageAppInfo)
                    {
                        entry.second.usedKB = static_cast<uint32_t>(getDirectorySizeInBytes(entry.second.path) / 1024);

                        /* Ensure applications do not exceed allocated space */
                        if (entry.second.usedKB > entry.second.quotaKB)
                        {
                            LOGERR("Application storage usage exceeded allocation: %s (Allocated: %u KB, Used: %u KB)",
                            entry.second.path.c_str(), entry.second.quotaKB, entry.second.usedKB);
                        }
                        else
                        {
                            /* quotaKB is total allocated space for the app, and usedKB is the current usage */
                            existingAppsReservationSpaceKB += (entry.second.quotaKB - entry.second.usedKB);
                        }
                    }

                    /* Calculate available space for new apps */
                    availableSizeKB = static_cast<int64_t>(curStorageFreeSpaceKB) - static_cast<int64_t>(existingAppsReservationSpaceKB);

                    /* Determine if required space is available */
                    if (availableSizeKB >= static_cast<int64_t>(requiredSpaceKB))
                    {
                        LOGINFO("Enough space available. Required: %u KB, Available: %llu KB",
                        requiredSpaceKB, availableSizeKB);
                        hasEnoughSpace = true;
                    }
                    else
                    {
                        LOGERR("Not enough space available. Required: %u KB, Available: %llu KB",
                        requiredSpaceKB, availableSizeKB);
                    }
                }
                else
                {
                    LOGERR("Failed to get filesystem stats for path: %s, Error: %s",
                    baseDir.c_str(), strerror(errno));
                    std::lock_guard<std::mutex> storageSizelock(mStorageSizeLock);
                    gStorageSize.blockSize = DEFAULT_STORAGE_DEV_BLOCK_SIZE; /* Fallback to default block size */
                }
            }

            return hasEnoughSpace;
        }

        /**
        * @brief : Creates storage for a given app id and returns the storage path
        */
        Core::hresult RequestHandler::CreateStorage(const std::string& appId, const uint32_t& size, std::string& path, std::string& errorReason)
        {
            Core::hresult status = Core::ERROR_GENERAL;
            std::string appDir = "";
            StorageAppInfo storageInfo;

            LOGINFO("Entered CreateStorage Implementation appId: %s", appId.c_str());
            if (appId.empty())
            {
                LOGERR("Invalid App ID");
                errorReason = "appId cannot be empty";
            }
            else
            {
                std::unique_lock<std::mutex> lock(mStorageManagerImplLock);

                /* Check if the storage mount directory exists or can be created */
                if (mkdir(mBaseStoragePath.c_str(), STORAGE_DIR_PERMISSION) != 0)
                {
                    /* Check if the error is not directory already exists */
                    if (errno != EEXIST)
                    {
                        errorReason = "Failed to create base storage directory: " + mBaseStoragePath;
                        LOGERR("Error creating base storage directory %s", mBaseStoragePath.c_str());
                        goto ret_fail;
                    }
                }

                /* Check if the appId storageInfo already exists or can be created */
                if (retrieveAppStorageInfoByAppID(appId, storageInfo))
                {
                    errorReason = "";
                    if (storageInfo.quotaKB == size)
                    {
                        /* App storage already exist, no need of map entry update */
                        path = storageInfo.path;
                        status = Core::ERROR_NONE;
                        goto ret_success;
                    }
                    else
                    {
                        /* App storage quote size is different now, remove the existing map entry and new entry can be created */
                        if (!removeAppStorageInfoByAppID(appId))
                        {
                            LOGERR("Failed to remove storage appId: %s", appId.c_str());
                            errorReason = "Failed to remove storage for appId: " + appId;
                            path = "";
                            goto ret_fail;
                        }
                    }
                }

                if (!hasEnoughStorageFreeSpace(mBaseStoragePath, size))
                {
                    LOGERR("Insufficient storage space for app [%s]. Requested: %u KB", appId.c_str(), size);
                    errorReason = "Insufficient storage space";
                    goto ret_fail;
                }
                else
                {
                    /* Check if the app storage directory exists or can be created */
                    appDir = mBaseStoragePath + "/" + appId;

                    if (mkdir(appDir.c_str(), STORAGE_DIR_PERMISSION) != 0)
                    {
                        /* Check if the error is not directory already exists */
                        if (errno != EEXIST)
                        {
                            errorReason = "Failed to create app storage directory: " + appDir;
                            LOGERR("Error creating app storage directory %s", appDir.c_str());
                            goto ret_fail;
                        }
                    }

                    /* Create app storage info and add it to the map */
                    storageInfo.path    = appDir;
                    storageInfo.uid     = -1; /* Will be updated by GetStorage API */
                    storageInfo.gid     = -1; /* Will be updated by GetStorage API */
                    storageInfo.quotaKB = size;
                    storageInfo.usedKB  = 0; /* Initially, no space is used */
                    if (createAppStorageInfoByAppID(appId, storageInfo))
                    {
                        LOGINFO("Created storage at appDir: %s", appDir.c_str());
                        path = std::move(appDir);
                    }
                    else
                    {
                        LOGERR("Failed to create storage for appId: %s", appDir.c_str());
                        errorReason = "Failed to create storage for appId: " + appId;
                        path = "";
                    }

                    //set Quota size in to the persistent store
                    status = appQuotaSizeProperty(SET, appId, &storageInfo.quotaKB);
                    if (Core::ERROR_NONE != status)
                    {
                        LOGERR("appQuotaSizeProperty Failed to SET for appId[%s]!!!", appId.c_str());
                    }
                    else
                    {
                        LOGINFO("appQuotaSizeProperty: SET success for appId[%s]\n", appId.c_str());
                    }

                }
            }

            ret_success:
            ret_fail:
            return status;
        }

        /**
        * @brief : Returns the storage information and location for a given app id
        */
        Core::hresult RequestHandler::GetStorage(const string& appId, const int32_t& userId, const int32_t& groupId, string& path, uint32_t& size, uint32_t& used)
        {
            Core::hresult status = Core::ERROR_GENERAL;
            StorageAppInfo storageInfo;

            std::unique_lock<std::mutex> lock(mStorageManagerImplLock);
            LOGINFO("Entered GetStorage Implementation");
            if(retrieveAppStorageInfoByAppID(appId, storageInfo))
            {
                path     = storageInfo.path;
                size     = storageInfo.quotaKB;
                used     = storageInfo.usedKB;
                if (storageInfo.usedKB > storageInfo.quotaKB)
                {
                    LOGWARN("Application storage usage exceeded allocation: %s (Allocated: %u KB, Used: %u KB)",storageInfo.path.c_str(), storageInfo.quotaKB, storageInfo.usedKB);
                }
                LOGINFO("GetStorage Information path = %s, userId = %d, groupId = %d, size = %u, used = %u ",path.c_str(), userId, groupId, size, used);

                status = Core::ERROR_NONE;
                if(storageInfo.uid != userId || storageInfo.gid != groupId)
                {
                    LOGINFO("Stored uid = %d gid = %d are different from param",storageInfo.uid, storageInfo.gid);
                    if (chown(path.c_str(), userId, groupId) != 0)
                    {
                        LOGERR("Failed to set ownership: %s", strerror(errno));
                        status = Core::ERROR_GENERAL;
                    }
                    else
                    {
                        storageInfo.uid = userId;
                        storageInfo.gid = groupId;
                        /* update uid and gid */
                        if (!createAppStorageInfoByAppID(appId, storageInfo))
                        {
                            LOGERR("Failed to update uid and gid of App storageInfo");
                            status = Core::ERROR_GENERAL;
                        }
                    }
                }
            }
            else
            {
                LOGERR("Failed to get the Information of the appId: %s", appId.c_str());
            }
            return status;
        }

        /**
        * @brief : Deletes storage for a given app id
        */
        Core::hresult RequestHandler::DeleteStorage(const string& appId, string& errorReason)
        {
            Core::hresult status = Core::ERROR_GENERAL;
            LOGINFO("Entered DeleteStorage Implementation");

            if (appId.empty())
            {
                errorReason = "AppId is empty";
                LOGERR("AppId is empty");
            }
            else
            {
                /* Lock the map to ensure thread safety */
                std::unique_lock<std::mutex> lock(mStorageManagerImplLock);
                /* Check if the App Folder exists in the map */
                auto it = mStorageAppInfo.find(appId);
                if (it == mStorageAppInfo.end())
                {
                    errorReason = "AppId not found in storage info";
                    LOGERR("AppId not found in storage info");
                }
                else
                {
                    const std::string path = it->second.path;
                    LOGINFO("App Folder exists, attempting to delete: %s", path.c_str());
                    if (deleteDirectoryEntries(appId, errorReason) == Core::ERROR_NONE)
                    {
                        if (0 == rmdir(path.c_str()))
                        {
                            LOGINFO("App Folder removed successfully");
                            status = Core::ERROR_NONE;
                            LOGINFO("Erasing the App Folder from the database");
                            mStorageAppInfo.erase(it);
                            appQuotaSizeProperty(DELETE, appId, nullptr); //Remove the persistent store entry.
                        }
                        else
                        {
                            errorReason = "Error deleting the empty App Folder: " + std::string(strerror(errno));
                            LOGERR("Error deleting the empty App Folder:  reason=%s, Path: %s", strerror(errno), path.c_str());
                        }
                    }
                }
            }
            return status;
        }

        /**
        * @brief : Callback function used by nftw to delete files and directories
        *
        * Deletes files and symbolic links using unlink
        * and directories using rmdir
        */
        int deleteCallback(const char *path, const struct stat *sb, int typeflag, struct FTW *ftwbuf)
        {
            int status = 0;

            if (ftwbuf->level == 0)
            {
                LOGINFO("Skip deleting root directory: %s", path);
            }
            else
            {
                switch (typeflag)
                {
                    case FTW_DP:
                        if (rmdir(path) != 0)
                        {
                            LOGERR("Failed to delete directory: %s, Error: %s", path, strerror(errno));
                            status = -1;
                        }
                        break;

                        case FTW_F:
                        case FTW_SL:
                        if (unlink(path) != 0)
                        {
                            LOGERR("Failed to delete file: %s, Error: %s", path, strerror(errno));
                            status = -1;
                        }
                        break;

                        default:
                            LOGERR("Unhandled type: %d for path: %s", typeflag, path);
                            status = -1;
                        break;
                }
            }
            return status;
        }

        /**
        * @brief : Deletes all entries within the specified directory
        */
        Core::hresult RequestHandler::deleteDirectoryEntries(const string& appId, string& errorReason)
        {
            Core::hresult status = Core::ERROR_GENERAL;
            std::unique_lock<std::mutex> appLock;

            if (!lockAppStorageInfo(appId, appLock))
            {
                errorReason = "Storage not found for appId: " + appId;
            }
            else
            {
                auto it = mStorageAppInfo.find(appId);
                if(it != mStorageAppInfo.end())
                {
                    const std::string path = it->second.path;
                    LOGINFO("Clearing App storage path: %s", path.c_str());
                    if (nftw(path.c_str(), deleteCallback, MAX_NUM_OF_FILE_DESCRIPTORS, FTW_DEPTH | FTW_PHYS) != 0)
                    {
                        LOGERR("Failed to clear App storage path: %s",path.c_str());
                        errorReason = "Failed to clear App storage path: " + path;
                    }
                    else
                    {
                        /* Successfully cleared app storage path */
                        it->second.usedKB = 0;
                        errorReason = "";
                        status = Core::ERROR_NONE;
                    }
                }
            }
            return status;
        }

        /**
        * @brief Locks the storage information for a specific application.
        *
        * This function searches for the application ID in the mStorageAppInfo map.
        * If found, it acquires a lock on the associated storage mutex to ensure
        * thread-safe access to the application's storage information.
        */
        bool RequestHandler::lockAppStorageInfo(const std::string& appId, std::unique_lock<std::mutex>& appLock)
        {
            bool status = false;

            LOGINFO("lockAppStorageInfo");

            auto it = mStorageAppInfo.find(appId);
            if (it == mStorageAppInfo.end())
            {
                LOGERR("App ID %s not found in storage info", appId.c_str());
            }
            else
            {
                appLock = std::unique_lock<std::mutex>(it->second.storageLock);
                status = true;
            }
            return status;
        }

        /**
        * @brief : Clears storage for a given app id
        */
        Core::hresult RequestHandler::Clear(const string& appId, string& errorReason)
        {
            Core::hresult status = Core::ERROR_GENERAL;
            LOGINFO("Entered Clear Implementation");
            std::unique_lock<std::mutex> lock(mStorageManagerImplLock);

            if (appId.empty())
            {
                errorReason = "Clear called with no appId";
            }
            else
            {
                status = deleteDirectoryEntries(appId, errorReason);
            }
            return status;
        }

        /**
        * @brief : Clears all app data except for the exempt app ids
        */
        Core::hresult RequestHandler::ClearAll(const string& exemptionAppIds, string& errorReason)
        {
            Core::hresult status = Core::ERROR_GENERAL;
            JsonValue parameters;
            LOGINFO("Entered ClearAll Implementation");
            std::unique_lock<std::mutex> lock(mStorageManagerImplLock);

            parameters.FromString(exemptionAppIds);
            std::list<std::string> exemptedIdsStrList;
            if (parameters.Content()== JsonValue::type::ARRAY)
            {
                const JsonArray exemptedIdsjson = parameters.Array();
                for (unsigned int i = 0; i < exemptedIdsjson.Length(); i++)
                {
                    exemptedIdsStrList.push_back(exemptedIdsjson[i].String());
                }
            }
            else
            {
                LOGERR("invalid exemptionappids format. input is not in array format");
            }
            DIR* dir = opendir(mBaseStoragePath.c_str());
            if (!dir)
            {
                errorReason = "Failed to open storage directory: " + mBaseStoragePath;
            }
            else
            {
                bool deletionFailed = false;
                struct dirent* entry;
                while ((entry = readdir(dir)) != nullptr)
                {
                    if (entry->d_type != DT_DIR || strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
                    {
                        continue;
                    }

                    string appDirName = entry->d_name;
                    if (std::find(exemptedIdsStrList.begin(), exemptedIdsStrList.end(), appDirName) != exemptedIdsStrList.end())
                    {
                        LOGINFO("Skipping exempted app directory: %s", appDirName.c_str());
                        continue;
                    }

                    Core::hresult deleteStatus = deleteDirectoryEntries(appDirName, errorReason);
                    if (deleteStatus != Core::ERROR_NONE)
                    {
                        LOGERR("Error deleting directory entries for: %s", appDirName.c_str());
                        deletionFailed = true;
                    }
                }
                closedir(dir);

                if (!deletionFailed)
                {
                    status = Core::ERROR_NONE;
                }
            }
            return status;
        }
        /**
        * @brief : Set the BaseStoragePath
        *
        * @param[in] path                   : Base storage path
        */
        void RequestHandler::SetBaseStoragePath(const std::string& path)
        {
            mBaseStoragePath = path;
            LOGINFO("Base Storage Path Set: %s", mBaseStoragePath.c_str());
        }
    }
}
