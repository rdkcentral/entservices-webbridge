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
#include <gmock/gmock.h>
#include "L2Tests.h"
#include "L2TestsMock.h"
#include <mutex>
#include <condition_variable>
#include <fstream>
#include <interfaces/IStorageManager.h>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>


#define TEST_LOG(x, ...) fprintf(stderr, "\033[1;32m[%s:%d](%s)<PID:%d><TID:%d>" x "\n\033[0m", __FILE__, __LINE__, __FUNCTION__, getpid(), gettid(), ##__VA_ARGS__); fflush(stderr);
#define DEFAULT_APP_STORAGE_PATH    "/tmp/persistent/storageManager"

#define STORAGE_BLOCK_SIZE         4096; // Block size
#define STORAGE_FR_SIZE            4096; // Fragment size
#define STORAGE_TOTAL_BLOCKS       100000; // Total blocks
#define STORAGE_FREE_BLOCKS        500000; // Free blocks
#define STORAGE_AVAILABLE_BLOCKS   500000; // Available blocks

#define JSON_TIMEOUT   (1000)
#define STORAGEMANAGER_CALLSIGN  _T("org.rdk.StorageManager")
#define STORAGEMANAGERL2TEST_CALLSIGN _T("L2tests.1")

using ::testing::NiceMock;
using namespace WPEFramework;
using testing::StrictMock;
using ::WPEFramework::Exchange::IStorageManager;

class StorageManagerTest : public L2TestMocks {
protected:
    virtual ~StorageManagerTest() override;

public:
    StorageManagerTest();

    uint32_t CreateStorageManagerInterfaceObjectUsingComRPCConnection();
    void ReleaseStorageManagerInterfaceObjectUsingComRPCConnection();

    private:

    protected:
        /** @brief Pointer to the IShell interface */
        PluginHost::IShell *mControllerStorageManager;
       Core::ProxyType<RPC::InvokeServerType<1, 0, 4>> mEngineStorageManager;
       Core::ProxyType<RPC::CommunicatorClient> mClientStorageManager;

        /** @brief Pointer to the IStorageManager interface */
        Exchange::IStorageManager *mStorageManagerPlugin;
};

StorageManagerTest:: StorageManagerTest():L2TestMocks()
{
        Core::JSONRPC::Message message;
        string response;
        uint32_t status = Core::ERROR_GENERAL;

         /* Activate plugin in constructor */
         status = ActivateService("org.rdk.PersistentStore");
         EXPECT_EQ(Core::ERROR_NONE, status);
         status = ActivateService(STORAGEMANAGER_CALLSIGN);
         EXPECT_EQ(Core::ERROR_NONE, status);
}

/**
 * @brief Destructor for SystemServices L2 test class
 */
StorageManagerTest::~StorageManagerTest()
{
    uint32_t status = Core::ERROR_GENERAL;

    status = DeactivateService("org.rdk.PersistentStore");
    EXPECT_EQ(Core::ERROR_NONE, status);

    status = DeactivateService(STORAGEMANAGER_CALLSIGN);
    EXPECT_EQ(Core::ERROR_NONE, status);
}

uint32_t StorageManagerTest::CreateStorageManagerInterfaceObjectUsingComRPCConnection()
{
    uint32_t return_value =  Core::ERROR_GENERAL;

    TEST_LOG("Creating mEngineStorageManager");
    mEngineStorageManager = Core::ProxyType<RPC::InvokeServerType<1, 0, 4>>::Create();
    mClientStorageManager = Core::ProxyType<RPC::CommunicatorClient>::Create(Core::NodeId("/tmp/communicator"), Core::ProxyType<Core::IIPCServer>(mEngineStorageManager));

    TEST_LOG("Creating mEngineStorageManager Announcements");
#if ((THUNDER_VERSION == 2) || ((THUNDER_VERSION == 4) && (THUNDER_VERSION_MINOR == 2)))
    mEngineStorageManager->Announcements(mmClientStorageManager->Announcement());
#endif
    if (!mClientStorageManager.IsValid())
    {
        TEST_LOG("Invalid mClientStorageManager");
    }
    else
    {
        mControllerStorageManager = mClientStorageManager->Open<PluginHost::IShell>(_T(STORAGEMANAGER_CALLSIGN), ~0, 3000);
        if (mControllerStorageManager)
        {
            mStorageManagerPlugin = mControllerStorageManager->QueryInterface<Exchange::IStorageManager>();
            return_value = Core::ERROR_NONE;
        }
    }
    return return_value;
}

void StorageManagerTest::ReleaseStorageManagerInterfaceObjectUsingComRPCConnection()
{
    if (mStorageManagerPlugin)
    {
        mStorageManagerPlugin->Release();
        mStorageManagerPlugin = nullptr;
    }

    if (mControllerStorageManager)
    {
        mControllerStorageManager->Release();
        mControllerStorageManager = nullptr;
    }

    mClientStorageManager->Close(RPC::CommunicationTimeOut);
    if (mClientStorageManager.IsValid())
    {
        mClientStorageManager.Release();
    }
    mEngineStorageManager.Release();
}

bool fileExists(const std::string& filename)
{
    std::ifstream file(filename);
    bool fileAvailable = file.good();

    if (fileAvailable)
    {
        file.close();
    }

    TEST_LOG("File Name: %s, fileAvailable: %d", filename.c_str(), fileAvailable);
    return fileAvailable;
}

void createFileAndAddContent(const std::string& filePath, const std::string& fileName, const std::string& appId, const std::string& content)
{
    std::string exampleFolder = std::string(DEFAULT_APP_STORAGE_PATH) + "/" + appId + "/example";

    if (mkdir(exampleFolder.c_str(), 0755) != 0 && errno != EEXIST)
    {
        TEST_LOG("Directory creation failed");
    }
    else
    {
        std::string fileNameWithPath = exampleFolder + "/" + fileName;
        std::ofstream outFile(fileNameWithPath);
    
        if (!outFile)
        {
            TEST_LOG("File creation failed");
        }
        else
        {
            outFile << content;
            TEST_LOG("File created: %s", fileNameWithPath.c_str());

            outFile.close();
        }
    }
}

