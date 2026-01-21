/**
* If not stated otherwise in this file or this component's LICENSE
* file the following copyright and licenses apply:
*
* Copyright 2025 RDK Management
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
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <mntent.h>
#include <fstream>
#include <string>
#include <vector>
#include <cstdio>
#include <mutex>
#include <chrono>
#include <condition_variable>

#include "PackageManager.h"
#include "PackageManagerImplementation.h"
#include "StorageManagerMock.h"
#include "ISubSystemMock.h"
#include "ServiceMock.h"
#include "COMLinkMock.h"
#include "ThunderPortability.h"
#include "WorkerPoolImplementation.h"
#include "FactoriesImplementation.h"

#define TEST_LOG(x, ...) fprintf(stderr, "\033[1;32m[%s:%d](%s)<PID:%d><TID:%d>" x "\n\033[0m", __FILE__, __LINE__, __FUNCTION__, getpid(), gettid(), ##__VA_ARGS__); fflush(stderr);
#define TIMEOUT   (500)
#define TIMEOUT_FOR_PAUSE (200)
#define TIMEOUT_FOR_INIT (200)
#define TIMEOUT_FOR_INSTALL (200)

using ::testing::NiceMock;
using namespace WPEFramework;
using namespace std;

typedef enum : uint32_t {
    PackageManager_invalidStatus = 0,
    PackageManager_AppDownloadStatus,
    PackageManager_AppInstallStatus
} PackageManagerTest_status_t;

struct StatusParams {
    string packageId;
    string version;
    string downloadId;
    string fileLocator;
    Exchange::IPackageDownloader::Reason reason;
};

class PackageManagerTest : public ::testing::Test {
protected:
    // Declare the protected members
    ServiceMock* mServiceMock = nullptr;
    StorageManagerMock* mStorageManagerMock = nullptr;
    SubSystemMock* mSubSystemMock = nullptr;

    Core::ProxyType<Plugin::PackageManager> plugin;
    Core::JSONRPC::Handler& mJsonRpcHandler;
    Core::JSONRPC::Message message;
    DECL_CORE_JSONRPC_CONX connection;
    string mJsonRpcResponse;
    string uri;

    PLUGINHOST_DISPATCHER *dispatcher;
    FactoriesImplementation factoriesImplementation;

    Core::ProxyType<Plugin::PackageManagerImplementation> mPackageManagerImpl;
    Core::ProxyType<WorkerPoolImplementation> workerPool;

    Exchange::IPackageDownloader* pkgdownloaderInterface = nullptr;
    Exchange::IPackageInstaller* pkginstallerInterface = nullptr;
    Exchange::IPackageHandler* pkghandlerInterface = nullptr;
    Exchange::IPackageDownloader::Options options;
    Exchange::IPackageDownloader::DownloadId downloadId;
    Exchange::IPackageDownloader::ProgressInfo progress;
    Exchange::IPackageDownloader::PackageInfo packageInfo;
    std::list<Exchange::IPackageDownloader::PackageInfo> packageInfoList;
    Exchange::IPackageDownloader::IPackageInfoIterator* packageInfoIterator = nullptr;

    // Constructor
    PackageManagerTest()
	: workerPool(Core::ProxyType<WorkerPoolImplementation>::Create(
            2, Core::Thread::DefaultStackSize(), 16)),
      plugin(Core::ProxyType<Plugin::PackageManager>::Create()),
      mJsonRpcHandler(*plugin),
      INIT_CONX(1,0)
    {
        mPackageManagerImpl = Core::ProxyType<Plugin::PackageManagerImplementation>::Create();

        pkgdownloaderInterface = static_cast<Exchange::IPackageDownloader*>(mPackageManagerImpl->QueryInterface(Exchange::IPackageDownloader::ID));

        pkginstallerInterface = static_cast<Exchange::IPackageInstaller*>(mPackageManagerImpl->QueryInterface(Exchange::IPackageInstaller::ID));

        pkghandlerInterface = static_cast<Exchange::IPackageHandler*>(mPackageManagerImpl->QueryInterface(Exchange::IPackageHandler::ID));

		Core::IWorkerPool::Assign(&(*workerPool));
		workerPool->Run();
    }

    // Destructor
    virtual ~PackageManagerTest() override
    {
        pkgdownloaderInterface->Release();
        pkginstallerInterface->Release();
        pkghandlerInterface->Release();
        
        Core::IWorkerPool::Assign(nullptr);
		workerPool.Release();
    }
	
	void SetUp() override 
	{		
		// Set up mocks and expect calls
        mServiceMock = new NiceMock<ServiceMock>;
        mStorageManagerMock = new NiceMock<StorageManagerMock>;
        mSubSystemMock = new NiceMock<SubSystemMock>;

        EXPECT_CALL(*mServiceMock, QueryInterfaceByCallsign(::testing::_, ::testing::_))
          .Times(::testing::AnyNumber())
          .WillRepeatedly(::testing::Invoke(
              [&](const uint32_t, const std::string& name) -> void* {
                if (name == "org.rdk.StorageManager") {
                    return reinterpret_cast<void*>(mStorageManagerMock);
                } 
            return nullptr;
        }));

        EXPECT_CALL(*mServiceMock, ConfigLine())
          .Times(::testing::AnyNumber())
          .WillRepeatedly(::testing::Return("{\"downloadDir\": \"/opt/CDL/\"}"));

        EXPECT_CALL(*mServiceMock, SubSystems())
          .Times(::testing::AnyNumber())
          .WillRepeatedly(::testing::Return(mSubSystemMock));
    }

    void initforJsonRpc() 
    {    
        EXPECT_CALL(*mServiceMock, Register(::testing::_))
          .Times(::testing::AnyNumber());

        EXPECT_CALL(*mServiceMock, AddRef())
          .Times(::testing::AnyNumber());

        // Activate the dispatcher and initialize the plugin for JSON-RPC
        PluginHost::IFactories::Assign(&factoriesImplementation);
        dispatcher = static_cast<PLUGINHOST_DISPATCHER*>(plugin->QueryInterface(PLUGINHOST_DISPATCHER_ID));
        dispatcher->Activate(mServiceMock);
        plugin->Initialize(mServiceMock);  
    }

    void initforComRpc() 
    {
        EXPECT_CALL(*mServiceMock, AddRef())
          .Times(::testing::AnyNumber());

        // Initialize the plugin for COM-RPC
        pkgdownloaderInterface->Initialize(mServiceMock);
    }

    void getDownloadParams()
    {
        // Initialize the parameters required for COM-RPC with default values
        uri = "https://www.examplefile.com/file-download/328";

        options = { 
            true,2,1024
        };

        downloadId = {};
    }

    void TearDown() override
    {
        // Clean up mocks
		if (mServiceMock != nullptr)
        {
			delete mServiceMock;
			mServiceMock = nullptr;
        }

        if(mSubSystemMock != nullptr)
        {
            delete mSubSystemMock;
            mSubSystemMock = nullptr;
        }
    }

    void deinitforJsonRpc() 
    {
        EXPECT_CALL(*mServiceMock, Unregister(::testing::_))
          .Times(::testing::AnyNumber());

        EXPECT_CALL(*mServiceMock, Release())
          .Times(::testing::AnyNumber());

        // Deactivate the dispatcher and deinitialize the plugin for JSON-RPC
        dispatcher->Deactivate();
        dispatcher->Release();

        plugin->Deinitialize(mServiceMock);
        
        if(mStorageManagerMock != nullptr)
        {
            delete mStorageManagerMock;
            mStorageManagerMock = nullptr;
        }
    }

    void deinitforComRpc()
    {
        EXPECT_CALL(*mServiceMock, Release())
          .Times(::testing::AnyNumber());

        EXPECT_CALL(*mStorageManagerMock, Release())
          .WillOnce(::testing::Invoke(
                [&]() {
                     delete mStorageManagerMock;
                     mStorageManagerMock = nullptr;
                     return 0;
            }));

        // Deinitialize the plugin for COM-RPC
        pkgdownloaderInterface->Deinitialize(mServiceMock);
    }

    void waitforSignal(uint32_t timeout_ms) 
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(timeout_ms));
    }
};

class NotificationTest : public Exchange::IPackageDownloader::INotification, 
                         public Exchange::IPackageInstaller::INotification
{
    private:
        BEGIN_INTERFACE_MAP(NotificationTest)
        INTERFACE_ENTRY(Exchange::IPackageDownloader::INotification)
        INTERFACE_ENTRY(Exchange::IPackageInstaller::INotification)
        END_INTERFACE_MAP

    public:
        /** @brief Mutex */
        std::mutex m_mutex;

        /** @brief Condition variable */
        std::condition_variable m_condition_variable;

        /** @brief Status signal flag */
        uint32_t m_status_signal = PackageManager_invalidStatus;

        StatusParams m_status_param;

        NotificationTest(){}
        ~NotificationTest(){}

        void SetStatusParams(const StatusParams& statusParam)
        {
            m_status_param = statusParam;
        }

        void OnAppDownloadStatus(Exchange::IPackageDownloader::IPackageInfoIterator* const packageInfos) override
        {
            m_status_signal = PackageManager_AppDownloadStatus;
            JsonValue downloadId;
            JsonValue fileLocator;
            JsonValue failReason;

            std::unique_lock<std::mutex> lock(m_mutex);
            if(packageInfos != nullptr) 
            {
                Exchange::IPackageDownloader::PackageInfo resultItem{};

                while (packageInfos->Next(resultItem) == true)
                {
                    downloadId = resultItem.downloadId;
                    fileLocator = resultItem.fileLocator;
                    failReason = (resultItem.reason == Exchange::IPackageDownloader::Reason::NONE) ? "NONE" :
                                (resultItem.reason == Exchange::IPackageDownloader::Reason::DOWNLOAD_FAILURE) ? "DOWNLOAD_FAILURE" :
                                (resultItem.reason == Exchange::IPackageDownloader::Reason::DISK_PERSISTENCE_FAILURE) ? "DISK_PERSISTENCE_FAILURE" : "UNKNOWN";
                }
            }

            EXPECT_EQ(m_status_param.downloadId, downloadId.String());

            m_condition_variable.notify_one();
        }

        void OnAppInstallationStatus(const string& jsonresponse) override
        {
            m_status_signal = PackageManager_AppInstallStatus;
            JsonValue packageId;
            JsonValue version;
            
            JsonArray arr;
            if(arr.IElement::FromString(jsonresponse) && arr.Length() > 0) {
                JsonObject obj = arr[0].Object();
                packageId = obj["packageId"];
                version = obj["version"]; 
            }

            std::unique_lock<std::mutex> lock(m_mutex);
            EXPECT_EQ(m_status_param.packageId, packageId.String());
            EXPECT_EQ(m_status_param.version, version.String());

            m_condition_variable.notify_one();
        }

        uint32_t WaitForStatusSignal(uint32_t timeout_ms, PackageManagerTest_status_t status)
        {
            uint32_t status_signal = PackageManager_invalidStatus;
            std::unique_lock<std::mutex> lock(m_mutex);
            auto now = std::chrono::steady_clock::now();
            auto timeout = std::chrono::milliseconds(timeout_ms);
            if (m_condition_variable.wait_until(lock, now + timeout) == std::cv_status::timeout)
            {
                 TEST_LOG("Timeout waiting for request status event");
                 return m_status_signal;
            }
            status_signal = m_status_signal;
            m_status_signal = PackageManager_invalidStatus;
            return status_signal;
        }
    };

