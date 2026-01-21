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
#include <interfaces/IAppManager.h>
#include <interfaces/IStore2.h>
#include <interfaces/IConfiguration.h>
#include <interfaces/IAppPackageManager.h>
#include <interfaces/IStorageManager.h>
#include "tracing/Logging.h"
#include <com/com.h>
#include <core/core.h>
#include <plugins/plugins.h>
#include "LifecycleInterfaceConnector.h"
#include <interfaces/IPackageManager.h>
#include <map>

namespace WPEFramework {
namespace Plugin {

    class AppManagerImplementation : public Exchange::IAppManager, public Exchange::IConfiguration {

    public:
        AppManagerImplementation();
        ~AppManagerImplementation() override;

        static AppManagerImplementation* getInstance();
        AppManagerImplementation(const AppManagerImplementation&) = delete;
        AppManagerImplementation& operator=(const AppManagerImplementation&) = delete;

        BEGIN_INTERFACE_MAP(AppManagerImplementation)
        INTERFACE_ENTRY(Exchange::IAppManager)
        INTERFACE_ENTRY(Exchange::IConfiguration)
        END_INTERFACE_MAP

    public:
        typedef enum _ApplicationType
        {
            APPLICATION_TYPE_UNKNOWN = 0,
            APPLICATION_TYPE_INTERACTIVE,
            APPLICATION_TYPE_SYSTEM
        } ApplicationType;

        typedef struct _PackageInfo
        {
            string version = "";
            uint32_t lockId = 0;
            string unpackedPath = "" ;
            WPEFramework::Exchange::RuntimeConfig configMetadata;
            string appMetadata = "";
            ApplicationType type;
        } PackageInfo;

        enum CurrentAction {
            APP_ACTION_NONE,
            APP_ACTION_LAUNCH,
            APP_ACTION_PRELOAD,
            APP_ACTION_SUSPEND,
            APP_ACTION_RESUME,
            APP_ACTION_CLOSE,
            APP_ACTION_TERMINATE,
            APP_ACTION_HIBERNATE,
            APP_ACTION_KILL,
        };

#ifdef ENABLE_AIMANAGERS_TELEMETRY_METRICS
        enum CurrentActionError{
            ERROR_NONE,
            ERROR_INVALID_PARAMS,
            ERROR_INTERNAL,
            ERROR_PACKAGE_LOCK,
            ERROR_PACKAGE_UNLOCK,
            ERROR_PACKAGE_LIST_FETCH,
            ERROR_PACKAGE_NOT_INSTALLED,
            ERROR_PACKAGE_INVALID,
            ERROR_SPAWN_APP,
            ERROR_UNLOAD_APP,
            ERROR_KILL_APP,
            ERROR_SET_TARGET_APP_STATE
        };
#endif

        typedef struct _AppInfo
        {
            /* From LifecycleManager */
            string appInstanceId;
            string activeSessionId;
            /* From PackageManager */
            PackageInfo packageInfo;
            /* App launch params*/
            Exchange::IAppManager::AppLifecycleState appNewState;
            Exchange::ILifecycleManager::LifecycleState appLifecycleState;
            timespec lastActiveStateChangeTime;
            uint32_t lastActiveIndex;
            string appIntent;
            Exchange::IAppManager::AppLifecycleState targetAppState;
            Exchange::IAppManager::AppLifecycleState appOldState;
            /* Current Action*/
            CurrentAction currentAction = APP_ACTION_NONE;
#ifdef ENABLE_AIMANAGERS_TELEMETRY_METRICS
            time_t currentActionTime;
#endif
        } AppInfo;

        std::map<std::string, AppInfo> mAppInfo;

        enum EventNames {
            APP_EVENT_UNKNOWN = 0,
            APP_EVENT_LIFECYCLE_STATE_CHANGED,
            APP_EVENT_INSTALLATION_STATUS,
            APP_EVENT_LAUNCH_REQUEST,
            APP_EVENT_UNLOADED
        };

        struct AppLaunchRequestParam{
            string appId;
            string launchArgs;
            string intent;
        };

        struct AppManagerRequest{
            CurrentAction mRequestAction;
            std::shared_ptr<void> mRequestParam;
        };

        private:
        class PackageManagerNotification : public Exchange::IPackageInstaller::INotification {

        public:
            PackageManagerNotification(AppManagerImplementation& parent) : mParent(parent){}
            ~PackageManagerNotification(){}

        void OnAppInstallationStatus(const string& jsonresponse)
        {
            LOGINFO("AppManagerImplementation on OnAppInstallationStatus");
            mParent.OnAppInstallationStatus(jsonresponse);
        }

        BEGIN_INTERFACE_MAP(PackageManagerNotification)
        INTERFACE_ENTRY(Exchange::IPackageInstaller::INotification)
        END_INTERFACE_MAP

        private:
            AppManagerImplementation& mParent;
        };

        class EXTERNAL Job : public Core::IDispatch {
        protected:
             Job(AppManagerImplementation *appManagerImplementation, EventNames event, JsonObject &params)
                : mAppManagerImplementation(appManagerImplementation)
                , _event(event)
                , _params(params) {
                if (mAppManagerImplementation != nullptr) {
                    mAppManagerImplementation->AddRef();
                }
            }

       public:
            Job() = delete;
            Job(const Job&) = delete;
            Job& operator=(const Job&) = delete;
            ~Job() {
                if (mAppManagerImplementation != nullptr) {
                    mAppManagerImplementation->Release();
                }
            }