/* Test Case for CreateGetAndDeleteStorageUsingComRpcSuccess
 * Creating Storage Manager Plugin using COM-RPC connection
 * Setting Mocks for access() and statvfs() system calls to simulate filesystem availability and space
 * Calling CreateStorage() with valid appId and size, verifying success and expected storage path
 * Setting Mock for chown() to simulate ownership change on created storage path
 * Calling GetStorage() with appId and verifying retrieved storage path, size, and used space
 * Calling DeleteStorage() and verifying successful deletion of the storage
 * Verifying GetStorage() returns error after deletion, as storage no longer exists
 * Releasing the Storage Manager Interface object
 */
TEST_F(StorageManagerTest, CreateGetAndDeleteStorageUsingComRpcSuccess)
{
    uint32_t status = Core::ERROR_GENERAL;

    uint32_t size = 1024;
    std::string path = " ";
    std::string errorReason = ""; 
    std::string appId = "testApp";
    std::string actualPath = "";
    uint32_t actualSize = 0;
    uint32_t actualUsed = 0;
    std::string expectedPath = "";

    TEST_LOG("### Test CreateGetAndDeleteStorageUsingComRpcSuccess Begin ###");

    status = CreateStorageManagerInterfaceObjectUsingComRPCConnection();
    EXPECT_EQ(status,Core::ERROR_NONE);


    EXPECT_CALL(*p_wrapsImplMock, access(::testing::_, ::testing::_))
        .WillRepeatedly([](const char* pathname, int mode) {
            // Simulate able to access
            return 0;
    });

    EXPECT_CALL(*p_wrapsImplMock, statvfs(::testing::_, ::testing::_))
        .WillRepeatedly([](const char* path, struct statvfs* buf) {
            // Simulate success
            TEST_LOG("### p_wrapsImplMock statvfs ###");
            buf->f_bsize = STORAGE_BLOCK_SIZE; // Block size
            buf->f_frsize = STORAGE_FR_SIZE; // Fragment size
            buf->f_blocks = STORAGE_TOTAL_BLOCKS; // Total blocks
            buf->f_bfree = STORAGE_FREE_BLOCKS; // Free blocks
            buf->f_bavail = STORAGE_AVAILABLE_BLOCKS; // Available blocks
            return 0;
    });

    /* Verifing CreateStorage */
    EXPECT_EQ(Core::ERROR_NONE, mStorageManagerPlugin->CreateStorage(appId, size, path, errorReason));

    expectedPath = std::string(DEFAULT_APP_STORAGE_PATH) + std::string("/") + appId;

    EXPECT_CALL(*p_wrapsImplMock, chown(::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly([](const char *path, uid_t owner, gid_t group) -> int {
            // Simulate success
            TEST_LOG("### p_wrapsImplMock chown ###");
            return 0;
    });

    /* Verifing  GetStorage */
    EXPECT_EQ(Core::ERROR_NONE, mStorageManagerPlugin->GetStorage(appId, 1000, 1001, actualPath, actualSize, actualUsed));
    TEST_LOG("GetStorage_Success actualPath = %s actualSize: %u used: %u",actualPath.c_str(), actualSize, actualUsed);

    EXPECT_STREQ(expectedPath.c_str(), actualPath.c_str());
    EXPECT_EQ(size, actualSize);
    EXPECT_EQ(0U, actualUsed);

    /* Verify DeleteStorage will success */
    EXPECT_EQ(Core::ERROR_NONE, mStorageManagerPlugin->DeleteStorage(appId, errorReason));

    /* Verify GetStorage will fail, it should return Core::ERROR_GENERAL as already deleted the storage */
    EXPECT_EQ(Core::ERROR_GENERAL, mStorageManagerPlugin->GetStorage(appId, 1000, 1001, actualPath, actualSize, actualUsed));

    if (Core::ERROR_NONE == status)
    {
        ReleaseStorageManagerInterfaceObjectUsingComRPCConnection();
    }

    TEST_LOG("### Test CreateGetAndDeleteStorageUsingComRpcSuccess End ###");
}

/* Test Case for CreateStorageUsingComRpcSuccessExistingIfStorageNotAccessiable
 * Creating Storage Manager Plugin using COM-RPC connection
 * Setting Mocks for statvfs() system call to simulate available disk space
 * Calling CreateStorage() to create new storage for given appId
 * Simulating access() failure (storage path not accessible) for existing path
 * Re-calling CreateStorage() to verify it handles inaccessible existing path gracefully
 * Setting Mocks for chown() to simulate ownership setup
 * Setting Mocks for access() to simulate storage path becoming accessible
 * Calling GetStorage() and verifying correct retrieval of path, size, and usage
 * Calling DeleteStorage() and verifying successful deletion
 * Verifying GetStorage() fails after deletion (storage no longer exists)
 * Releasing the Storage Manager Interface object
 */
TEST_F(StorageManagerTest, CreateStorageUsingComRpcSuccessExistingIfStorageNotAccessiable)
{
    uint32_t status = Core::ERROR_GENERAL;

    uint32_t size = 1024;
    std::string path = " ";
    std::string errorReason = "";
    std::string appId = "testApp";
    std::string actualPath = "";
    uint32_t actualSize = 0;
    uint32_t actualUsed = 0;
    std::string expectedPath = "";

    TEST_LOG("### Test CreateStorageUsingComRpcSuccessExistingIfStorageNotAccessiable Begin ###");

    status = CreateStorageManagerInterfaceObjectUsingComRPCConnection();
    EXPECT_EQ(status,Core::ERROR_NONE);

    EXPECT_CALL(*p_wrapsImplMock, statvfs(::testing::_, ::testing::_))
        .WillRepeatedly([](const char* path, struct statvfs* buf) {
            // Simulate success
            TEST_LOG("### p_wrapsImplMock statvfs ###");
            buf->f_bsize = STORAGE_BLOCK_SIZE; // Block size
            buf->f_frsize = STORAGE_FR_SIZE; // Fragment size
            buf->f_blocks = STORAGE_TOTAL_BLOCKS; // Total blocks
            buf->f_bfree = STORAGE_FREE_BLOCKS; // Free blocks
            buf->f_bavail = STORAGE_AVAILABLE_BLOCKS; // Available blocks
            return 0;
    });

    /* Verifing CreateStorage */
    EXPECT_EQ(Core::ERROR_NONE, mStorageManagerPlugin->CreateStorage(appId, size, path, errorReason));

    EXPECT_CALL(*p_wrapsImplMock, access(::testing::_, ::testing::_))
        .WillOnce([](const char* pathname, int mode) {
            // Simulate Not accessible
            return 1;
    });

    /* Verifing CreateStorage */
    EXPECT_EQ(Core::ERROR_NONE, mStorageManagerPlugin->CreateStorage(appId, size, path, errorReason));

    expectedPath = std::string(DEFAULT_APP_STORAGE_PATH) + std::string("/") + appId;

    EXPECT_CALL(*p_wrapsImplMock, chown(::testing::_, ::testing::_, ::testing::_))
        .WillRepeatedly([](const char *path, uid_t owner, gid_t group) -> int {
            // Simulate success
            TEST_LOG("### p_wrapsImplMock chown ###");
            return 0;
    });

    EXPECT_CALL(*p_wrapsImplMock, access(::testing::_, ::testing::_))
        .WillRepeatedly([](const char* pathname, int mode) {
            // Simulate accessible
            return 0;
    });

    /* Verifing  GetStorage */
    EXPECT_EQ(Core::ERROR_NONE, mStorageManagerPlugin->GetStorage(appId, 1000, 1001, actualPath, actualSize, actualUsed));
    TEST_LOG("GetStorage_Success actualPath = %s actualSize: %u used: %u",actualPath.c_str(), actualSize, actualUsed);

    EXPECT_STREQ(expectedPath.c_str(), actualPath.c_str());
    EXPECT_EQ(size, actualSize);
    EXPECT_EQ(0U, actualUsed);

    /* Verify DeleteStorage will success */
    EXPECT_EQ(Core::ERROR_NONE, mStorageManagerPlugin->DeleteStorage(appId, errorReason));

    /* Verify GetStorage will fail, it should return Core::ERROR_GENERAL as already deleted the storage */
    EXPECT_EQ(Core::ERROR_GENERAL, mStorageManagerPlugin->GetStorage(appId, 1000, 1001, actualPath, actualSize, actualUsed));

    if (Core::ERROR_NONE == status)
    {
        ReleaseStorageManagerInterfaceObjectUsingComRPCConnection();
    }

    TEST_LOG("### Test CreateStorageUsingComRpcSuccessExistingIfStorageNotAccessiable End ###");
}

/* Test Case for CreateStorageUsingComRpcSuccessIfPathExists
 * Creating Storage Manager Plugin using COM-RPC connection
 * Setting Mocks for access() to simulate storage path already exists and is accessible
 * Setting Mocks for statvfs() to simulate available disk space
 * Calling CreateStorage() with appId and verifying success on first creation
 * Re-calling CreateStorage() with the same appId to ensure it returns success if path already exists and is accessible
 * Calling DeleteStorage() to verify storage is successfully removed
 * Releasing the Storage Manager Interface object
 */
TEST_F(StorageManagerTest, CreateStorageUsingComRpcSuccessIfPathExists)
{
    uint32_t status = Core::ERROR_GENERAL;

    uint32_t size = 1024;
    std::string path = " ";
    std::string errorReason = ""; 
    std::string appId = "testApp";

    TEST_LOG("### Test CreateStorageUsingComRpcSuccessIfPathExists Begin ###");

    status = CreateStorageManagerInterfaceObjectUsingComRPCConnection();
    EXPECT_EQ(status,Core::ERROR_NONE);

    EXPECT_CALL(*p_wrapsImplMock, access(::testing::_, ::testing::_))
        .WillRepeatedly([](const char* pathname, int mode) {
            // Simulate able to access
            return 0;
    });

    EXPECT_CALL(*p_wrapsImplMock, statvfs(::testing::_, ::testing::_))
        .WillRepeatedly([](const char* path, struct statvfs* buf) {
            // Simulate success
            TEST_LOG("### p_wrapsImplMock statvfs ###");
            buf->f_bsize = STORAGE_BLOCK_SIZE; // Block size
            buf->f_frsize = STORAGE_FR_SIZE; // Fragment size
            buf->f_blocks = STORAGE_TOTAL_BLOCKS; // Total blocks
            buf->f_bfree = STORAGE_FREE_BLOCKS; // Free blocks
            buf->f_bavail = STORAGE_AVAILABLE_BLOCKS; // Available blocks
            return 0;
    });

    /* Verifing CreateStorage */
    EXPECT_EQ(Core::ERROR_NONE, mStorageManagerPlugin->CreateStorage(appId, size, path, errorReason));

    /* Call CreateStorage again with same AppId and expecting to return  success */
    EXPECT_EQ(Core::ERROR_NONE, mStorageManagerPlugin->CreateStorage(appId, size, path, errorReason));

    /* Delete the Storage */
    EXPECT_EQ(Core::ERROR_NONE, mStorageManagerPlugin->DeleteStorage(appId, errorReason));

    if (Core::ERROR_NONE == status)
    {
        ReleaseStorageManagerInterfaceObjectUsingComRPCConnection();
    }

    TEST_LOG("### Test CreateStorageUsingComRpcSuccessIfPathExists End ###");
}

/* Test Case for CreateStorageUsingComRpcFailiureIfEmptyAppId
 * Creating Storage Manager Plugin using COM-RPC connection
 * Calling CreateStorage() with an empty appId
 * Verifying that the method fails and returns Core::ERROR_GENERAL
 * Verifying the appropriate error message is set in errorReason ("appId cannot be empty")
 * Releasing the Storage Manager Interface object
 */
TEST_F(StorageManagerTest, CreateStorageUsingComRpcFailiureIfEmptyAppId)
{
    uint32_t status = Core::ERROR_GENERAL;

    uint32_t size = 1024;
    std::string path = " ";
    std::string errorReason = ""; 
    std::string appId = "";

    TEST_LOG("### Test CreateStorageUsingComRpcFailiureIfEmptyAppId Begin ###");

    status = CreateStorageManagerInterfaceObjectUsingComRPCConnection();
    EXPECT_EQ(status,Core::ERROR_NONE);

    /* Verifing CreateStorage with empty appId and expecting should return Core::ERROR_GENERAL and verify errorReason */
    EXPECT_EQ(Core::ERROR_GENERAL, mStorageManagerPlugin->CreateStorage(appId, size, path, errorReason));
    EXPECT_STREQ("appId cannot be empty", errorReason.c_str());

    if (Core::ERROR_NONE == status)
    {
        ReleaseStorageManagerInterfaceObjectUsingComRPCConnection();
    }

    TEST_LOG("### Test CreateStorageUsingComRpcFailiureIfEmptyAppId End ###");
}

/* Test Case for CreateStorageUsingComRpcFailiureIfSizeExceeded
 * Creating Storage Manager Plugin using COM-RPC connection
 * Setting Mocks for access() to simulate the storage path is accessible
 * Setting Mocks for statvfs() to simulate insufficient available storage space
 * Calling CreateStorage() with a size that exceeds available space
 * Verifying that the method fails and returns Core::ERROR_GENERAL
 * Verifying the appropriate error message is set in errorReason ("Insufficient storage space")
 * Releasing the Storage Manager Interface object
 */
TEST_F(StorageManagerTest, CreateStorageUsingComRpcFailiureIfSizeExceeded)
{
    uint32_t status = Core::ERROR_GENERAL;

    uint32_t size = 29046281;
    std::string path = " ";
    std::string errorReason = ""; 
    std::string appId = "testApp";

    TEST_LOG("### Test CreateStorageUsingComRpcFailiureIfSizeExceeded Begin ###");

    status = CreateStorageManagerInterfaceObjectUsingComRPCConnection();
    EXPECT_EQ(status,Core::ERROR_NONE);

    EXPECT_CALL(*p_wrapsImplMock, access(::testing::_, ::testing::_))
        .WillRepeatedly([](const char* pathname, int mode) {
            // Simulate able to access
            return 0;
    });

    EXPECT_CALL(*p_wrapsImplMock, statvfs(::testing::_, ::testing::_))
        .WillRepeatedly([](const char* path, struct statvfs* buf) {
            // Simulate success
            TEST_LOG("### p_wrapsImplMock statvfs ###");
            buf->f_bsize = STORAGE_BLOCK_SIZE; // Block size
            buf->f_frsize = STORAGE_FR_SIZE; // Fragment size
            buf->f_blocks = 100000; // Total blocks
            buf->f_bfree = 0; // Free blocks
            buf->f_bavail = 0; // Available blocks
            return 0;
    });

    /* Verifing CreateStorage */
    EXPECT_EQ(Core::ERROR_GENERAL, mStorageManagerPlugin->CreateStorage(appId, size, path, errorReason));
    EXPECT_STREQ("Insufficient storage space", errorReason.c_str());

    if (Core::ERROR_NONE == status)
    {
        ReleaseStorageManagerInterfaceObjectUsingComRPCConnection();
    }

    TEST_LOG("### Test CreateStorageUsingComRpcFailiureIfSizeExceeded End ###");
}

/* Test Case for GetStorageUsingComRpcFailiureIfEmptyAppId
 * Creating Storage Manager Plugin using COM-RPC connection
 * Setting Mocks for access() and statvfs() to simulate accessible storage path and available space
 * Calling CreateStorage() with valid appId to set up the environment
 * Calling GetStorage() with an empty appId and verifying it returns Core::ERROR_GENERAL
 * Verifying that storage retrieval fails due to invalid (empty) appId
 * Deleting the previously created storage
 * Releasing the Storage Manager Interface object
 */
TEST_F(StorageManagerTest, GetStorageUsingComRpcFailiureIfEmptyAppId)
{
    uint32_t status = Core::ERROR_GENERAL;

    uint32_t size = 1024;
    std::string path = " ";
    std::string errorReason = ""; 
    std::string appId = "testApp";
    std::string actualPath = "";
    uint32_t actualSize = 0;
    uint32_t actualUsed = 0;

    TEST_LOG("### Test GetStorageUsingComRpcFailiureIfEmptyAppId Begin ###");

    status = CreateStorageManagerInterfaceObjectUsingComRPCConnection();
    EXPECT_EQ(status,Core::ERROR_NONE);

    EXPECT_CALL(*p_wrapsImplMock, access(::testing::_, ::testing::_))
        .WillRepeatedly([](const char* pathname, int mode) {
            // Simulate able to access
            return 0;
    });

    EXPECT_CALL(*p_wrapsImplMock, statvfs(::testing::_, ::testing::_))
        .WillRepeatedly([](const char* path, struct statvfs* buf) {
            // Simulate success
            TEST_LOG("### p_wrapsImplMock statvfs ###");
            buf->f_bsize = STORAGE_BLOCK_SIZE; // Block size
            buf->f_frsize = STORAGE_FR_SIZE; // Fragment size
            buf->f_blocks = STORAGE_TOTAL_BLOCKS; // Total blocks
            buf->f_bfree = STORAGE_FREE_BLOCKS; // Free blocks
            buf->f_bavail = STORAGE_AVAILABLE_BLOCKS; // Available blocks
            return 0;
    });

    /* Verifing CreateStorage */
    EXPECT_EQ(Core::ERROR_NONE, mStorageManagerPlugin->CreateStorage(appId, size, path, errorReason));

    /* Verifing GetStorage with empty appId and expecting Core::ERROR_GENERAL */
    EXPECT_EQ(Core::ERROR_GENERAL, mStorageManagerPlugin->GetStorage(" ", 1000, 1001, actualPath, actualSize, actualUsed));

    /* Delete the Storage */
    EXPECT_EQ(Core::ERROR_NONE, mStorageManagerPlugin->DeleteStorage(appId, errorReason));

    if (Core::ERROR_NONE == status)
    {
        ReleaseStorageManagerInterfaceObjectUsingComRPCConnection();
    }

    TEST_LOG("### Test GetStorageUsingComRpcFailiureIfEmptyAppId End ###");
}

/* Test Case for DeleteStorageUsingComRpcFailiureIfEmptyAppId
 * Creating Storage Manager Plugin using COM-RPC connection
 * Setting Mocks for access() and statvfs() to simulate accessible path and available storage
 * Calling CreateStorage() with a valid appId to ensure storage is initialized
 * Calling DeleteStorage() with an empty appId and verifying it fails with Core::ERROR_GENERAL
 * Verifying that the appropriate error message ("AppId is empty") is set in errorReason
 * Releasing the Storage Manager Interface object
 */
TEST_F(StorageManagerTest, DeleteStorageUsingComRpcFailiureIfEmptyAppId)
{
    uint32_t status = Core::ERROR_GENERAL;

    uint32_t size = 1024;
    std::string path = " ";
    std::string errorReason = ""; 
    std::string appId = "testApp";

    TEST_LOG("### Test DeleteStorageUsingComRpcFailiureIfEmptyAppId Begin ###");

    status = CreateStorageManagerInterfaceObjectUsingComRPCConnection();
    EXPECT_EQ(status,Core::ERROR_NONE);

    EXPECT_CALL(*p_wrapsImplMock, access(::testing::_, ::testing::_))
        .WillRepeatedly([](const char* pathname, int mode) {
            // Simulate able to access
            return 0;
    });

    EXPECT_CALL(*p_wrapsImplMock, statvfs(::testing::_, ::testing::_))
        .WillRepeatedly([](const char* path, struct statvfs* buf) {
            // Simulate success
            TEST_LOG("### p_wrapsImplMock statvfs ###");
            buf->f_bsize = STORAGE_BLOCK_SIZE; // Block size
            buf->f_frsize = STORAGE_FR_SIZE; // Fragment size
            buf->f_blocks = STORAGE_TOTAL_BLOCKS; // Total blocks
            buf->f_bfree = STORAGE_FREE_BLOCKS; // Free blocks
            buf->f_bavail = STORAGE_AVAILABLE_BLOCKS; // Available blocks
            return 0;
    });

    /* Verifing CreateStorage */
    EXPECT_EQ(Core::ERROR_NONE, mStorageManagerPlugin->CreateStorage(appId, size, path, errorReason));

    /* Verifing DeleteStorage with empty appId and expecting Core::ERROR_GENERAL */
    EXPECT_EQ(Core::ERROR_GENERAL, mStorageManagerPlugin->DeleteStorage("", errorReason));
    EXPECT_STREQ("AppId is empty", errorReason.c_str());

    if (Core::ERROR_NONE == status)
    {
        ReleaseStorageManagerInterfaceObjectUsingComRPCConnection();
    }

    TEST_LOG("### Test DeleteStorageUsingComRpcFailiureIfEmptyAppId End ###");
}

/* Test Case for ClearStorageUsingJsonRpcSuccess
 * Creating Storage Manager Plugin using COM-RPC connection
 * Setting Mocks for access() and statvfs() to simulate accessible path and available storage
 * Calling CreateStorage() with a valid appId to initialize storage
 * Creating a sample file inside the storage path to simulate existing data
 * Using JSON-RPC method "clear" to invoke storage content clearing for the specified appId
 * Verifying that the previously created file is deleted and storage is cleared successfully
 * Calling DeleteStorage() to remove the storage and verify successful deletion
 * Releasing the Storage Manager Interface object
 */
TEST_F(StorageManagerTest, ClearStorageUsingJsonRpcSuccess)
{
    uint32_t status = Core::ERROR_GENERAL;

    uint32_t size = 1024;
    std::string path = " ";
    std::string errorReason = ""; 
    std::string appId = "testApp";

    TEST_LOG("### Test ClearStorageUsingJsonRpcSuccess Begin ###");

    status = CreateStorageManagerInterfaceObjectUsingComRPCConnection();
    EXPECT_EQ(status,Core::ERROR_NONE);

    EXPECT_CALL(*p_wrapsImplMock, access(::testing::_, ::testing::_))
        .WillRepeatedly([](const char* pathname, int mode) {
            // Simulate able to access
            return 0;
    });

    EXPECT_CALL(*p_wrapsImplMock, statvfs(::testing::_, ::testing::_))
        .WillRepeatedly([](const char* path, struct statvfs* buf) {
            // Simulate success
            TEST_LOG("### p_wrapsImplMock statvfs ###");
            buf->f_bsize = STORAGE_BLOCK_SIZE; // Block size
            buf->f_frsize = STORAGE_FR_SIZE; // Fragment size
            buf->f_blocks = STORAGE_TOTAL_BLOCKS; // Total blocks
            buf->f_bfree = STORAGE_FREE_BLOCKS; // Free blocks
            buf->f_bavail = STORAGE_AVAILABLE_BLOCKS; // Available blocks
            return 0;
    });

    /* Create the Storage for appId */
    EXPECT_EQ(Core::ERROR_NONE, mStorageManagerPlugin->CreateStorage(appId, size, path, errorReason));

    /* Mocking for adding some content in storage path of the appId */
    createFileAndAddContent(std::string(DEFAULT_APP_STORAGE_PATH), std::string("example1.txt"), appId, std::string("Text file for AiManager testApp"));

    /* Clearing the contents of the appId */
    JsonObject params;
    JsonObject resultJson;
    params["appId"] = appId;
    /* Invoking setAppProperty method */
    status = InvokeServiceMethod(STORAGEMANAGER_CALLSIGN, "clear", params, resultJson);
    EXPECT_EQ(status,Core::ERROR_NONE);

    /* Verify file is not present in the path */
    std::string fileName1 = std::string(DEFAULT_APP_STORAGE_PATH) + std::string("/") + appId + "/example/" + std::string("example1.txt") ;
    EXPECT_EQ(false, fileExists(fileName1));

    /* Verifing DeleteStorage with appId and expecting Core::ERROR_NONE */
    EXPECT_EQ(Core::ERROR_NONE, mStorageManagerPlugin->DeleteStorage(appId, errorReason));

    if (Core::ERROR_NONE == status)
    {
        ReleaseStorageManagerInterfaceObjectUsingComRPCConnection();
    }

    TEST_LOG("### Test ClearStorageUsingJsonRpcSuccess End ###");
}

/* Test Case for ClearStorageUsingJsonRpcFailiureIfEmptyAppId
 * Creates Storage Manager Plugin using COM-RPC connection
 * Sets mocks for access() and statvfs() to simulate accessible storage path
 * Creates storage for a valid appId and adds a sample file inside the storage path
 * Attempts to clear storage content using JSON-RPC "clear" method with an empty appId
 * Expects failure (Core::ERROR_GENERAL) and verifies that the sample file still exists
 * Deletes the storage created for cleanup and verifies success
 * Releases the Storage Manager Interface object
 */
TEST_F(StorageManagerTest, ClearStorageUsingJsonRpcFailiureIfEmptyAppId)
{
    uint32_t status = Core::ERROR_GENERAL;

    uint32_t size = 1024;
    std::string path = " ";
    std::string errorReason = ""; 
    std::string appId = "testApp";

    TEST_LOG("### Test ClearStorageUsingJsonRpcFailiureIfEmptyAppId Begin ###");

    status = CreateStorageManagerInterfaceObjectUsingComRPCConnection();
    EXPECT_EQ(status,Core::ERROR_NONE);

    EXPECT_CALL(*p_wrapsImplMock, access(::testing::_, ::testing::_))
        .WillRepeatedly([](const char* pathname, int mode) {
            // Simulate able to access
            return 0;
    });

    EXPECT_CALL(*p_wrapsImplMock, statvfs(::testing::_, ::testing::_))
        .WillRepeatedly([](const char* path, struct statvfs* buf) {
            // Simulate success
            TEST_LOG("### p_wrapsImplMock statvfs ###");
            buf->f_bsize = STORAGE_BLOCK_SIZE; // Block size
            buf->f_frsize = STORAGE_FR_SIZE; // Fragment size
            buf->f_blocks = STORAGE_TOTAL_BLOCKS; // Total blocks
            buf->f_bfree = STORAGE_FREE_BLOCKS; // Free blocks
            buf->f_bavail = STORAGE_AVAILABLE_BLOCKS; // Available blocks
            return 0;
    });

    /* Create the Storage for appId */
    EXPECT_EQ(Core::ERROR_NONE, mStorageManagerPlugin->CreateStorage(appId, size, path, errorReason));

    /* Mocking for adding some content in storage path of the appId */
    /* Mocking for adding some content in storage path of the appId */
    createFileAndAddContent(std::string(DEFAULT_APP_STORAGE_PATH), std::string("example1.txt"), appId, std::string("Text file for AiManager testApp"));

    /* Clearing the contents of the appId */
    JsonObject params;
    JsonObject resultJson;
    params["appId"] = "";
    /* Invoking setAppProperty method */
    status = InvokeServiceMethod(STORAGEMANAGER_CALLSIGN, "clear", params, resultJson);
    EXPECT_EQ(status,Core::ERROR_GENERAL);

    /* Verify file is present in the path */
    std::string fileName1 = std::string(DEFAULT_APP_STORAGE_PATH) + std::string("/") + appId + "/example/" + std::string("example1.txt") ;
    EXPECT_EQ(true, fileExists(fileName1));

    /* Verifing DeleteStorage with appId and expecting Core::ERROR_NONE */
    EXPECT_EQ(Core::ERROR_NONE, mStorageManagerPlugin->DeleteStorage(appId, errorReason));

    if (Core::ERROR_NONE == status)
    {
        ReleaseStorageManagerInterfaceObjectUsingComRPCConnection();
    }

    TEST_LOG("### Test ClearStorageUsingJsonRpcFailiureIfEmptyAppId End ###");
}

/*
 * Test Case: ClearAllStorageUsingJsonRpcWithoutExemptionSuccess
 * Initializes StorageManagerPlugin via COM-RPC connection.
 * Mocks access() and statvfs() calls to simulate accessible storage with sufficient space.
 * Creates storage for three distinct app IDs (appId1, appId2, appId3).
 * Adds sample files inside each app's storage directory.
 * Invokes the JSON-RPC "clearAll" method without exempting any app IDs.
 * Expects the clearing operation to succeed (Core::ERROR_NONE).
 * Verifies that all sample files are deleted from all app storage paths.
 * Deletes all created storages and verifies successful deletion.
 * Releases the StorageManager interface after test completion.
 */
TEST_F(StorageManagerTest, ClearAllStorageUsingJsonRpcWithoutExemptionSuccess)
{
    uint32_t status = Core::ERROR_GENERAL;

    uint32_t size = 1024;
    std::string path = " ";
    std::string errorReason = ""; 
    std::string appId1 = "testApp1";
    std::string appId2 = "testApp2";
    std::string appId3 = "testApp3";

    TEST_LOG("### Test ClearAllStorageUsingJsonRpcWithoutExemptionSuccess Begin ###");

    status = CreateStorageManagerInterfaceObjectUsingComRPCConnection();
    EXPECT_EQ(status,Core::ERROR_NONE);

    EXPECT_CALL(*p_wrapsImplMock, access(::testing::_, ::testing::_))
        .WillRepeatedly([](const char* pathname, int mode) {
            // Simulate able to access
            return 0;
    });

    EXPECT_CALL(*p_wrapsImplMock, statvfs(::testing::_, ::testing::_))
        .WillRepeatedly([](const char* path, struct statvfs* buf) {
            // Simulate success
            TEST_LOG("### p_wrapsImplMock statvfs ###");
            buf->f_bsize = STORAGE_BLOCK_SIZE; // Block size
            buf->f_frsize = STORAGE_FR_SIZE; // Fragment size
            buf->f_blocks = STORAGE_TOTAL_BLOCKS; // Total blocks
            buf->f_bfree = STORAGE_FREE_BLOCKS; // Free blocks
            buf->f_bavail = STORAGE_AVAILABLE_BLOCKS; // Available blocks
            return 0;
    });

    /* Create the Storage for appId1 */
    EXPECT_EQ(Core::ERROR_NONE, mStorageManagerPlugin->CreateStorage(appId1, size, path, errorReason));

    /* Create the Storage for appId2 */
    EXPECT_EQ(Core::ERROR_NONE, mStorageManagerPlugin->CreateStorage(appId2, size, path, errorReason));

    /* Create the Storage for appId3 */
    EXPECT_EQ(Core::ERROR_NONE, mStorageManagerPlugin->CreateStorage(appId3, size, path, errorReason));

    /* Mocking for adding some content in storage path of the appId */
    createFileAndAddContent(std::string(DEFAULT_APP_STORAGE_PATH), std::string("example1.txt"), appId1, std::string("Text file for AiManager testApp1"));
    createFileAndAddContent(std::string(DEFAULT_APP_STORAGE_PATH), std::string("example2.txt"), appId2, std::string("Text file for AiManager testApp2"));
    createFileAndAddContent(std::string(DEFAULT_APP_STORAGE_PATH), std::string("example3.txt"), appId3, std::string("Text file for AiManager testApp3"));

    /* Clearing the contents of the appId */
    JsonObject params;
    JsonObject resultJson;
    params["exemptionAppIds"] = {""};
    /* Invoking setAppProperty method */
    status = InvokeServiceMethod(STORAGEMANAGER_CALLSIGN, "clearAll", params, resultJson);
    EXPECT_EQ(status,Core::ERROR_NONE);

    /* Verify file is not present in the path */

    std::string fileName1 = std::string(DEFAULT_APP_STORAGE_PATH) + std::string("/") + appId1 + "/example/" + std::string("example1.txt") ;
    std::string fileName2 = std::string(DEFAULT_APP_STORAGE_PATH) + std::string("/") + appId2 + "/example/" + std::string("example2.txt") ;
    std::string fileName3 = std::string(DEFAULT_APP_STORAGE_PATH) + std::string("/") + appId3 + "/example/" + std::string("example3.txt") ;
    EXPECT_EQ(false, fileExists(fileName1));
    EXPECT_EQ(false, fileExists(fileName2));
    EXPECT_EQ(false, fileExists(fileName3));

    /* Verifing DeleteStorage with appId and expecting Core::ERROR_NONE */
    EXPECT_EQ(Core::ERROR_NONE, mStorageManagerPlugin->DeleteStorage(appId1, errorReason));
    EXPECT_EQ(Core::ERROR_NONE, mStorageManagerPlugin->DeleteStorage(appId2, errorReason));
    EXPECT_EQ(Core::ERROR_NONE, mStorageManagerPlugin->DeleteStorage(appId3, errorReason));

    if (Core::ERROR_NONE == status)
    {
        ReleaseStorageManagerInterfaceObjectUsingComRPCConnection();
    }

    TEST_LOG("### Test ClearAllStorageUsingJsonRpcWithoutExemptionSuccess End ###");
}

/*
 * Test Case: ClearAllStorageUsingJsonRpcWithExemptionSuccess
 *  Initializes StorageManagerPlugin via COM-RPC connection.
 *  Mocks access() and statvfs() calls to simulate accessible storage with sufficient space.
 *  Creates storage for three app IDs: appId1, appId2, and appId3.
 *  Adds example files to each app's storage directory.
 *  Invokes the JSON-RPC "clearAll" method with exemption list containing appId1 and appId2.
 *  Expects the clearAll operation to succeed (Core::ERROR_NONE).
 *  Verifies that files for exempted apps (appId1 and appId2) still exist.
 *  Verifies that the file for the non-exempted app (appId3) is deleted.
 *  Deletes all three storages and verifies successful deletion.
 *  Releases the StorageManager interface after test completion.
 */
TEST_F(StorageManagerTest, ClearAllStorageUsingJsonRpcWithExemptionSuccess)
{
    uint32_t status = Core::ERROR_GENERAL;

    uint32_t size = 1024;
    std::string path = " ";
    std::string errorReason = ""; 
    std::string appId1 = "testApp1";
    std::string appId2 = "testApp2";
    std::string appId3 = "testApp3";

    TEST_LOG("### Test ClearAllStorageUsingJsonRpcWithExemptionSuccess Begin ###");

    status = CreateStorageManagerInterfaceObjectUsingComRPCConnection();
    EXPECT_EQ(status,Core::ERROR_NONE);

    EXPECT_CALL(*p_wrapsImplMock, access(::testing::_, ::testing::_))
        .WillRepeatedly([](const char* pathname, int mode) {
            // Simulate able to access
            return 0;
    });

    EXPECT_CALL(*p_wrapsImplMock, statvfs(::testing::_, ::testing::_))
        .WillRepeatedly([](const char* path, struct statvfs* buf) {
            TEST_LOG("### p_wrapsImplMock statvfs ###");
            buf->f_bsize = STORAGE_BLOCK_SIZE; // Block size
            buf->f_frsize = STORAGE_FR_SIZE; // Fragment size
            buf->f_blocks = STORAGE_TOTAL_BLOCKS; // Total blocks
            buf->f_bfree = STORAGE_FREE_BLOCKS; // Free blocks
            buf->f_bavail = STORAGE_AVAILABLE_BLOCKS; // Available blocks
            return 0;
    });

    /* Create the Storage for appId1 */
    EXPECT_EQ(Core::ERROR_NONE, mStorageManagerPlugin->CreateStorage(appId1, size, path, errorReason));

    /* Create the Storage for appId2 */
    EXPECT_EQ(Core::ERROR_NONE, mStorageManagerPlugin->CreateStorage(appId2, size, path, errorReason));

    /* Create the Storage for appId3 */
    EXPECT_EQ(Core::ERROR_NONE, mStorageManagerPlugin->CreateStorage(appId3, size, path, errorReason));

    /* Mocking for adding some content in storage path of the appId */
    createFileAndAddContent(std::string(DEFAULT_APP_STORAGE_PATH), std::string("example1.txt"), appId1, std::string("Text file for AiManager testApp1"));
    createFileAndAddContent(std::string(DEFAULT_APP_STORAGE_PATH), std::string("example2.txt"), appId2, std::string("Text file for AiManager testApp2"));
    createFileAndAddContent(std::string(DEFAULT_APP_STORAGE_PATH), std::string("example3.txt"), appId3, std::string("Text file for AiManager testApp3"));

    /* Clearing the contents of the appId */
    JsonObject params;
    JsonObject resultJson;
    string applist;

    params["exemptionAppIds"] = {"[\"testApp1\",\"testApp2\"]"};

    /* Invoking setAppProperty method */
    status = InvokeServiceMethod(STORAGEMANAGER_CALLSIGN, "clearAll", params, resultJson);
    EXPECT_EQ(status,Core::ERROR_NONE);

    /* Verify file is present in the path */
    std::string fileName1 = std::string(DEFAULT_APP_STORAGE_PATH) + std::string("/") + appId1 + "/example/" + std::string("example1.txt") ;
    std::string fileName2 = std::string(DEFAULT_APP_STORAGE_PATH) + std::string("/") + appId2 + "/example/" + std::string("example2.txt") ;
    std::string fileName3 = std::string(DEFAULT_APP_STORAGE_PATH) + std::string("/") + appId3 + "/example/" + std::string("example3.txt") ;
    EXPECT_EQ(true, fileExists(fileName1));
    EXPECT_EQ(true, fileExists(fileName2));

    /* Verify file is not present in the path */
    EXPECT_EQ(false, fileExists(fileName3));

    /* Verifing DeleteStorage with appId and expecting Core::ERROR_NONE */
    EXPECT_EQ(Core::ERROR_NONE, mStorageManagerPlugin->DeleteStorage(appId1, errorReason));
    EXPECT_EQ(Core::ERROR_NONE, mStorageManagerPlugin->DeleteStorage(appId2, errorReason));
    EXPECT_EQ(Core::ERROR_NONE, mStorageManagerPlugin->DeleteStorage(appId3, errorReason));
    if (Core::ERROR_NONE == status)
    {
        ReleaseStorageManagerInterfaceObjectUsingComRPCConnection();
    }

    TEST_LOG("### Test ClearAllStorageUsingJsonRpcWithExemptionSuccess End ###");
}

/*
 * Test Case: ClearAllStorageUsingJsonRpcFailiureIfStorageDirectoryNotPresent
 * Attempts to remove the storage manager directory (/tmp/persistent/storageManager).
 * Verifies that the directory removal was attempted (logs success or failure).
 * Invokes the JSON-RPC "clearAll" method with an empty exemption list.
 * Expects the clearAll operation to fail with Core::ERROR_GENERAL since the storage directory is missing.
 * Does not create or initialize any storage objects prior to the clearAll call.
 */
TEST_F(StorageManagerTest, ClearAllStorageUsingJsonRpcFailiureIfStorageDirectoryNotPresent)
{
    uint32_t status = Core::ERROR_GENERAL;
    std::string path = " ";
    std::string errorReason = ""; 

    TEST_LOG("### Test ClearAllStorageUsingJsonRpcFailiureIfStorageDirectoryNotPresent Begin ###");

    if (rmdir("/tmp/persistent/storageManager") == 0)
    {
        TEST_LOG("Directory removed successfully");
    }
    else
    {
        TEST_LOG("Directory removed Failed");
    }

    /* Clearing the contents of the appId */
    JsonObject params;
    JsonObject resultJson;
    params["exemptionAppIds"] = {"[]"};
    /* Invoking setAppProperty method */
    status = InvokeServiceMethod(STORAGEMANAGER_CALLSIGN, "clearAll", params, resultJson);
    EXPECT_EQ(status,Core::ERROR_GENERAL);

    TEST_LOG("### Test ClearAllStorageUsingJsonRpcFailiureIfStorageDirectoryNotPresent End ###");
}

