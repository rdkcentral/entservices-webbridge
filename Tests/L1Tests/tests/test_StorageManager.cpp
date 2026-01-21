#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <string>
#include <cstring>
#include "StorageManager.h"
#include "StorageManagerImplementation.h"
#include "ServiceMock.h"
#include "Store2Mock.h"
#include "WrapsMock.h"
#include "ThunderPortability.h"
#include "COMLinkMock.h"
#include "RequestHandler.h"

extern "C" DIR* __real_opendir(const char* pathname);

using ::testing::NiceMock;
using ::testing::Return;
using namespace WPEFramework;

#define TEST_LOG(x, ...) fprintf(stderr, "\033[1;32m[%s:%d](%s)<PID:%d><TID:%d>" x "\n\033[0m", __FILE__, __LINE__, __FUNCTION__, getpid(), gettid(), ##__VA_ARGS__); fflush(stderr);

class StorageManagerTest : public ::testing::Test {
    protected:
        //JSONRPC
        Core::ProxyType<Plugin::StorageManager> plugin; // create a proxy object
        Core::JSONRPC::Handler& handler;
        Core::JSONRPC::Context connection; // create a JSONRPC context
        string response; // create a string to hold the response
        Exchange::IConfiguration* storageManagerConfigure; // create a pointer to IConfiguration
        //comrpc 
        Exchange::IStorageManager* interface; // create a pointer to IStorageManager
        NiceMock<ServiceMock> service; // an instance of mock service object
        Core::ProxyType<Plugin::StorageManagerImplementation> StorageManagerImplementation; // declare an proxy object
        ServiceMock  *p_serviceMock  = nullptr;
        WrapsImplMock *p_wrapsImplMock   = nullptr;

        Store2Mock* mStore2Mock = nullptr;