       public:
            static Core::ProxyType<Core::IDispatch> Create(AppManagerImplementation *appManagerImplementation, EventNames event, JsonObject params) {
#ifndef USE_THUNDER_R4
                return (Core::proxy_cast<Core::IDispatch>(Core::ProxyType<Job>::Create(appManagerImplementation, event, params)));
#else
                return (Core::ProxyType<Core::IDispatch>(Core::ProxyType<Job>::Create(appManagerImplementation, event, params)));
#endif
            }

            virtual void Dispatch() {
                mAppManagerImplementation->Dispatch(_event, _params);
            }
        private:
            AppManagerImplementation *mAppManagerImplementation;
            const EventNames _event;
            const JsonObject _params;
        };

    public:
        Core::hresult Register(Exchange::IAppManager::INotification *notification) override ;
        Core::hresult Unregister(Exchange::IAppManager::INotification *notification) override ;
        Core::hresult LaunchApp(const string& appId , const string& intent , const string& launchArgs) override;
        Core::hresult CloseApp(const string& appId) override;
        Core::hresult TerminateApp(const string& appId) override;
        Core::hresult KillApp(const string& appId) override;
        Core::hresult GetLoadedApps(Exchange::IAppManager::ILoadedAppInfoIterator*& appData) override;
        Core::hresult SendIntent(const string& appId , const string& intent) override;
        Core::hresult PreloadApp(const string& appId , const string& launchArgs ,string& error) override;
        Core::hresult GetAppProperty(const string& appId , const string& key , string& value) override;
        Core::hresult SetAppProperty(const string& appId , const string& key ,const string& value) override;
        Core::hresult GetInstalledApps(string& apps) override;
        Core::hresult IsInstalled(const string& appId, bool& installed) override;
        Core::hresult StartSystemApp(const string& appId) override;
        Core::hresult StopSystemApp(const string& appId) override;
        Core::hresult ClearAppData(const string& appId) override;
        Core::hresult ClearAllAppData() override;
        Core::hresult GetAppMetadata(const string& appId, const string& metaData, string& result) override;
        Core::hresult GetMaxRunningApps(int32_t& maxRunningApps) const override;
        Core::hresult GetMaxHibernatedApps(int32_t& maxHibernatedApps) const override;
        Core::hresult GetMaxHibernatedFlashUsage(int32_t& maxHibernatedFlashUsage) const override;
        Core::hresult GetMaxInactiveRamUsage(int32_t& maxInactiveRamUsage) const override;
        void handleOnAppLifecycleStateChanged(const string& appId, const string& appInstanceId, const Exchange::IAppManager::AppLifecycleState newState,
                                        const Exchange::IAppManager::AppLifecycleState oldState, const Exchange::IAppManager::AppErrorReason errorReason);
        void handleOnAppUnloaded(const string& appId, const string& appInstanceId);
        void handleOnAppLaunchRequest(const string& appId, const string& intent, const string& source);
        std::string getInstallAppType(ApplicationType type);

        // IConfiguration methods
        uint32_t Configure(PluginHost::IShell* service) override;

    private: /* private methods */
        Core::hresult createPersistentStoreRemoteStoreObject();
        void releasePersistentStoreRemoteStoreObject();
        Core::hresult createPackageManagerObject();
        void releasePackageManagerObject();
        void getCustomValues(WPEFramework::Exchange::RuntimeConfig& runtimeConfig);
        Core::hresult createStorageManagerRemoteObject();
        void releaseStorageManagerRemoteObject();
    private:
        mutable Core::CriticalSection mAdminLock;
        std::list<Exchange::IAppManager::INotification*> mAppManagerNotification;
        LifecycleInterfaceConnector* mLifecycleInterfaceConnector;
        Exchange::IStore2* mPersistentStoreRemoteStoreObject;
        Exchange::IPackageHandler* mPackageManagerHandlerObject;
        Exchange::IPackageInstaller* mPackageManagerInstallerObject;
        Exchange::IStorageManager* mStorageManagerRemoteObject;
        PluginHost::IShell* mCurrentservice;
        Core::Sink<PackageManagerNotification> mPackageManagerNotification;
        std::thread mAppManagerWorkerThread;
        std::mutex mAppManagerLock;
        std::condition_variable mAppRequestListCV;
        std::list<std::shared_ptr<AppManagerRequest>> mAppRequestList;
        Core::hresult fetchAppPackageList(std::vector<WPEFramework::Exchange::IPackageInstaller::Package>& packageList);
        void checkIsInstalled(const std::string& appId, bool& installed, const std::vector<WPEFramework::Exchange::IPackageInstaller::Package>& packageList);
        Core::hresult packageLock(const string& appId, PackageInfo &packageData, Exchange::IPackageHandler::LockReason lockReason);
        Core::hresult packageUnLock(const string& appId);
        bool createOrUpdatePackageInfoByAppId(const string& appId, PackageInfo &packageData);
        bool removeAppInfoByAppId(const string &appId);
        void OnAppInstallationStatus(const string& jsonresponse);
        void  AppManagerWorkerThread(void);
        void dispatchEvent(EventNames, const JsonObject &params);
        void Dispatch(EventNames event, const JsonObject params);
        friend class Job;

    public/*members*/:
        static AppManagerImplementation* _instance;

    public: /* public methods */
        void updateCurrentAction(const std::string& appId, CurrentAction action);
#ifdef ENABLE_AIMANAGERS_TELEMETRY_METRICS
        void updateCurrentActionTime(const std::string& appId, time_t currentActionTime, CurrentAction currentAction);
#endif
    bool checkInstallUninstallBlock(const std::string& appId);
    };
} /* namespace Plugin */
} /* namespace WPEFramework */
