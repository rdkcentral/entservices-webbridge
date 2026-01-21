#pragma once
#include "Module.h"
#include <interfaces/IStore2.h>
#include <ftw.h>
#include <mutex>

namespace WPEFramework
{
    namespace Plugin 
    {
        class RequestHandler
        {
            public:
                static RequestHandler& getInstance();
                RequestHandler(const RequestHandler&) = delete;
                RequestHandler& operator=(const RequestHandler&) = delete;
                ~RequestHandler();
 
                void SetBaseStoragePath(const std::string& path);
                typedef struct _StorageAppInfo
                {
                    std::string path;    /* Path to the application's storage */
                    int32_t uid;         /* UID of the user who owns the storage */
                    int32_t gid;         /* GID of the group who owns the storage */
                    uint32_t quotaKB;    /* Quota size in kilobytes for the storage */
                    uint32_t usedKB;     /* Used space in kilobytes for the storage */
                    std::mutex storageLock; /* Mutex for thread safety */
                } StorageAppInfo;

                typedef struct _StorageSize
                {
                    uint64_t blockSize = 0;
                    uint64_t usedBytes = 0;
                } StorageSize;

                enum StorageActionType
                {
                    UNKNOWN = 0,
                    SET,
                    GET,
                    DELETE
                };

                bool isValidAppStorageDirectory(const std::string& dirName);
                Core::hresult createPersistentStoreRemoteStoreObject();
                void releasePersistentStoreRemoteStoreObject();
                Core::hresult appQuotaSizeProperty(StorageActionType actionType, const std::string& appId, uint32_t* quotaValue);
                Core::hresult populateAppInfoCacheFromStoragePath();
                void setCurrentService(PluginHost::IShell* service);

                Core::hresult CreateStorage(const string& appId, const uint32_t& size, string& path, string& errorReason);
                Core::hresult GetStorage(const string& appId, const int32_t& userId, const int32_t& groupId, string& path, uint32_t& size, uint32_t& used);
                Core::hresult DeleteStorage(const string& appId, string& errorReason);
                Core::hresult Clear(const string& appId, string& errorReason);
                Core::hresult ClearAll(const string& exemptionAppIds, string& errorReason);

            private:

                bool createAppStorageInfoByAppID(const std::string& appId, StorageAppInfo &storageInfo);
                bool retrieveAppStorageInfoByAppID(const string &appId, StorageAppInfo &storageInfo);
                bool removeAppStorageInfoByAppID(const string &appId);
                bool hasEnoughStorageFreeSpace(const std::string& baseDir, uint32_t requiredSpaceKB);
                uint64_t getDirectorySizeInBytes(const std::string &path);
                static int getSize(const char *path, const struct stat *statPtr, int currentFlag, struct FTW *internalFtwUsage);
                Core::hresult deleteDirectoryEntries(const string& appId, string& errorReason);
                bool lockAppStorageInfo(const std::string& appId, std::unique_lock<std::mutex>& appLock);

                /*Members*/
                RequestHandler();
                PluginHost::IShell* mService;
                static RequestHandler* mInstance;
                mutable std::mutex mStorageManagerImplLock;
                static std::mutex mStorageSizeLock;
                Exchange::IStore2* mPersistentStoreRemoteStoreObject;
                std::map<std::string, StorageAppInfo> mStorageAppInfo;  /* Map storing app storage info for each appId */
                std::string mBaseStoragePath;
        };
    } /* namespace Plugin */
} /* namespace WPEFramework */