/* Test Case for verifying registered methods using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Check if the methods listed exist by using the Exists() from the JSON RPC handler
 * Verify the methods exist by asserting that Exists() returns Core::ERROR_NONE
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, registeredMethodsusingJsonRpc) {

    initforJsonRpc();

    // TC-1: Check if the listed methods exist using JsonRpc
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Exists(_T("download")));
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Exists(_T("pause")));
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Exists(_T("resume")));
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Exists(_T("cancel")));
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Exists(_T("delete")));
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Exists(_T("progress")));
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Exists(_T("getStorageInformation")));
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Exists(_T("rateLimit")));
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Exists(_T("install")));
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Exists(_T("uninstall")));
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Exists(_T("listPackages")));
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Exists(_T("config")));
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Exists(_T("packageState")));
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Exists(_T("lock")));
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Exists(_T("unlock")));
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Exists(_T("getLockedInfo")));

	deinitforJsonRpc();
}

/* Test Case for adding download request to a regular queue using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, notifications/events, mocks and expectations
 * Invoke the download method using the JSON RPC handler, passing the required parameters 
 * Verify that the download method is invoked successfully by asserting that it returns Core::ERROR_NONE and checking the downloadId
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, downloadMethodusingJsonRpcSuccess) {
    
    initforJsonRpc();

    Core::Event onAppDownloadStatus(false, true);

    EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillRepeatedly(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type) {
                return true;
            }));

    EXPECT_CALL(*mServiceMock, Submit(::testing::_, ::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const uint32_t, const Core::ProxyType<Core::JSON::IElement>& json) {
                onAppDownloadStatus.SetEvent();
                return Core::ERROR_NONE;
            }));

    EVENT_SUBSCRIBE(0, _T("onAppDownloadStatus"), _T("org.rdk.PackageManagerRDKEMS"), message);
    
    // TC-2: Add download request to regular queue using JsonRpc
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("download"), _T("{\"url\": \"https://httpbin.org/bytes/1024\"}"), mJsonRpcResponse));

    EXPECT_EQ(Core::ERROR_NONE, onAppDownloadStatus.Lock());
    EVENT_UNSUBSCRIBE(0, _T("onAppDownloadStatus"), _T("org.rdk.PackageManagerRDKEMS"), message);

    EXPECT_NE(mJsonRpcResponse.find("1001"), std::string::npos);

    deinitforJsonRpc();
}

/* Test Case for checking download request error when internet is unavailable using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the method using the JSON RPC handler, passing the required parameters
 * Verify download method error due to unavailability of internet by asserting that it returns Core::ERROR_UNAVAILABLE
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, downloadMethodusingJsonRpcError) {
    
    initforJsonRpc();

    EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type) {
                return false;
            }));
    
    // TC-3: Download request error when internet is unavailable using JsonRpc
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, mJsonRpcHandler.Invoke(connection, _T("download"), _T("{\"url\": \"https://httpbin.org/bytes/1024\"}"), mJsonRpcResponse));

    deinitforJsonRpc();
}

/* Test Case for adding download request to a priority queue using ComRpc
 * 
 * Set up and initialize required COM-RPC resources, configurations, notifications/events, mocks and expectations
 * Obtain the required parameters for downloading using the getDownloadParams()
 * Call the Download method using the COM RPC interface along with the required parameters, setting priority as true and wait
 * Verify successful download request by asserting that it returns Core::ERROR_NONE and checking the downloadId
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, downloadMethodsusingComRpcSuccess) {

    initforComRpc();

    getDownloadParams();

    uri = "https://httpbin.org/bytes/1024";

    EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillRepeatedly(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type){
                return true;
            }));
    
    // TC-4: Add download request to priority queue using ComRpc
    EXPECT_EQ(Core::ERROR_NONE, pkgdownloaderInterface->Download(uri, options, downloadId));
    
    waitforSignal(TIMEOUT);

    EXPECT_EQ(downloadId.downloadId, "1001");

	deinitforComRpc();
}

/* Test Case for checking download request error when internet is unavailable using ComRpc
 * 
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Obtain the required parameters for downloading using the getDownloadParams()
 * Call the Download method using the COM RPC interface along with the required parameters
 * Verify download method error due to unavailability of internet by asserting that it returns Core::ERROR_UNAVAILABLE
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, downloadMethodsusingComRpcError) {

    initforComRpc();

    getDownloadParams();

    EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type){
                return false;
            }));
    
    // TC-5: Download request error when internet is unavailable using ComRpc
    EXPECT_EQ(Core::ERROR_UNAVAILABLE, pkgdownloaderInterface->Download(uri, options, downloadId));

	deinitforComRpc();   
}

/* Test Case for pausing download via ID using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the download method using the JSON RPC handler, passing the required parameters and wait
 * Verify that the download method is invoked successfully by asserting that it returns Core::ERROR_NONE and checking the downloadId
 * Invoke the pause method using the JSON RPC handler, passing the downloadId
 * Verify that the pause method is invoked successfully by asserting that it returns Core::ERROR_NONE
 * Invoke the cancel method using the JSON RPC handler, passing the downloadId for cancelling download
 * Verify that the cancel method is invoked successfully by asserting that it returns Core::ERROR_NONE
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, pauseMethodusingJsonRpcSuccess) {

    initforJsonRpc();

    EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type) {
                return true;
            }));

    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("download"), _T("{\"url\": \"https://www.examplefile.com/file-download/328\"}"), mJsonRpcResponse));

    waitforSignal(TIMEOUT_FOR_PAUSE);

    EXPECT_NE(mJsonRpcResponse.find("1001"), std::string::npos);

    // TC-6: Pause download via downloadId using JsonRpc
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("pause"), _T("{\"downloadId\": \"1001\"}"), mJsonRpcResponse));

    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("cancel"), _T("{\"downloadId\": \"1001\"}"), mJsonRpcResponse));

	deinitforJsonRpc();
}

/* Test Case for pausing failed using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the pause method using the JSON RPC handler, passing downloadId
 * Verify pause method failure by asserting that it returns Core::ERROR_GENERAL
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, pauseMethodusingJsonRpcFailure) {

    initforJsonRpc();

    // TC-7: Failure in pausing download using JsonRpc
    EXPECT_EQ(Core::ERROR_GENERAL, mJsonRpcHandler.Invoke(connection, _T("pause"), _T("{\"downloadId\": \"1001\"}"), mJsonRpcResponse));

    deinitforJsonRpc();
}

/* Test Case for pausing download via ID using ComRpc
 *
 * Set up and initialize required COM-RPC resources, configurations, notificatios/events, mocks and expectations
 * Obtain the required parameters for downloading using the getDownloadParams()
 * Call the Download method using the COM RPC interface along with the required parameters and wait
 * Verify successful download by asserting that it returns Core::ERROR_NONE and checking the downloadId
 * Call the pause method using the COM RPC interface, passing the downloadId
 * Verify successful pause by asserting that it returns Core::ERROR_NONE
 * Call the cancel method using the COM RPC interface, passing the downloadId for cancelling download
 * Verify successful cancel by asserting that it returns Core::ERROR_NONE
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, pauseMethodusingComRpcSuccess) {

    initforComRpc();

    getDownloadParams();

    EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type) {
                return true;
            }));

    EXPECT_EQ(Core::ERROR_NONE, pkgdownloaderInterface->Download(uri, options, downloadId));

    waitforSignal(TIMEOUT_FOR_PAUSE);

    EXPECT_EQ(downloadId.downloadId, "1001");

    string downloadIdStr = "1001";

    // TC-8: Pause download via downloadId using ComRpc
    EXPECT_EQ(Core::ERROR_NONE, pkgdownloaderInterface->Pause(downloadIdStr));

    EXPECT_EQ(Core::ERROR_NONE, pkgdownloaderInterface->Cancel(downloadIdStr));

	deinitforComRpc();    
}

/* Test Case for pausing failed using ComRpc
 *
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Call the pause method using the COM RPC interface, passing downloadId
 * Verify pause method failure by asserting that it returns Core::ERROR_GENERAL
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, pauseMethodusingComRpcFailure) {

    initforComRpc();

    string downloadId = "1001";

    // TC-9: Failure in pausing download using ComRpc
    EXPECT_EQ(Core::ERROR_GENERAL, pkgdownloaderInterface->Pause(downloadId));

	deinitforComRpc();
}

/* Test Case for resuming download via ID using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the download method using the JSON RPC handler, passing the required parameters and wait
 * Verify that the download method is invoked successfully by asserting that it returns Core::ERROR_NONE and checking the downloadId
 * Invoke the pause method using the JSON RPC handler, passing the downloadId
 * Verify that the pause method is invoked successfully by asserting that it returns Core::ERROR_NONE
 * Invoke the resume method using the JSON RPC handler, passing the downloadId
 * Verify that the resume method is invoked successfully by asserting that it returns Core::ERROR_NONE
 * Invoke the cancel method using the JSON RPC handler, passing the downloadId for cancelling download
 * Verify that the cancel method is invoked successfully by asserting that it returns Core::ERROR_NONE
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, resumeMethodusingJsonRpcSuccess) {

    initforJsonRpc();

    EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type) {
                return true;
            }));

    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("download"), _T("{\"url\": \"https://www.examplefile.com/file-download/328\"}"), mJsonRpcResponse));

    waitforSignal(TIMEOUT_FOR_PAUSE);

    EXPECT_NE(mJsonRpcResponse.find("1001"), std::string::npos);

    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("pause"), _T("{\"downloadId\": \"1001\"}"), mJsonRpcResponse));

    // TC-10: Resume download via downloadId using JsonRpc
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("resume"), _T("{\"downloadId\": \"1001\"}"), mJsonRpcResponse));

    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("cancel"), _T("{\"downloadId\": \"1001\"}"), mJsonRpcResponse));

	deinitforJsonRpc();    
}

 /* Test Case for resuming failed using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the resume method using the JSON RPC handler, passing downloadId
 * Verify resume method failure by asserting that it returns Core::ERROR_GENERAL
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

 TEST_F(PackageManagerTest, resumeMethodusingJsonRpcFailure) {

    initforJsonRpc();

    // TC-11: Failure in resuming download using JsonRpc
    EXPECT_EQ(Core::ERROR_GENERAL, mJsonRpcHandler.Invoke(connection, _T("resume"), _T("{\"downloadId\": \"1001\"}"), mJsonRpcResponse));

	deinitforJsonRpc();
}

 /* Test Case for resuming download via ID using ComRpc
 *
 * Set up and initialize required COM-RPC resources, configurations, notifications/events, mocks and expectations
 * Obtain the required parameters for downloading using the getDownloadParams()
 * Call the Download method using the COM RPC interface along with the required parameters and wait
 * Verify successful download by asserting that it returns Core::ERROR_NONE and checking the downloadId
 * Call the pause method using the COM RPC interface, passing the downloadId
 * Verify successful pause by asserting that it returns Core::ERROR_NONE
 * Call the resume method using the COM RPC interface, passing the downloadId
 * Verify successful resume by asserting that it returns Core::ERROR_NONE
 * Call the cancel method using the COM RPC interface, passing the downloadId for cancelling download
 * Verify successful cancel by asserting that it returns Core::ERROR_NONE
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, resumeMethodusingComRpcSuccess) {

    initforComRpc();

    getDownloadParams();

    EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type) {
                return true;
            }));

   	EXPECT_EQ(Core::ERROR_NONE, pkgdownloaderInterface->Download(uri, options, downloadId));

    waitforSignal(TIMEOUT_FOR_PAUSE);

    EXPECT_EQ(downloadId.downloadId, "1001");

    string downloadIdStr = "1001";
    
    EXPECT_EQ(Core::ERROR_NONE, pkgdownloaderInterface->Pause(downloadIdStr));

    // TC-12: Resume download via downloadId using ComRpc
    EXPECT_EQ(Core::ERROR_NONE, pkgdownloaderInterface->Resume(downloadIdStr));

    EXPECT_EQ(Core::ERROR_NONE, pkgdownloaderInterface->Cancel(downloadIdStr));

    deinitforComRpc();
}

 /* Test Case for resuming failed using ComRpc
 *
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Call the resume method using the COM RPC interface, passing downloadId
 * Verify resume method failure by asserting that it returns Core::ERROR_GENERAL
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

  TEST_F(PackageManagerTest, resumeMethodusingComRpcFailure) {

    initforComRpc();

    string downloadId = "1001";

    // TC-13: Failure in resuming download using ComRpc
    EXPECT_EQ(Core::ERROR_GENERAL, pkgdownloaderInterface->Resume(downloadId));

	deinitforComRpc();
}

/* Test Case for cancelling download via ID using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the download method using the JSON RPC handler, passing the required parameters and wait
 * Verify that the download method is invoked successfully by asserting that it returns Core::ERROR_NONE and checking the downloadId
 * Invoke the pause method using the JSON RPC handler, passing the downloadId
 * Verify that the pause method is invoked successfully by asserting that it returns Core::ERROR_NONE
 * Invoke the cancel method using the JSON RPC handler, passing the downloadId
 * Verify that the cancel method is invoked successfully by asserting that it returns Core::ERROR_NONE
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, cancelMethodusingJsonRpcSuccess) {

    initforJsonRpc();

    EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type) {
                return true;
            }));

    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("download"), _T("{\"url\": \"https://www.examplefile.com/file-download/328\"}"), mJsonRpcResponse));

    waitforSignal(TIMEOUT_FOR_PAUSE);

    EXPECT_NE(mJsonRpcResponse.find("1001"), std::string::npos);

    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("pause"), _T("{\"downloadId\": \"1001\"}"), mJsonRpcResponse));

    // TC-14: Cancel download via downloadId using JsonRpc
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("cancel"), _T("{\"downloadId\": \"1001\"}"), mJsonRpcResponse));
	
    deinitforJsonRpc();
}

/* Test Case for cancelling failed using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the cancel method using the JSON RPC handler, passing downloadId
 * Verify cancel method failure by asserting that it returns Core::ERROR_GENERAL
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

 TEST_F(PackageManagerTest, cancelMethodusingJsonRpcFailure) {

    initforJsonRpc();

    // TC-15: Failure in cancelling download using JsonRpc
    EXPECT_EQ(Core::ERROR_GENERAL, mJsonRpcHandler.Invoke(connection, _T("cancel"), _T("{\"downloadId\": \"1001\"}"), mJsonRpcResponse));

	deinitforJsonRpc();
}

/* Test Case for cancelling download via ID using ComRpc
 *
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Obtain the required parameters for downloading using the getDownloadParams()
 * Call the Download method using the COM RPC interface along with the required parameters and wait
 * Verify successful download by asserting that it returns Core::ERROR_NONE and checking the downloadId
 * Call the pause method using the COM RPC interface, passing the downloadId
 * Verify successful pause by asserting that it returns Core::ERROR_NONE
 * Call the cancel method using the COM RPC interface, passing the downloadId
 * Verify successful cancel by asserting that it returns Core::ERROR_NONE
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, cancelMethodusingComRpcSuccess) {

    initforComRpc();

    getDownloadParams();
    
    EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type) {
                return true;
            }));

    EXPECT_EQ(Core::ERROR_NONE, pkgdownloaderInterface->Download(uri, options, downloadId));

    waitforSignal(TIMEOUT_FOR_PAUSE);

    EXPECT_EQ(downloadId.downloadId, "1001");

    string downloadIdStr = "1001";
    
    EXPECT_EQ(Core::ERROR_NONE, pkgdownloaderInterface->Pause(downloadIdStr));

    // TC-16: Cancel download via downloadId using ComRpc
    EXPECT_EQ(Core::ERROR_NONE, pkgdownloaderInterface->Cancel(downloadIdStr));

	deinitforComRpc();
}

/* Test Case for cancelling failed using ComRpc
 *
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Call the cancel method using the COM RPC interface, passing downloadId
 * Verify cancel method failure by asserting that it returns Core::ERROR_GENERAL
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, cancelMethodusingComRpcFailure) {

    initforComRpc();

    string downloadId = "1001";

    // TC-17: Failure in cancelling download using ComRpc
    EXPECT_EQ(Core::ERROR_GENERAL, pkgdownloaderInterface->Cancel(downloadId));

	deinitforComRpc();
}

/* Test Case for delete download using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, notifications/events, mocks and expectations
 * Invoke the download method using the JSON RPC handler, passing the required parameters
 * Verify that the download method is invoked successfully by asserting that it returns Core::ERROR_NONE and checking the downloadId
 * Invoke the delete method using the JSON RPC handler, passing the fileLocator
 * Verify successful delete by asserting that it returns Core::ERROR_NONE
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, deleteMethodusingJsonRpcSuccess) {

    initforJsonRpc();

    Core::Event onAppDownloadStatus(false, true);

    EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type) {
                return true;
            }));

    EXPECT_CALL(*mServiceMock, Submit(::testing::_, ::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const uint32_t, const Core::ProxyType<Core::JSON::IElement>& json) {
                onAppDownloadStatus.SetEvent();
                return Core::ERROR_NONE;
            }));

    EVENT_SUBSCRIBE(0, _T("onAppDownloadStatus"), _T("org.rdk.PackageManagerRDKEMS"), message);

    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("download"), _T("{\"url\": \"https://httpbin.org/bytes/1024\"}"), mJsonRpcResponse));

    EXPECT_EQ(Core::ERROR_NONE, onAppDownloadStatus.Lock());
    EVENT_UNSUBSCRIBE(0, _T("onAppDownloadStatus"), _T("org.rdk.PackageManagerRDKEMS"), message);

    EXPECT_NE(mJsonRpcResponse.find("1001"), std::string::npos);

    // TC-18: Delete download using JsonRpc
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("delete"), _T("{\"fileLocator\": \"/opt/CDL/package1001\"}"), mJsonRpcResponse));

	deinitforJsonRpc();
}

/* Test Case for delete failed using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the delete method using the JSON RPC handler, passing fileLocator
 * Verify delete method failure by asserting that it returns Core::ERROR_GENERAL
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, deleteMethodusingJsonRpcFailure) {

    initforJsonRpc();

    // TC-19: Failure in delete using JsonRpc
    EXPECT_EQ(Core::ERROR_GENERAL, mJsonRpcHandler.Invoke(connection, _T("delete"), _T("{\"fileLocator\": \"\"}"), mJsonRpcResponse));

	deinitforJsonRpc();
}

/* Test Case for delete download using ComRpc
 *
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Obtain the required parameters for downloading using the getDownloadParams()
 * Call the Download method using the COM RPC interface along with the required parameters and wait
 * Verify successful download by asserting that it returns Core::ERROR_NONE and checking the downloadId
 * Call the delete method using the COM RPC interface, passing fileLocator
 * Verify successful delete by asserting that it returns Core::ERROR_NONE
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, deleteMethodusingComRpcSuccess) {

    initforComRpc();

    getDownloadParams();

    uint32_t timeout_ms = 4000;

    EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type) {
                return true;
            }));

    EXPECT_EQ(Core::ERROR_NONE, pkgdownloaderInterface->Download(uri, options, downloadId));

    waitforSignal(timeout_ms);

    EXPECT_EQ(downloadId.downloadId, "1001");

    string fileLocator = "/opt/CDL/package1001";

    // TC-20: Delete download failure when download in progress using ComRpc
    EXPECT_EQ(Core::ERROR_NONE, pkgdownloaderInterface->Delete(fileLocator));

	deinitforComRpc();
}

/* Test Case for delete download failure using ComRpc
 *
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Call the delete method using the COM RPC interface, passing fileLocator as empty string
 * Verify delete method failure by asserting that it returns Core::ERROR_GENERAL
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, deleteMethodusingComRpcFailure) {

    initforComRpc();

    string fileLocator = "";

    // TC-21: Failure in delete using ComRpc
    EXPECT_EQ(Core::ERROR_GENERAL, pkgdownloaderInterface->Delete(fileLocator));

	deinitforComRpc();
}

/* Test Case for download progress via ID using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the download method using the JSON RPC handler, passing the required parameters and wait
 * Verify that the download method is invoked successfully by asserting that it returns Core::ERROR_NONE and checking the downloadId
 * Invoke the pause method using the JSON RPC handler, passing the downloadId
 * Verify that the pause method is invoked successfully by asserting that it returns Core::ERROR_NONE
 * Invoke the progress method using the JSON RPC handler, passing the downloadId and progress info
 * Verify that the progress method is invoked successfully by asserting that it returns Core::ERROR_NONE and checking that response is not empty string.
 * Invoke the cancel method using the JSON RPC handler, passing the downloadId for cancelling download
 * Verify that the cancel method is invoked successfully by asserting that it returns Core::ERROR_NONE
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, progressMethodusingJsonRpcSuccess) {

    initforJsonRpc();

    EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type) {
                return true;
            }));
            
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("download"), _T("{\"url\": \"https://www.examplefile.com/file-download/328\"}"), mJsonRpcResponse));

    waitforSignal(TIMEOUT_FOR_PAUSE);

    EXPECT_NE(mJsonRpcResponse.find("1001"), std::string::npos);

    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("pause"), _T("{\"downloadId\": \"1001\"}"), mJsonRpcResponse));

    // TC-22: Download progress via downloadId using JsonRpc
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("progress"), _T("{\"downloadId\": \"1001\"}"), mJsonRpcResponse));

    EXPECT_NE(mJsonRpcResponse, "");

    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("cancel"), _T("{\"downloadId\": \"1001\"}"), mJsonRpcResponse));

	deinitforJsonRpc();
}

/* Test Case for download progress failure using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the progress method using the JSON RPC handler, passing downloadId and progress info
 * Verify progress method failure by asserting that it returns Core::ERROR_GENERAL
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, progressMethodusingJsonRpcFailure) {

    initforJsonRpc();

    // TC-23: Download progress failure using JsonRpc
    EXPECT_EQ(Core::ERROR_GENERAL, mJsonRpcHandler.Invoke(connection, _T("progress"), _T("{\"downloadId\": \"1001\"}"), mJsonRpcResponse));

	deinitforJsonRpc();
}

/* Test Case for download progress via ID using ComRpc
 *
 * Set up and initialize required COM-RPC resources, configurations, notifications/events, mocks and expectations
 * Obtain the required parameters for downloading using the getDownloadParams()
 * Call the Download method using the COM RPC interface along with the required parameters and wait
 * Verify successful download by asserting that it returns Core::ERROR_NONE and checking the downloadId
 * Call the pause method using the COM RPC interface along with downloadId
 * Verify successful pause by asserting that it returns Core::ERROR_NONE
 * Call the progress method using the COM RPC interface, passing the downloadId and progress info
 * Verify successful progress by asserting that it returns Core::ERROR_NONE and checking that progress is non-zero
 * Call the cancel method using the COM RPC interface, passing the downloadId for cancelling download
 * Verify successful cancel by asserting that it returns Core::ERROR_NONE
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

 TEST_F(PackageManagerTest, progressMethodusingComRpcSuccess) {

    initforComRpc();

    getDownloadParams();
    
    EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type) {
                return true;
            }));

    progress = {};

    EXPECT_EQ(Core::ERROR_NONE, pkgdownloaderInterface->Download(uri, options, downloadId));

    waitforSignal(TIMEOUT_FOR_PAUSE);

    EXPECT_EQ(downloadId.downloadId, "1001");

    string downloadIdStr = "1001";

    EXPECT_EQ(Core::ERROR_NONE, pkgdownloaderInterface->Pause(downloadIdStr));

    // TC-24: Download progress via downloadId using ComRpc
    EXPECT_EQ(Core::ERROR_NONE, pkgdownloaderInterface->Progress(downloadIdStr, progress));

    EXPECT_EQ(Core::ERROR_NONE, pkgdownloaderInterface->Cancel(downloadIdStr));
    
	deinitforComRpc();
}

/* Test Case for download progress failure using ComRpc
 *
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Call the progress method using the COM RPC interface, passing downloadId and progress info
 * Verify progress method failure by asserting that it returns Core::ERROR_GENERAL
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, progressMethodusingComRpcFailure) {

    initforComRpc();

    progress = {};

    string downloadId = "1001";

    // TC-25: Progress failure via downloadId using ComRpc
    EXPECT_EQ(Core::ERROR_GENERAL, pkgdownloaderInterface->Progress(downloadId, progress));

	deinitforComRpc();
}

/* Test Case for getting storage information using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the getStorageInformation method using the JSON RPC handler, passing required parameters
 * Verify getStorageInformation method success by asserting that it returns Core::ERROR_NONE
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, getStorageInformationusingJsonRpc) {

    initforJsonRpc();

    // TC-26: Get Storage Details using JsonRpc
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("getStorageInformation"), _T("{\"quotaKB\": 1024, \"usedKB\": 568}"), mJsonRpcResponse));

	deinitforJsonRpc();
}

/* Test Case for getting storage information using ComRpc
 *
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Call the getStorageInformation method using the COM RPC interface, passing required parameters
 * Verify getStorageInformation method success by asserting that it returns Core::ERROR_NONE
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, getStorageInformationusingComRpc) {

    initforComRpc();

    uint32_t quotaKB = 1024;
    uint32_t usedKB = 568;

    // TC-27: Get Storage Details using ComRpc
    EXPECT_EQ(Core::ERROR_NONE, pkgdownloaderInterface->GetStorageInformation(quotaKB, usedKB));

	deinitforComRpc();
}

/* Test Case for setting rate limit via ID using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the download method using the JSON RPC handler, passing the required parameters and wait
 * Verify that the download method is invoked successfully by asserting that it returns Core::ERROR_NONE and checking the downloadId
 * Invoke the pause method using the JSON RPC handler, passing the downloadId
 * Verify that the pause method is invoked successfully by asserting that it returns Core::ERROR_NONE
 * Invoke the rateLimit method using the JSON RPC handler, passing the downloadId and the limit
 * Verify that the rateLimit method is invoked successfully by asserting that it returns Core::ERROR_NONE
 * Invoke the cancel method using the JSON RPC handler, passing the downloadId for cancelling download
 * Verify that the cancel method is invoked successfully by asserting that it returns Core::ERROR_NONE
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, rateLimitusingJsonRpcSuccess) {

    initforJsonRpc();

    EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type) {
                return true;
            }));

    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("download"), _T("{\"url\": \"https://www.examplefile.com/file-download/328\"}"), mJsonRpcResponse));

    waitforSignal(TIMEOUT_FOR_PAUSE);

    EXPECT_NE(mJsonRpcResponse.find("1001"), std::string::npos);

    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("pause"), _T("{\"downloadId\": \"1001\"}"), mJsonRpcResponse));

    // TC-28: Set rate limit via downloadID using JsonRpc
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("rateLimit"), _T("{\"downloadId\": \"1001\", \"limit\": 1024}"), mJsonRpcResponse));

    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("cancel"), _T("{\"downloadId\": \"1001\"}"), mJsonRpcResponse));

	deinitforJsonRpc();    
}

 /* Test Case for setting rate limit failure using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the rateLimit method using the JSON RPC handler, passing downloadId and limit
 * Verify rateLimit method failure by asserting that it returns Core::ERROR_GENERAL
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, rateLimitusingJsonRpcFailure) {

    initforJsonRpc();

    // TC-29: Rate limit failure using JsonRpc
    EXPECT_EQ(Core::ERROR_GENERAL, mJsonRpcHandler.Invoke(connection, _T("rateLimit"), _T("{\"downloadId\": \"1001\", \"limit\": 1024}"), mJsonRpcResponse));

	deinitforJsonRpc();
}

/* Test Case for setting rate limit via ID using ComRpc
 *
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Obtain the required parameters for downloading using the getDownloadParams()
 * Call the Download method using the COM RPC interface along with the required parameters and wait
 * Verify successful download by asserting that it returns Core::ERROR_NONE and checking the downloadId
 * Call the Pause method using the COM RPC interface along with downloadId
 * Verify successful pause by asserting that it returns Core::ERROR_NONE
 * Call the rateLimit method using the COM RPC interface, passing the downloadId and limit
 * Verify rateLimit is set successfully by asserting that it returns Core::ERROR_NONE
 * Call the cancel method using the COM RPC interface, passing the downloadId for cancelling download
 * Verify successful cancel by asserting that it returns Core::ERROR_NONE
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, rateLimitusingComRpcSuccess) {

    initforComRpc();

    getDownloadParams();
    
    EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type) {
                return true;
            }));

    uint64_t limit = 1024;

    EXPECT_EQ(Core::ERROR_NONE, pkgdownloaderInterface->Download(uri, options, downloadId));

    waitforSignal(TIMEOUT_FOR_PAUSE);

    EXPECT_EQ(downloadId.downloadId, "1001");

    string downloadIdStr = "1001";

    EXPECT_EQ(Core::ERROR_NONE, pkgdownloaderInterface->Pause(downloadIdStr));

    // TC-30: Set rate limit via downloadID using ComRpc
    EXPECT_EQ(Core::ERROR_NONE, pkgdownloaderInterface->RateLimit(downloadIdStr, limit));

    EXPECT_EQ(Core::ERROR_NONE, pkgdownloaderInterface->Cancel(downloadIdStr));
    
	deinitforComRpc();
}

/* Test Case for failure in setting rateLimit using ComRpc
 *
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Call the rateLimit method using the COM RPC interface, passing downloadId and limit
 * Verify rateLimit method failure by asserting that it returns Core::ERROR_GENERAL
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

 TEST_F(PackageManagerTest, rateLimitusingComRpcFailure) {

    initforComRpc();

    uint64_t limit = 1024;
    string downloadId = "1001";

    // TC-31: Rate limit failure using ComRpc
    EXPECT_EQ(Core::ERROR_GENERAL, pkgdownloaderInterface->RateLimit(downloadId, limit));

	deinitforComRpc();
}

// IPackageInstaller methods

/* Test Case for error on install due to invalid signature using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the install method using the JSON RPC handler, passing the required parameters, keeping the file locator field empty
 * Verify that the install method fails by asserting that it returns Core::ERROR_INVALID_SIGNATURE
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, installusingJsonRpcInvalidSignature) {

    initforJsonRpc();

	waitforSignal(TIMEOUT_FOR_INIT);
    
    // TC-32: Error on install due to invalid signature using JsonRpc
    EXPECT_EQ(Core::ERROR_INVALID_SIGNATURE, mJsonRpcHandler.Invoke(connection, _T("install"), _T("{\"packageId\": \"YouTube\", \"version\": \"100.1.24\", \"additionalMetadata\": [{\"name\": \"testApp\", \"value\": \"2\"}], \"fileLocator\": \"\"}"), mJsonRpcResponse));

	deinitforJsonRpc();
}

/* Test Case for install success using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the download method using the JSON RPC handler, passing the required parameters, verify successful download and wait 
 * Invoke the install method using the JSON RPC handler, passing the required parameters
 * Verify successful install by asserting that it returns Core::ERROR_NONE
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, installusingJsonRpcSuccess) {

    initforJsonRpc();

    waitforSignal(TIMEOUT_FOR_INIT);

	EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillRepeatedly(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type) {
                return true;
        	}));

	EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("download"), _T("{\"url\": \"https://httpbin.org/bytes/1024\"}"), mJsonRpcResponse));

	waitforSignal(TIMEOUT);
	
    // TC-33: Install using JsonRpc
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("install"), _T("{\"packageId\": \"YouTube\", \"version\": \"100.1.24\", \"additionalMetadata\": [{\"name\": \"testApp\", \"value\": \"2\"}], \"fileLocator\": \"/opt/CDL/package1001\"}"), mJsonRpcResponse));

	deinitforJsonRpc();
}

 /* Test Case for error on install due to invalid signature using ComRpc
 *
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Call the install method using the COM RPC interface, passing required parameters, keeping the fileLocator parameter as empty and wait
 * Verify error on install by asserting that it returns Core::ERROR_INVALID_SIGNATURE
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, installusingComRpcInvalidSignature) {

    initforComRpc();

    string packageId = "YouTube";
    string version = "100.1.24";
    string fileLocator = "";
    Exchange::IPackageInstaller::FailReason reason = Exchange::IPackageInstaller::FailReason::NONE;
    list<Exchange::IPackageInstaller::KeyValue> kv = { {"testapp", "2"} };

    auto additionalMetadata = Core::Service<RPC::IteratorType<Exchange::IPackageInstaller::IKeyValueIterator>>::Create<Exchange::IPackageInstaller::IKeyValueIterator>(kv);

	waitforSignal(TIMEOUT_FOR_INIT);

    // TC-34: Error on install due to invalid signature using ComRpc
    EXPECT_EQ(Core::ERROR_INVALID_SIGNATURE, pkginstallerInterface->Install(packageId, version, additionalMetadata, fileLocator, reason));

	deinitforComRpc();
}

/* Test Case for install success using ComRpc
 * 
 * Set up and initialize required COM-RPC resources, configurations, notifications/events, mocks and expectations
 * Call the download method using the COM-RPC interface, passing required parameters for download, verify and wait
 * Call the install method using the COM RPC interface, passing required parameters and wait
 * Verify successful install by asserting that it returns Core::ERROR_NONE
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

 TEST_F(PackageManagerTest, installusingComRpcSuccess) {

    initforComRpc();

	getDownloadParams();

	uri = "https://httpbin.org/bytes/1024";

    uint32_t timeout_ms = 3000;

    string packageId = "YouTube";
    string version = "100.1.24";
    string fileLocator = "/opt/CDL/package1001";
    Exchange::IPackageInstaller::FailReason reason = Exchange::IPackageInstaller::FailReason::NONE;
    list<Exchange::IPackageInstaller::KeyValue> kv = { {"testapp", "2"} };

    auto additionalMetadata = Core::Service<RPC::IteratorType<Exchange::IPackageInstaller::IKeyValueIterator>>::Create<Exchange::IPackageInstaller::IKeyValueIterator>(kv);

	EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillRepeatedly(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type) {
                return true;
        	}));

    EXPECT_CALL(*mStorageManagerMock, CreateStorage(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string& appId, const uint32_t &size, string& path, string &errorReason) {
                return Core::ERROR_NONE;
            }));

    waitforSignal(TIMEOUT_FOR_INIT);

	EXPECT_EQ(Core::ERROR_NONE, pkgdownloaderInterface->Download(uri, options, downloadId));

	EXPECT_EQ(downloadId.downloadId, "1001");

 	waitforSignal(TIMEOUT);
	 
    // TC-35: Install using ComRpc
    EXPECT_EQ(Core::ERROR_NONE, pkginstallerInterface->Install(packageId, version, additionalMetadata, fileLocator, reason));

    waitforSignal(timeout_ms);

	deinitforComRpc();   
}

/* Test Case for uninstall success using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the download method using the JSON RPC handler, passing the required parameters, verify successful download and wait
 * Invoke the install method using the JSON RPC handler, passing the required parameters and wait
 * Verify successful install by asserting that it returns Core::ERROR_NONE
 * Invoke the uninstall method using the JSON RPC handler, passing the required parameters
 * Verify successful uninstall by asserting that it returns Core::ERROR_NONE
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, uninstallusingJsonRpcSuccess) {

    initforJsonRpc();

    waitforSignal(TIMEOUT_FOR_INIT);

	EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillRepeatedly(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type) {
                return true;
        	}));

	EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("download"), _T("{\"url\": \"https://httpbin.org/bytes/1024\"}"), mJsonRpcResponse));

	waitforSignal(TIMEOUT);

    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("install"), _T("{\"packageId\": \"YouTube\", \"version\": \"100.1.24\", \"additionalMetadata\": [{\"name\": \"testApp\", \"value\": \"2\"}], \"fileLocator\": \"/opt/CDL/package1001\"}"), mJsonRpcResponse));

	waitforSignal(TIMEOUT_FOR_INSTALL);
	
    // TC-36: Uninstall using JsonRpc
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("uninstall"), _T("{\"packageId\": \"YouTube\"}"), mJsonRpcResponse));

	deinitforJsonRpc();
}

/* Test Case for uninstall success using ComRpc
 * 
 * Set up and initialize required COM-RPC resources, configurations, notifications/events, mocks and expectations
 * Call the download method using the COM-RPC interface, passing required parameters for download, verify and wait
 * Call the install method using the COM RPC interface, passing required parameters and wait 
 * Verify successful install by asserting that it returns Core::ERROR_NONE
 * Call the uninstall method using the COM RPC interface, passing required parameters and wait 
 * Verify successful uninstall by asserting that it returns Core::ERROR_NONE
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, uninstallusingComRpcSuccess) {

    initforComRpc();

	getDownloadParams();

	uri = "https://httpbin.org/bytes/1024";

    uint32_t timeout_ms = 3000;

    string packageId = "YouTube";
    string errorReason = "no error";
    string version = "100.1.24";
    string fileLocator = "/opt/CDL/package1001";
    Exchange::IPackageInstaller::FailReason reason = Exchange::IPackageInstaller::FailReason::NONE;
    list<Exchange::IPackageInstaller::KeyValue> kv = { {"testapp", "2"} };

    auto additionalMetadata = Core::Service<RPC::IteratorType<Exchange::IPackageInstaller::IKeyValueIterator>>::Create<Exchange::IPackageInstaller::IKeyValueIterator>(kv);

	EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillRepeatedly(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type) {
                return true;
        	}));
    
    EXPECT_CALL(*mStorageManagerMock, CreateStorage(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string& appId, const uint32_t &size, string& path, string &errorReason) {
                return Core::ERROR_NONE;
            }));

    EXPECT_CALL(*mStorageManagerMock, DeleteStorage(::testing::_, ::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string& appId, string &errorReason) {
                return Core::ERROR_NONE;
            }));

    waitforSignal(TIMEOUT_FOR_INIT);

	EXPECT_EQ(Core::ERROR_NONE, pkgdownloaderInterface->Download(uri, options, downloadId));

	EXPECT_EQ(downloadId.downloadId, "1001");

	waitforSignal(TIMEOUT);

    EXPECT_EQ(Core::ERROR_NONE, pkginstallerInterface->Install(packageId, version, additionalMetadata, fileLocator, reason));

    waitforSignal(timeout_ms);

	// TC-37: Uninstall using ComRpc
    EXPECT_EQ(Core::ERROR_NONE, pkginstallerInterface->Uninstall(packageId, errorReason));
    
	waitforSignal(timeout_ms);

	deinitforComRpc();
}

/* Test Case for list packages method success using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the download method using the JSON RPC handler, passing the required parameters, verify successful download and wait
 * Invoke the install method using the JSON RPC handler, passing the required parameters and wait
 * Verify successful install by asserting that it returns Core::ERROR_NONE
 * Invoke the listPackages method using the JSON RPC handler, passing the required parameters
 * Verify that the listPackages method is successful by asserting that it returns Core::ERROR_NONE
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, listPackagesusingJsonRpcSuccess) {

    initforJsonRpc();

	EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillRepeatedly(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type) {
                return true;
        	}));

	waitforSignal(TIMEOUT_FOR_INIT);
	
	EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("download"), _T("{\"url\": \"https://httpbin.org/bytes/1024\"}"), mJsonRpcResponse));

	waitforSignal(TIMEOUT);

    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("install"), _T("{\"packageId\": \"YouTube\", \"version\": \"100.1.24\", \"additionalMetadata\": [{\"name\": \"testApp\", \"value\": \"2\"}], \"fileLocator\": \"/opt/CDL/package1001\"}"), mJsonRpcResponse));

	waitforSignal(TIMEOUT_FOR_INSTALL);
	
	// TC-38: list packages using JsonRpc
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("listPackages"), _T("{\"packages\": {}}"), mJsonRpcResponse));

	EXPECT_NE(mJsonRpcResponse, "");

	deinitforJsonRpc();   
}

/* Test Case for list packages method success using ComRpc
 * 
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Call the download method using the COM-RPC interface, passing required parameters for download, verify and wait
 * Call the install method using the COM RPC interface, passing required parameters and wait 
 * Verify successful install by asserting that it returns Core::ERROR_NONE
 * Call the ListPackages method using the COM RPC interface, passing the required parameters
 * Verify that the ListPackages method is successful by asserting that it returns Core::ERROR_NONE
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, listPackagesusingComRpcSuccess) {

    initforComRpc();

	getDownloadParams();

	uri = "https://httpbin.org/bytes/1024";

	string packageId = "YouTube";
	string version = "100.1.24";
    string fileLocator = "/opt/CDL/package1001";
    Exchange::IPackageInstaller::FailReason reason = Exchange::IPackageInstaller::FailReason::NONE;
	list<Exchange::IPackageInstaller::KeyValue> kv = { {"testapp", "2"} };
	
    list<Exchange::IPackageInstaller::Package> packageList = { {} };

    auto packages = Core::Service<RPC::IteratorType<Exchange::IPackageInstaller::IPackageIterator>>::Create<Exchange::IPackageInstaller::IPackageIterator>(packageList);

	auto additionalMetadata = Core::Service<RPC::IteratorType<Exchange::IPackageInstaller::IKeyValueIterator>>::Create<Exchange::IPackageInstaller::IKeyValueIterator>(kv);

	EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillRepeatedly(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type) {
                return true;
        	}));

    EXPECT_CALL(*mStorageManagerMock, CreateStorage(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string& appId, const uint32_t &size, string& path, string &errorReason) {
                return Core::ERROR_NONE;
            }));

	waitforSignal(TIMEOUT_FOR_INIT);

	EXPECT_EQ(Core::ERROR_NONE, pkgdownloaderInterface->Download(uri, options, downloadId));

	EXPECT_EQ(downloadId.downloadId, "1001");

	waitforSignal(TIMEOUT);

    EXPECT_EQ(Core::ERROR_NONE, pkginstallerInterface->Install(packageId, version, additionalMetadata, fileLocator, reason));

    waitforSignal(TIMEOUT_FOR_INSTALL);

	// TC-39: list packages using ComRpc
    EXPECT_EQ(Core::ERROR_NONE, pkginstallerInterface->ListPackages(packages));

	deinitforComRpc();
}

/* Test Case for package state failure using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the download method using the JSON RPC handler, passing the required parameters, verify successful download and wait
 * Invoke the install method using the JSON RPC handler, passing the required parameters and wait
 * Verify successful install by asserting that it returns Core::ERROR_NONE
 * Invoke the packageState method using the JSON RPC handler, passing the required parameters
 * Verify packageState method success by asserting that it returns Core::ERROR_NONE 
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, packageStateusingJsonRpcSuccess) {

    initforJsonRpc();

	EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillRepeatedly(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type) {
                return true;
        	}));

	waitforSignal(TIMEOUT_FOR_INIT);

	EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("download"), _T("{\"url\": \"https://httpbin.org/bytes/1024\"}"), mJsonRpcResponse));

	waitforSignal(TIMEOUT);

    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("install"), _T("{\"packageId\": \"YouTube\", \"version\": \"100.1.24\", \"additionalMetadata\": [{\"name\": \"testApp\", \"value\": \"2\"}], \"fileLocator\": \"/opt/CDL/package1001\"}"), mJsonRpcResponse));

	waitforSignal(TIMEOUT_FOR_INSTALL);
	
    // TC-40: Package state using JsonRpc
    EXPECT_EQ(Core::ERROR_NONE, mJsonRpcHandler.Invoke(connection, _T("packageState"), _T("{\"packageId\": \"YouTube\", \"version\": \"100.1.24\"}"), mJsonRpcResponse));

	deinitforJsonRpc();
}

/* Test Case for package state failure using ComRpc
 * 
 * Set up and initialize required COM-RPC resources, configurations, notifications/events, mocks and expectations
 * Call the download method using the COM-RPC interface, passing required parameters for download, verify and wait
 * Call the install method using the COM RPC interface, passing the required parameters and wait 
 * Verify successful install by asserting that it returns Core::ERROR_NONE
 * Call the PackageState method using the COM RPC interface, passing the required parameters and wait
 * Verify package state method success by asserting that it returns Core::ERROR_NONE and state is 3 - INSTALLED
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, packageStateusingComRpcSuccess) {

    initforComRpc();

	getDownloadParams();

	uri = "https://httpbin.org/bytes/1024";

    uint32_t timeout_ms = 3000;

    string packageId = "YouTube";
    string version = "100.1.24";
    string fileLocator = "/opt/CDL/package1001";
    Exchange::IPackageInstaller::FailReason reason = Exchange::IPackageInstaller::FailReason::NONE;
    list<Exchange::IPackageInstaller::KeyValue> kv = { {"testapp", "2"} };
    Exchange::IPackageInstaller::InstallState state = Exchange::IPackageInstaller::InstallState::INSTALLING;

    auto additionalMetadata = Core::Service<RPC::IteratorType<Exchange::IPackageInstaller::IKeyValueIterator>>::Create<Exchange::IPackageInstaller::IKeyValueIterator>(kv);

	EXPECT_CALL(*mSubSystemMock, IsActive(::testing::_))
        .Times(::testing::AnyNumber())
        .WillRepeatedly(::testing::Invoke(
            [&](const PluginHost::ISubSystem::subsystem type) {
                return true;
        	}));
	
    EXPECT_CALL(*mStorageManagerMock, CreateStorage(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(::testing::AnyNumber())
        .WillOnce(::testing::Invoke(
            [&](const string& appId, const uint32_t &size, string& path, string &errorReason) {
                return Core::ERROR_NONE;
            }));

	waitforSignal(TIMEOUT_FOR_INIT);

	EXPECT_EQ(Core::ERROR_NONE, pkgdownloaderInterface->Download(uri, options, downloadId));

	EXPECT_EQ(downloadId.downloadId, "1001");

	waitforSignal(TIMEOUT);	

    EXPECT_EQ(Core::ERROR_NONE, pkginstallerInterface->Install(packageId, version, additionalMetadata, fileLocator, reason));

    waitforSignal(timeout_ms);

    // TC-41: Package state using ComRpc
    EXPECT_EQ(Core::ERROR_NONE, pkginstallerInterface->PackageState(packageId, version, state));

	EXPECT_EQ(static_cast<int>(state), 3);

	timeout_ms = 1000;
    waitforSignal(timeout_ms);

	deinitforComRpc();
}

// IPackageHandler methods

/* Test Case for unlock error using JsonRpc
 * 
 * Set up and initialize required JSON-RPC resources, configurations, mocks and expectations
 * Invoke the unlock method using the JSON RPC handler, passing the required parameters
 * Verify unlock method failure by asserting that it returns Core::ERROR_GENERAL
 * Deinitialize the JSON-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, unlockmethodusingJsonRpcFailure) {

    initforJsonRpc();

    waitforSignal(TIMEOUT_FOR_INIT);

	// TC-42: Failure on unlock using JsonRpc
    EXPECT_EQ(Core::ERROR_GENERAL, mJsonRpcHandler.Invoke(connection, _T("unlock"), _T("{\"packageId\": \"YouTube\", \"version\": \"100.1.24\"}"), mJsonRpcResponse));

	deinitforJsonRpc();
}

/* Test Case for unlock failure using ComRpc
 * 
 * Set up and initialize required COM-RPC resources, configurations, mocks and expectations
 * Call the Unlock method using the COM RPC interface, passing required parameters
 * Verify Unlock method failure by asserting that it returns Core::ERROR_GENERAL
 * Deinitialize the COM-RPC resources and clean-up related test resources
 */

TEST_F(PackageManagerTest, unlockmethodusingComRpcFailure) {

    initforComRpc();

    string packageId = "YouTube";
    string version = "100.1.24";

	waitforSignal(TIMEOUT_FOR_INIT);

    // TC-43: Failure on unlock using ComRpc
    EXPECT_EQ(Core::ERROR_GENERAL, pkghandlerInterface->Unlock(packageId, version));

	deinitforComRpc();
}