        StorageManagerTest():
        plugin(Core::ProxyType<Plugin::StorageManager>::Create()),
        handler(*plugin),
        connection(0,1,"")
        {
            StorageManagerImplementation = Core::ProxyType<Plugin::StorageManagerImplementation>::Create();
            mStore2Mock = new NiceMock<Store2Mock>;

            p_wrapsImplMock  = new NiceMock <WrapsImplMock>;
            Wraps::setImpl(p_wrapsImplMock);
            static struct dirent mockDirent;
            std::memset(&mockDirent, 0, sizeof(mockDirent));
            std::strncpy(mockDirent.d_name, "mockApp", sizeof(mockDirent.d_name) - 1);
            mockDirent.d_type = DT_DIR;            
            ON_CALL(*mStore2Mock, GetValue(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
                .WillByDefault(::testing::Invoke(
                [](Exchange::IStore2::ScopeType scope, const std::string& appId, const std::string& key, std::string& value, uint32_t& ttl) -> uint32_t {
                    if (key == "quotaSize") {
                        value = "1024"; // Simulate a valid numeric quota
                    } else {
                        value = "mockValue"; // Default value for other keys
                    }

                    ttl = 0;
                    return Core::ERROR_NONE;
            }));

            ON_CALL(*p_wrapsImplMock, opendir(::testing::_))
                .WillByDefault(::testing::Invoke([](const char* pathname) {
                    // Simulate success
                    return __real_opendir(pathname);
            }));
            ON_CALL(*p_wrapsImplMock, readdir(::testing::_))
                .WillByDefault([](DIR* dirp) -> struct dirent* {
                    static int call_count = 0;
                    static struct dirent entry;
                    if (call_count == 0) {
                        std::strncpy(entry.d_name, "mockApp", sizeof(entry.d_name) - 1);
                        entry.d_type = DT_DIR;
                        call_count++;
                        return &entry;
                    } else if (call_count == 1) {
                        std::strncpy(entry.d_name, "testApp", sizeof(entry.d_name) - 1);
                        entry.d_type = DT_DIR;
                        call_count++;
                        return &entry;
                    } else {
                        call_count = 0; // Reset for next traversal
                        return nullptr;
                    }
            });

            ON_CALL(*p_wrapsImplMock, nftw(::testing::_, ::testing::_, ::testing::_, ::testing::_))
            .WillByDefault([](const char* dirpath, int (*fn)(const char*, const struct stat*, int, struct FTW*), int nopenfd, int flags) {
                // Simulate success
                return 0;
            });

            ON_CALL(*p_wrapsImplMock, stat(::testing::_, ::testing::_))
                .WillByDefault([](const char* path, struct stat* info) {
                    // Simulate success
                    return 0;
            });

            ON_CALL(*p_wrapsImplMock, closedir(::testing::_))
                .WillByDefault([](DIR* dirp) {
                    // Simulate success
                    return 0;
            });

            EXPECT_CALL(service, QueryInterfaceByCallsign(::testing::_, ::testing::_))
            .WillRepeatedly(::testing::Invoke(
            [&](const uint32_t, const std::string& name) -> void* {
                if (name == "org.rdk.PersistentStore") {
                    return reinterpret_cast<void*>(mStore2Mock);
                }
                return nullptr;
            }));

            interface = static_cast<Exchange::IStorageManager*>(
                StorageManagerImplementation->QueryInterface(Exchange::IStorageManager::ID));

            storageManagerConfigure = static_cast<Exchange::IConfiguration*>(
            StorageManagerImplementation->QueryInterface(Exchange::IConfiguration::ID));
            StorageManagerImplementation->Configure(&service);
            plugin->Initialize(&service);
          
        }
        virtual ~StorageManagerTest() override {
            plugin->Deinitialize(&service);
            storageManagerConfigure->Release();
            Wraps::setImpl(nullptr);
            if (p_wrapsImplMock != nullptr)
            {
                delete p_wrapsImplMock;
                p_wrapsImplMock = nullptr;
            }
            if (mStore2Mock != nullptr)
            {
                delete mStore2Mock;
                mStore2Mock = nullptr;
            }

        }
        virtual void SetUp()
        {
            ASSERT_TRUE(interface != nullptr);
        }
    
        virtual void TearDown()
        {
            ASSERT_TRUE(interface != nullptr);
        }
    };


/*
    Test for  CreateStorage with empty appId
    This test checks the failure case when an empty appId is provided.
    It expects the CreateStorage method to return an error code and set the error reason accordingly.
    The test verifies that the error reason matches the expected message "appId cannot be empty".
    It also logs the error reason for debugging purposes.
*/
TEST_F(StorageManagerTest, CreateStorage_Failure){
    std::string appId = "";
    uint32_t size = 1024;
    std::string path = " ";
    std::string errorReason = "";
    EXPECT_EQ(Core::ERROR_GENERAL, interface->CreateStorage(appId, size, path, errorReason));
    EXPECT_STREQ("appId cannot be empty", errorReason.c_str());
    TEST_LOG("CreateStorage_Failure errorReason = %s",errorReason.c_str());

}

/*
    CreateStorage_Success test checks the failure of creation of storage for a given appId due to insufficient storage space.
    Creates a mock environment where the necessary functions like mkdir, access, nftw, are set up to simulate a successful resutls.
    Creates a mock environment where the statvfs function is set up to simulate a unsuccessful result with insufficient storage space.
    It verifies that the CreateStorage method returns a failure code and errorReason that Insufficient storage space.
    The test also logs the success message for debugging purposes.
*/
TEST_F(StorageManagerTest, CreateStorageSizeExceeded_Failure){
    std::string appId = "testApp";
    uint32_t size = 1000000;
    std::string path = " ";
    std::string errorReason = "";
    // mock the mkdir function to always return success
    EXPECT_CALL(*p_wrapsImplMock, mkdir(::testing::_, ::testing::_))
        .WillRepeatedly([](const char* path, mode_t mode) {
            return 0;
    });
    ON_CALL(*p_wrapsImplMock, access(::testing::_, ::testing::_))
        .WillByDefault([](const char* path, int mode) {
            // Simulate file exists
            return 0;
    });
    EXPECT_CALL(*p_wrapsImplMock, nftw(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly([](const char* dirpath, int (*fn)(const char*, const struct stat*, int, struct FTW*), int nopenfd, int flags) {
            // Simulate success
            return 0;
    });

    EXPECT_CALL(*p_wrapsImplMock, statvfs(::testing::_, ::testing::_))
    .WillRepeatedly([](const char* path, struct statvfs* buf) {
        // Simulate failure
        buf->f_bsize = 4096; // Block size
        buf->f_frsize = 4096; // Fragment size
        buf->f_blocks = 100000; // Total blocks
        buf->f_bfree = 0; // Free blocks
        buf->f_bavail = 0; // Available blocks
        return 0;
    });
    
    ON_CALL(*mStore2Mock, SetValue(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillByDefault(::testing::Invoke([](Exchange::IStore2::ScopeType scope,
                                        const std::string& appId,
                                        const std::string& key,
                                        const std::string& value,
                                        const uint32_t ttl) -> uint32_t {
        return Core::ERROR_NONE;
    }));
    EXPECT_EQ(Core::ERROR_GENERAL, interface->CreateStorage(appId, size, path, errorReason));
    EXPECT_STREQ("Insufficient storage space", errorReason.c_str());
    TEST_LOG("CreateStorageSizeExceeded_Failure errorReason = %s",errorReason.c_str());
}

/*
    CreateStorage_Success test checks the failure of creation of storage for a given appId due to mkdir failure.
    Creates a mock environment where the necessary functions like access, nftw, are set up to simulate a successful resutls.
    Creates a mock environment where the mkdir function is set up to simulate a failure with ENOTDIR error.
    It verifies that the CreateStorage method returns a failure code and errorReason that Failed to create base storage directory: /opt/persistent/storageManager.
    The test also logs the message for debugging purposes.
*/
TEST_F(StorageManagerTest, CreateStoragemkdirFail_Failure){
    
    std::string appId = "testApp";
    uint32_t size = 1024;
    std::string path = " ";
    std::string errorReason = "";

    EXPECT_CALL(*p_wrapsImplMock, mkdir(::testing::_, ::testing::_))
        .WillOnce([](const char* path, mode_t mode) {
            errno = ENOTDIR;  // error that should trigger failure
            return -1;
        });

    EXPECT_EQ(Core::ERROR_GENERAL, interface->CreateStorage(appId, size, path, errorReason));
    EXPECT_STREQ("Failed to create base storage directory: /opt/persistent/storageManager", errorReason.c_str());
    TEST_LOG("CreateStoragemkdirFail_Failure errorReason = %s",errorReason.c_str());

}

/*
    CreateStorage_PathDoesNotExists_Success test checks the successful creation of storage for a given appId when the path does not exist.
    Creates a mock environment where the necessary functions like mkdir, access, nftw, statvfs, and stat are set up to simulate a successful resutls.
    It verifies that the CreateStorage method returns a success code and the errorReason is empty.
    The test also logs the success message for debugging purposes.
*/
TEST_F(StorageManagerTest, CreateStorage_PathDoesNotExists_Success){

    std::string appId = "testApp";
    uint32_t size = 1024;
    std::string path = " ";
    std::string errorReason = "";

    EXPECT_CALL(*p_wrapsImplMock, mkdir(::testing::_, ::testing::_))
        .WillRepeatedly([](const char* path, mode_t mode) {
           errno = EEXIST;  // Directory already exists
            return -1;
    });
    
    ON_CALL(*p_wrapsImplMock, access(::testing::_, ::testing::_))
    .WillByDefault([](const char* pathname, int mode) {
        // Simulate file exists
        return 0;
    });
    
    ON_CALL(*p_wrapsImplMock, nftw(::testing::_, ::testing::_, ::testing::_, ::testing::_))
    .WillByDefault([](const char* dirpath, int (*fn)(const char*, const struct stat*, int, struct FTW*), int nopenfd, int flags) {
        // Simulate success
        return 0;
    });

    EXPECT_CALL(*p_wrapsImplMock, statvfs(::testing::_, ::testing::_))
        .WillRepeatedly([](const char* path, struct statvfs* buf) {
            // Simulate success
            buf->f_bsize = 4096; // Block size
            buf->f_frsize = 4096; // Fragment size
            buf->f_blocks = 100000; // Total blocks
            buf->f_bfree = 500000; // Free blocks
            buf->f_bavail = 500000; // Available blocks
            return 0;
    });

    ON_CALL(*p_wrapsImplMock, stat(::testing::_, ::testing::_))
        .WillByDefault([](const char* path, struct stat* info) {
            // Simulate success
            return 0;
    });
    
    ON_CALL(*mStore2Mock, SetValue(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillByDefault(::testing::Invoke([](Exchange::IStore2::ScopeType scope,
                                        const std::string& appId,
                                        const std::string& key,
                                        const std::string& value,
                                        const uint32_t ttl) -> uint32_t {
        // Simulate success
        return Core::ERROR_NONE;
    }));

    EXPECT_EQ(Core::ERROR_NONE, interface->CreateStorage(appId, size, path, errorReason));
    EXPECT_STREQ("", errorReason.c_str());

}

/*
    CreateStorage_Success test checks the successful creation of storage for a given appId.
    Creates a mock environment where the necessary functions like mkdir, access, nftw, statvfs, and stat are set up to simulate a successful resutls.
    It verifies that the CreateStorage method returns a success code and the errorReason is empty.
    The test also logs the success message for debugging purposes.
*/
TEST_F(StorageManagerTest, CreateStorage_Success){

    uint32_t size = 1024;
    std::string path = " ";
    std::string errorReason = ""; 
    std::string appId = "testApp";

    // mock the mkdir function to always return success
    EXPECT_CALL(*p_wrapsImplMock, mkdir(::testing::_, ::testing::_))
        .WillRepeatedly([](const char* path, mode_t mode) {
            return 0;
    });

    ON_CALL(*p_wrapsImplMock, access(::testing::_, ::testing::_))
        .WillByDefault([](const char* path, int mode) {
            // Simulate file exists
            return 0;
    });
        
    ON_CALL(*p_wrapsImplMock, nftw(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillByDefault([](const char* dirpath, int (*fn)(const char*, const struct stat*, int, struct FTW*), int nopenfd, int flags) {
            // Simulate success
            return 0;
    });

    EXPECT_CALL(*p_wrapsImplMock, statvfs(::testing::_, ::testing::_))
        .WillRepeatedly([](const char* path, struct statvfs* buf) {
            // Simulate success
            buf->f_bsize = 4096; // Block size
            buf->f_frsize = 4096; // Fragment size
            buf->f_blocks = 100000; // Total blocks
            buf->f_bfree = 500000; // Free blocks
            buf->f_bavail = 500000; // Available blocks
            return 0;
    });

    ON_CALL(*p_wrapsImplMock, stat(::testing::_, ::testing::_))
        .WillByDefault([](const char* path, struct stat* info) {
            // Simulate success
            return 0;
    });
    
    ON_CALL(*mStore2Mock, SetValue(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillByDefault(::testing::Invoke([](Exchange::IStore2::ScopeType scope,
                                        const std::string& appId,
                                        const std::string& key,
                                        const std::string& value,
                                        const uint32_t ttl) -> uint32_t {
        // Simulate success
        return Core::ERROR_NONE;
    }));
  
    EXPECT_EQ(Core::ERROR_NONE, interface->CreateStorage(appId, size, path, errorReason));
    EXPECT_STREQ("", errorReason.c_str());
}

/*
    GetStorage_Failure test checks the failure of getting storage information for a blank appId.
    Creates a mock environment where the necessary functions like mkdir, access, nftw, statvfs, and chown are set up to simulate a successful resutls.
    It verifies that the GetStorage method returns a failure code when the appId is empty.
*/
TEST_F(StorageManagerTest, GetStorage_Failure){

    std::string appId = "";
    uint32_t userId = 100;
    uint32_t groupId = 101;
    std::string path = "";
    uint32_t size = 0;
    uint32_t used = 0;

    EXPECT_EQ(Core::ERROR_GENERAL, interface->GetStorage(appId, userId, groupId, path, size, used));
}
/*
    GetStorage_chownFailure test checks the failure of getting storage information when the chown operation fails.
    Creates a mock environment where the necessary functions like mkdir, access, nftw, statvfs, chown are set up to simulate a successful resutls.
    chown is set up to simulate a faiure result.
    It verifies that the GetStorage method returns a failure code when the chown operation fails.
*/
TEST_F(StorageManagerTest, GetStorage_chownFailure){

    std::string appId = "testApp";
    uint32_t size = 1024;
    std::string errorReason = "";
    uint32_t userId = 100;
    uint32_t groupId = 101;
    std::string path = "";
    uint32_t used = 0;

    // mock the mkdir function to always return success
    EXPECT_CALL(*p_wrapsImplMock, mkdir(::testing::_, ::testing::_))
        .WillRepeatedly([](const char* path, mode_t mode) {
            return 0;
    });
    ON_CALL(*p_wrapsImplMock, access(::testing::_, ::testing::_))
        .WillByDefault([](const char* path, int mode) {
            // Simulate file exists
            return 0;
    });
    ON_CALL(*p_wrapsImplMock, nftw(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillByDefault([](const char* dirpath, int (*fn)(const char*, const struct stat*, int, struct FTW*), int nopenfd, int flags) {
            // Simulate success
            return 0;
    });

    EXPECT_CALL(*p_wrapsImplMock, statvfs(::testing::_, ::testing::_))
        .WillRepeatedly([](const char* path, struct statvfs* buf) {
            // Simulate success
            buf->f_bsize = 4096; // Block size
            buf->f_frsize = 4096; // Fragment size
            buf->f_blocks = 100000; // Total blocks
            buf->f_bfree = 50000; // Free blocks
            buf->f_bavail = 50000; // Available blocks
            return 0;
    });

    EXPECT_CALL(*p_wrapsImplMock, chown(::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly([](const char* path, uid_t owner, gid_t group) {
            // Simulate Failure
            return -1;
    });

    ON_CALL(*mStore2Mock, SetValue(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillByDefault(::testing::Invoke([](Exchange::IStore2::ScopeType scope,
                                    const std::string& appId,
                                    const std::string& key,
                                    const std::string& value,
                                    const uint32_t ttl) -> uint32_t {
        // Simulate success
        return Core::ERROR_NONE;
    }));

    ON_CALL(*mStore2Mock, GetValue(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
    .WillByDefault(::testing::Invoke(
        [](Exchange::IStore2::ScopeType scope, const std::string& appId, const std::string& key, std::string& value, uint32_t& ttl) -> uint32_t {
            if (key == "quotaSize") {
                value = "1024"; // Simulate a valid numeric quota
            } else {
                value = "mockValue"; // Default value for other keys
            }
            ttl = 0;
            return Core::ERROR_NONE;
    }));

    EXPECT_EQ(Core::ERROR_NONE, interface->CreateStorage(appId, size, path, errorReason));
    EXPECT_EQ(Core::ERROR_GENERAL, interface->GetStorage(appId, userId, groupId, path, size, used));
}

/*
    GetStorage_Success test checks the successful retrieval of storage information for a given appId.
    Creates a mock environment where the necessary functions like mkdir, access, nftw, statvfs, and chown are set up to simulate a successful resutls.
    It verifies that the GetStorage method returns a success code and the size and used values are set correctly.
    The test also checks that the path is set to the expected value.
*/
TEST_F(StorageManagerTest, GetStorage_Success){

    std::string appId = "testApp";
    uint32_t size = 1024;
    std::string errorReason = "";
    uint32_t userId = 100;
    uint32_t groupId = 101;
    std::string path = "";
    uint32_t used = 0;

    // mock the mkdir function to always return success
    EXPECT_CALL(*p_wrapsImplMock, mkdir(::testing::_, ::testing::_))
        .WillRepeatedly([](const char* path, mode_t mode) {
            return 0;
    });
    ON_CALL(*p_wrapsImplMock, access(::testing::_, ::testing::_))
        .WillByDefault([](const char* path, int mode) {
            // Simulate file exists
            return 0;
    });
    ON_CALL(*p_wrapsImplMock, nftw(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillByDefault([](const char* dirpath, int (*fn)(const char*, const struct stat*, int, struct FTW*), int nopenfd, int flags) {
            // Simulate success
            return 0;
    });

    EXPECT_CALL(*p_wrapsImplMock, statvfs(::testing::_, ::testing::_))
        .WillRepeatedly([](const char* path, struct statvfs* buf) {
            // Simulate success
            buf->f_bsize = 4096; // Block size
            buf->f_frsize = 4096; // Fragment size
            buf->f_blocks = 100000; // Total blocks
            buf->f_bfree = 50000; // Free blocks
            buf->f_bavail = 50000; // Available blocks
            return 0;
    });
    
    EXPECT_CALL(*p_wrapsImplMock, chown(::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly([](const char* path, uid_t owner, gid_t group) {
            // Simulate success
            return 0;
    });
    
    ON_CALL(*mStore2Mock, SetValue(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillByDefault(::testing::Invoke([](Exchange::IStore2::ScopeType scope,
                                    const std::string& appId,
                                    const std::string& key,
                                    const std::string& value,
                                    const uint32_t ttl) -> uint32_t {
        // Simulate success
        return Core::ERROR_NONE;
    }));
    
    ON_CALL(*mStore2Mock, GetValue(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
    .WillByDefault(::testing::Invoke(
        [](Exchange::IStore2::ScopeType scope, const std::string& appId, const std::string& key, std::string& value, uint32_t& ttl) -> uint32_t {
            if (key == "quotaSize") {
                value = "1024"; // Simulate a valid numeric quota
            } else {
                value = "mockValue"; // Default value for other keys
            }
            ttl = 0;
            return Core::ERROR_NONE;
    }));
    
    EXPECT_EQ(Core::ERROR_NONE, interface->CreateStorage(appId, size, path, errorReason));
    EXPECT_EQ(Core::ERROR_NONE, interface->GetStorage(appId, userId, groupId, path, size, used));
    EXPECT_EQ(1024, size);
    EXPECT_EQ(0, used);
    EXPECT_EQ(path, "/opt/persistent/storageManager/testApp");
}

/*
    DeleteStorage_Failure test checks the failure of deleting storage for a blank appId.
    It verifies that the DeleteStorage method returns a failure code when the appId is empty and sets the errorReason accordingly.
    The test also logs the error reason for debugging purposes.
*/
TEST_F(StorageManagerTest, DeleteStorage_Failure){
    std::string appId = "";
    std::string errorReason = "";

    EXPECT_EQ(Core::ERROR_GENERAL, interface->DeleteStorage(appId, errorReason));
    EXPECT_STREQ("AppId is empty", errorReason.c_str());
    TEST_LOG("DeleteStorage_Failure errorReason = %s",errorReason.c_str());
}

/*
    DeleteStorage_rmdirFilure test checks the failure of deleting storage for a given appId when the rmdir function fails.
    Creates a mock environment where the necessary functions like mkdir, access, nftw, statvfs, are rmdir are set up to simulate a successful results.
    rmdir is set up to simulate a failure result.
    It verifies that the DeleteStorage method returns a failure code and errorReason that Error deleting the empty App Folder: File exists.
*/
TEST_F(StorageManagerTest, DeleteStorage_rmdirFilure){

    std::string appId = "testApp";
    uint32_t size = 1024;
    std::string errorReason = "";
    std::string path = "";

    // mock the mkdir function to always return success
    EXPECT_CALL(*p_wrapsImplMock, mkdir(::testing::_, ::testing::_))
        .WillRepeatedly([](const char* path, mode_t mode) {
            return 0;
    });
    ON_CALL(*p_wrapsImplMock, access(::testing::_, ::testing::_))
        .WillByDefault([](const char* path, int mode) {
            // Simulate file exists
            return 0;
    });
    ON_CALL(*p_wrapsImplMock, nftw(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillByDefault([](const char* dirpath, int (*fn)(const char*, const struct stat*, int, struct FTW*), int nopenfd, int flags) {
            // Simulate success
            return 0;
    });

    EXPECT_CALL(*p_wrapsImplMock, statvfs(::testing::_, ::testing::_))
        .WillRepeatedly([](const char* path, struct statvfs* buf) {
            // Simulate success
            buf->f_bsize = 4096; // Block size
            buf->f_frsize = 4096; // Fragment size
            buf->f_blocks = 100000; // Total blocks
            buf->f_bfree = 50000; // Free blocks
            buf->f_bavail = 50000; // Available blocks
            return 0;
    });

    EXPECT_CALL(*p_wrapsImplMock, rmdir(::testing::_))
        .WillRepeatedly([](const char* pathname) {
            // Simulate failure
            return -1;
    });

    ON_CALL(*mStore2Mock, DeleteKey(::testing::_, ::testing::_, ::testing::_))
        .WillByDefault(::testing::Invoke(
        [](Exchange::IStore2::ScopeType scope, const std::string& appId, const std::string& key) -> uint32_t {
            // Simulate success
            return Core::ERROR_NONE;
    }));

    EXPECT_EQ(Core::ERROR_NONE, interface->CreateStorage(appId, size, path, errorReason));
    EXPECT_EQ(Core::ERROR_GENERAL, interface->DeleteStorage(appId, errorReason));
    EXPECT_STREQ("Error deleting the empty App Folder: File exists", errorReason.c_str());
}

/*
    DeleteStorage_Success test checks the successful deletion of storage for a given appId.
    Creates a mock environment where the necessary functions like mkdir, access, nftw, statvfs, and rmdir are set up to simulate a successful resutls.
    It verifies that the DeleteStorage method returns a success code and the errorReason is empty.
*/
TEST_F(StorageManagerTest, DeleteStorage_Success){

    std::string appId = "testApp";
    uint32_t size = 1024;
    std::string errorReason = "";
    std::string path = "";

    // mock the mkdir function to always return success
    EXPECT_CALL(*p_wrapsImplMock, mkdir(::testing::_, ::testing::_))
        .WillRepeatedly([](const char* path, mode_t mode) {
            return 0;
    });
    ON_CALL(*p_wrapsImplMock, access(::testing::_, ::testing::_))
        .WillByDefault([](const char* path, int mode) {
            // Simulate file exists
            return 0;
    });
    ON_CALL(*p_wrapsImplMock, nftw(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillByDefault([](const char* dirpath, int (*fn)(const char*, const struct stat*, int, struct FTW*), int nopenfd, int flags) {
            // Simulate success
            return 0;
    });

    EXPECT_CALL(*p_wrapsImplMock, statvfs(::testing::_, ::testing::_))
        .WillRepeatedly([](const char* path, struct statvfs* buf) {
            // Simulate success
            buf->f_bsize = 4096; // Block size
            buf->f_frsize = 4096; // Fragment size
            buf->f_blocks = 100000; // Total blocks
            buf->f_bfree = 50000; // Free blocks
            buf->f_bavail = 50000; // Available blocks
            return 0;
    });
    
    EXPECT_CALL(*p_wrapsImplMock, rmdir(::testing::_))
        .WillRepeatedly([](const char* pathname) {
            // Simulate success
            return 0;
    });

    ON_CALL(*mStore2Mock, DeleteKey(::testing::_, ::testing::_, ::testing::_))
        .WillByDefault(::testing::Invoke(
        [](Exchange::IStore2::ScopeType scope, const std::string& appId, const std::string& key) -> uint32_t {
            // Simulate success
            return Core::ERROR_NONE;
    }));
    
    EXPECT_EQ(Core::ERROR_NONE, interface->CreateStorage(appId, size, path, errorReason));
    EXPECT_EQ(Core::ERROR_NONE, interface->DeleteStorage(appId, errorReason));
    EXPECT_STREQ("", errorReason.c_str());
}


/*
    test_clear_failure_json checks the failure of the clear method when an empty appId is provided.
    It verifies that the clear method returns a failure code and sets the errorReason accordingly.
    The test also logs the error reason for debugging purposes.
*/
TEST_F(StorageManagerTest, test_clear_failure_json){
    std::string appId = "";
    std::string errorReason = "";

    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("clear"), _T("{\"appId\":\"\"}"), response));
}

/*
    test_clear_nftwFailure_json checks the failure of the clear method when the nftw function fails.
    It creates a mock environment where the necessary functions like mkdir, access, statvfs, and SetValue are set up to simulate a successful result.
    nftw is set up to simulate a failure.
    The nftw function is set up to simulate a failure.
    It verifies that the clear method returns a failure code and sets the errorReason accordingly.
*/
TEST_F(StorageManagerTest, test_clear_nftwFailure_json){

    std::string path = "";
    std::string errorReason = "";
    // mock the mkdir function to always return success
    EXPECT_CALL(*p_wrapsImplMock, mkdir(::testing::_, ::testing::_))
        .WillRepeatedly([](const char* path, mode_t mode) {
            return 0;
    });
    ON_CALL(*p_wrapsImplMock, access(::testing::_, ::testing::_))
        .WillByDefault([](const char* path, int mode) {
            // Simulate file exists
            return 0;
    });
    ON_CALL(*p_wrapsImplMock, nftw(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillByDefault([](const char* dirpath, int (*fn)(const char*, const struct stat*, int, struct FTW*), int nopenfd, int flags) {
            // Simulate failure
            return -1;
    });

    EXPECT_CALL(*p_wrapsImplMock, statvfs(::testing::_, ::testing::_))
        .WillRepeatedly([](const char* path, struct statvfs* buf) {
            // Simulate success
            buf->f_bsize = 4096; // Block size
            buf->f_frsize = 4096; // Fragment size
            buf->f_blocks = 100000; // Total blocks
            buf->f_bfree = 50000; // Free blocks
            buf->f_bavail = 50000; // Available blocks
            return 0;
    });

    ON_CALL(*mStore2Mock, SetValue(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillByDefault(::testing::Invoke([](Exchange::IStore2::ScopeType scope,
                                        const std::string& appId,
                                        const std::string& key,
                                        const std::string& value,
                                        const uint32_t ttl) -> uint32_t {
        // Simulate success
        return Core::ERROR_NONE;
    }));

    EXPECT_EQ(Core::ERROR_NONE, interface->CreateStorage("testApp", 1024, path, errorReason));
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("clear"), _T("{\"appId\":\"testApp\"}"), response));
}

/*
    test_clear_success_json checks the successful execution of the clear method for a given appId.
    It creates a mock environment where the necessary functions like mkdir, access, nftw, statvfs, and SetValue are set up to simulate a successful result.
    It creates a storage for the appId "testApp" with a size of 1024 bytes.
    The test then invokes the clear method with the appId "testApp" and expects a success code.
    The response is expected to be empty, indicating that the storage has been cleared successfully
*/
TEST_F(StorageManagerTest, test_clear_success_json){
    
    std::string path = "";
    std::string errorReason = "";
    // mock the mkdir function to always return success
    EXPECT_CALL(*p_wrapsImplMock, mkdir(::testing::_, ::testing::_))
        .WillRepeatedly([](const char* path, mode_t mode) {
            return 0;
    });
    ON_CALL(*p_wrapsImplMock, access(::testing::_, ::testing::_))
        .WillByDefault([](const char* path, int mode) {
            // Simulate file exists
            return 0;
    });
    ON_CALL(*p_wrapsImplMock, nftw(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillByDefault([](const char* dirpath, int (*fn)(const char*, const struct stat*, int, struct FTW*), int nopenfd, int flags) {
            // Simulate success
            return 0;
    });

    EXPECT_CALL(*p_wrapsImplMock, statvfs(::testing::_, ::testing::_))
        .WillRepeatedly([](const char* path, struct statvfs* buf) {
            // Simulate success
            buf->f_bsize = 4096; // Block size
            buf->f_frsize = 4096; // Fragment size
            buf->f_blocks = 100000; // Total blocks
            buf->f_bfree = 50000; // Free blocks
            buf->f_bavail = 50000; // Available blocks
            return 0;
    });

    ON_CALL(*mStore2Mock, SetValue(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillByDefault(::testing::Invoke([](Exchange::IStore2::ScopeType scope,
                                        const std::string& appId,
                                        const std::string& key,
                                        const std::string& value,
                                        const uint32_t ttl) -> uint32_t {
        // Simulate success
        return Core::ERROR_NONE;
    }));
 
    EXPECT_EQ(Core::ERROR_NONE, interface->CreateStorage("testApp", 1024, path, errorReason));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("clear"), _T("{\"appId\":\"testApp\"}"), response));
}

/*
    test_clearall_failure_json checks the failure of the clearAll method when an error occurs during the clearing process.
    It creates a mock environment where the necessary functions like mkdir, access, nftw, statvfs, and SetValue are set up to simulate a successful result.
    It creates storage for two appIds: "testApp" and "testexempt".
    The test then invokes the clearAll method with a JSON string containing exemptionAppIds.
    It expects the clearAll method to return a failure code and sets the errorReason accordingly.
    The test also logs the error reason for debugging purposes.
*/
TEST_F(StorageManagerTest, test_clearall_failure_json){

    std::string path = "";
    std::string errorReason = "";
    std::string wrappedJson = "{\"exemptionAppIds\": \"[\\\"testexempt\\\"]\"}";
    static int callCount = 0;

    // mock the mkdir function to always return success
    EXPECT_CALL(*p_wrapsImplMock, mkdir(::testing::_, ::testing::_))
        .WillRepeatedly([](const char* path, mode_t mode) {
            return 0;
    });
    ON_CALL(*p_wrapsImplMock, access(::testing::_, ::testing::_))
        .WillByDefault([](const char* path, int mode) {
            // Simulate file exists
            return 0;
    });
    EXPECT_CALL(*p_wrapsImplMock, nftw(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillOnce([](const char* dirpath, int (*fn)(const char*, const struct stat*, int, struct FTW*), int nopenfd, int flags) {
            // Simulate success
            return 0;
        })
        .WillOnce([](const char* dirpath, int (*fn)(const char*, const struct stat*, int, struct FTW*), int nopenfd, int flags) {
            // Simulate success
            return 0;
        })
        .WillOnce([](const char* dirpath, int (*fn)(const char*, const struct stat*, int, struct FTW*), int nopenfd, int flags) {
            // Simulate success
            return 0;
        })
        .WillOnce([](const char* dirpath, int (*fn)(const char*, const struct stat*, int, struct FTW*), int nopenfd, int flags) {
            // Simulate success
            return -1;
    });

    EXPECT_CALL(*p_wrapsImplMock, statvfs(::testing::_, ::testing::_))
        .WillRepeatedly([](const char* path, struct statvfs* buf) {
            // Simulate success
            buf->f_bsize = 4096; // Block size
            buf->f_frsize = 4096; // Fragment size
            buf->f_blocks = 100000; // Total blocks
            buf->f_bfree = 50000; // Free blocks
            buf->f_bavail = 50000; // Available blocks
            return 0;
    });

    ON_CALL(*mStore2Mock, SetValue(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillByDefault(::testing::Invoke([](Exchange::IStore2::ScopeType scope,
                                        const std::string& appId,
                                        const std::string& key,
                                        const std::string& value,
                                        const uint32_t ttl) -> uint32_t {
        // Simulate success
        return Core::ERROR_NONE;
    }));

    EXPECT_CALL(*p_wrapsImplMock, opendir(::testing::_))
        .WillOnce(::testing::Invoke([](const char* pathname) {
        // Simulate success
        return (__real_opendir(pathname));

    }));

    ON_CALL(*p_wrapsImplMock, readdir(::testing::_))
    .WillByDefault([](DIR* dirp) -> struct dirent* {
        static struct dirent entry;
        switch (callCount++) {
            case 0:
                std::strcpy(entry.d_name, "testApp");
                entry.d_type = DT_DIR;
                return &entry;
            case 1:
                std::strcpy(entry.d_name, "testexempt");
                entry.d_type = DT_DIR;
                return &entry;
            default:
                return nullptr;
        }
    });
    ON_CALL(*p_wrapsImplMock, closedir(::testing::_))
    .WillByDefault([](DIR* dirp) {
        // Simulate success
        return 0;
    });

    EXPECT_EQ(Core::ERROR_NONE, interface->CreateStorage("testApp", 1024, path, errorReason));
    EXPECT_EQ(Core::ERROR_NONE, interface->CreateStorage("testexempt", 1024, path, errorReason));
    EXPECT_EQ(Core::ERROR_GENERAL, handler.Invoke(connection, _T("clearAll"), wrappedJson, response));
}

/*
    test_clearall_without_exemption_json checks the successful execution of the clearAll method without any exemption appIds.
    It verifies that the clearAll method returns a success code and the response is empty ensuring all storage is cleared.
*/
TEST_F(StorageManagerTest, test_clearall_without_exemption_json){
    std::string exemptionAppIds = "";
    std::string errorReason = "";
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("clearAll"), _T("{}"), response));
}

/*
    test_clearall_success_json checks the successful execution of the clearAll method with exemption appIds.
    It creates a mock environment where the necessary functions like mkdir, access, nftw, statvfs, and SetValue are set up to simulate a successful result.
    It creates storage for two appIds: "testApp" and "testexempt".
    The test then invokes the clearAll method with a JSON string containing exemptionAppIds.
    It expects the clearAll method to return a success code and the response is empty indicating that the storage has been cleared successfully except for the exempted appId.
*/
TEST_F(StorageManagerTest, test_clearall_success_json){
    std::string path = "";
    std::string errorReason = ""; 
    std::string wrappedJson = "{\"exemptionAppIds\": \"{\\\"exemptionAppIds\\\": [\\\"testexempt\\\"]}\"}";
    static int callCount = 0;
    // mock the mkdir function to always return success
    EXPECT_CALL(*p_wrapsImplMock, mkdir(::testing::_, ::testing::_))
        .WillRepeatedly([](const char* path, mode_t mode) {
            return 0;
    });
    ON_CALL(*p_wrapsImplMock, access(::testing::_, ::testing::_))
        .WillByDefault([](const char* path, int mode) {
            // Simulate file exists
            return 0;
    });
    ON_CALL(*p_wrapsImplMock, nftw(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillByDefault([](const char* dirpath, int (*fn)(const char*, const struct stat*, int, struct FTW*), int nopenfd, int flags) {
            // Simulate success
            return 0;
    });
    EXPECT_CALL(*p_wrapsImplMock, statvfs(::testing::_, ::testing::_))
        .WillRepeatedly([](const char* path, struct statvfs* buf) {
            // Simulate success
            buf->f_bsize = 4096; // Block size
            buf->f_frsize = 4096; // Fragment size
            buf->f_blocks = 100000; // Total blocks
            buf->f_bfree = 50000; // Free blocks
            buf->f_bavail = 50000; // Available blocks
        return 0;
    });
    ON_CALL(*mStore2Mock, SetValue(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .WillByDefault(::testing::Invoke([](Exchange::IStore2::ScopeType scope,
                                        const std::string& appId,
                                        const std::string& key,
                                        const std::string& value,
                                        const uint32_t ttl) -> uint32_t {
        // Simulate success
        return Core::ERROR_NONE;
    }));

    ON_CALL(*p_wrapsImplMock, opendir(::testing::_))
        .WillByDefault(::testing::Invoke([](const char* pathname) {
        // Simulate success
        return (__real_opendir(pathname));

    }));

    ON_CALL(*p_wrapsImplMock, readdir(::testing::_))
    .WillByDefault([](DIR* dirp) -> struct dirent* {
        static struct dirent entry;
        switch (callCount++) {
            case 0:
                std::strcpy(entry.d_name, "testApp");
                entry.d_type = DT_DIR;
                return &entry;
            case 1:
                std::strcpy(entry.d_name, "testexempt");
                entry.d_type = DT_DIR;
                return &entry;
            default:
                return nullptr;
        }
    });

    ON_CALL(*p_wrapsImplMock, closedir(::testing::_))
    .WillByDefault([](DIR* dirp) {
        // Simulate success
        return 0;
    });

    EXPECT_EQ(Core::ERROR_NONE, interface->CreateStorage("testApp", 1024, path, errorReason));
    EXPECT_EQ(Core::ERROR_NONE, interface->CreateStorage("testexempt", 1024, path, errorReason));
    EXPECT_EQ(Core::ERROR_NONE, handler.Invoke(connection, _T("clearAll"), wrappedJson, response));
}

