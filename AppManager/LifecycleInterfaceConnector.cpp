/**
* If not stated otherwise in this file or this component's LICENSE
* file the following copyright and licenses apply:
*
* Copyright 2020 RDK Management
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

#include "Module.h"
#include "LifecycleInterfaceConnector.h"
#include <string>
#include <memory>
#include <iostream>
#include <mutex>
#include <thread>
#include <fstream>
#include <sstream>
#include <list>
#include <unistd.h>
#include <plugins/System.h>

#include <interfaces/ILifecycleManager.h>
#include "AppManagerImplementation.h"
#include "UtilsString.h"
#ifdef ENABLE_AIMANAGERS_TELEMETRY_METRICS
#include "AppManagerTelemetryReporting.h"
#endif

#define SUSPEND_POLICY_FILE        "/tmp/AI2.0Suspendable"
#define HIBERNATE_POLICY_FILE      "/tmp/AI2.0Hibernatable"
#define PAUSE_STATE_WAITTIME       1000

using namespace std;
using namespace Utils;

namespace WPEFramework
{
    namespace Plugin
    {
        LifecycleInterfaceConnector* LifecycleInterfaceConnector::_instance = nullptr;
        static uint32_t gAppsActiveCounter = 0;

        LifecycleInterfaceConnector::LifecycleInterfaceConnector(PluginHost::IShell* service)
        : mLifecycleManagerRemoteObject(nullptr),
          mLifecycleManagerStateRemoteObject(nullptr),
          mNotification(*this),
          mAppStateChangeNotification(*this),
          mCurrentservice(nullptr),
          mAppIdAwaitingPause("")
        {
            LOGINFO("Create LifecycleInterfaceConnector Instance");
            LifecycleInterfaceConnector::_instance = this;
            if (service != nullptr)
            {
                mCurrentservice = service;
                mCurrentservice->AddRef();
            }
        }

        LifecycleInterfaceConnector::~LifecycleInterfaceConnector()
        {
            if (nullptr != mCurrentservice)
            {
               mCurrentservice->Release();
               mCurrentservice = nullptr;
            }
            LifecycleInterfaceConnector::_instance = nullptr;

	    //clear action list
	    mAppCurrentActionList.clear();
        }

        Core::hresult LifecycleInterfaceConnector::createLifecycleManagerRemoteObject()
        {
            Core::hresult status = Core::ERROR_GENERAL;

            if (nullptr == mCurrentservice)
            {
                LOGWARN("mCurrentservice is null \n");
            }
            else if (nullptr == (mLifecycleManagerRemoteObject = mCurrentservice->QueryInterfaceByCallsign<WPEFramework::Exchange::ILifecycleManager>("org.rdk.LifecycleManager")))
            {
                LOGWARN("Failed to create LifecycleManager Remote object\n");
            }
            else if (nullptr == (mLifecycleManagerStateRemoteObject = mCurrentservice->QueryInterfaceByCallsign<WPEFramework::Exchange::ILifecycleManagerState>("org.rdk.LifecycleManager")))
            {
                LOGWARN("Failed to create LifecycleManagerState Remote object\n");
            }
            else
            {
                LOGINFO("Created LifecycleManager Remote Object \n");
                mLifecycleManagerRemoteObject->AddRef();
                mLifecycleManagerRemoteObject->Register(&mAppStateChangeNotification);

                LOGINFO("Created LifecycleManagerState Remote Object \n");
                mLifecycleManagerStateRemoteObject->AddRef();
                mLifecycleManagerStateRemoteObject->Register(&mNotification);
                LOGINFO("LifecycleManagerState notification registered");

                status = Core::ERROR_NONE;
            }
            return status;
        }

        void LifecycleInterfaceConnector::releaseLifecycleManagerRemoteObject()
        {
            ASSERT(nullptr != mLifecycleManagerRemoteObject );
            if(mLifecycleManagerRemoteObject )
            {
                mLifecycleManagerRemoteObject->Unregister(&mAppStateChangeNotification);
                mLifecycleManagerRemoteObject ->Release();
                mLifecycleManagerRemoteObject = nullptr;
            }

            ASSERT(nullptr != mLifecycleManagerStateRemoteObject );
            if(mLifecycleManagerStateRemoteObject )
            {
                mLifecycleManagerStateRemoteObject->Unregister(&mNotification);
                mLifecycleManagerStateRemoteObject ->Release();
                mLifecycleManagerStateRemoteObject = nullptr;
            }
        }

        Core::hresult LifecycleInterfaceConnector::isAppLoaded(const string& appId, bool& loaded)
        {
            Core::hresult status = Core::ERROR_GENERAL;
            mAdminLock.Lock();
            /* Checking if mLifecycleManagerRemoteObject is not valid then create the object */
            if (nullptr == mLifecycleManagerRemoteObject)
            {
                LOGINFO("Create LifecycleManager Remote store object");
                if (Core::ERROR_NONE != createLifecycleManagerRemoteObject())
                {
                    LOGERR("Failed to create LifecycleInterfaceConnector");
                }
            }
            ASSERT (nullptr != mLifecycleManagerRemoteObject);

            if (nullptr != mLifecycleManagerRemoteObject)
            {
                status = mLifecycleManagerRemoteObject->IsAppLoaded(appId, loaded);
            }
            mAdminLock.Unlock();
            return status;
        }


/*
 * @brief LaunchApp invokes this to call LifecycleManager API.
 * @Params  : const string& appId , const string& intent , const string& launchArgs
 * @return  : Core::hresult
 */
        Core::hresult LifecycleInterfaceConnector::launch(const string& appId, const string& intent, const string& launchArgs, WPEFramework::Exchange::RuntimeConfig& runtimeConfigObject)
        {
            Core::hresult status = Core::ERROR_GENERAL;
            AppManagerImplementation*appManagerImplInstance = AppManagerImplementation::getInstance();
            bool loaded = false;
            string appInstanceId = "";
            string errorReason = "";
            bool success = true;
            Exchange::ILifecycleManager::LifecycleState state = Exchange::ILifecycleManager::LifecycleState::UNLOADED;
#ifdef ENABLE_AIMANAGERS_TELEMETRY_METRICS
            AppManagerTelemetryReporting& appManagerTelemetryReporting =AppManagerTelemetryReporting::getInstance();
#endif

            if (appId.empty())
            {
                LOGERR("appId is empty");
            }
            else
            {
                mAdminLock.Lock();
                /* Checking if mLifecycleManagerRemoteObject is not valid then create the object */
                if (nullptr == mLifecycleManagerRemoteObject)
                {
                    LOGINFO("Create LifecycleManager Remote store object");
                    if (Core::ERROR_NONE != createLifecycleManagerRemoteObject())
                    {
                        LOGERR("Failed to create LifecycleInterfaceConnector");
                    }
                }
                ASSERT (nullptr != mLifecycleManagerRemoteObject);

                if (nullptr != mLifecycleManagerRemoteObject)
                {
                    status = isAppLoaded(appId, loaded);
                    if (appManagerImplInstance != nullptr)
                    {
                        auto it = appManagerImplInstance->mAppInfo.find(appId);
                        if ((loaded == true) &&
                            (Core::ERROR_NONE == status) &&
                            (it != appManagerImplInstance->mAppInfo.end()) &&
                            (it->second.appNewState == Exchange::IAppManager::AppLifecycleState::APP_STATE_SUSPENDED))
                        {
                            appManagerImplInstance->updateCurrentAction(appId, AppManagerImplementation::APP_ACTION_RESUME);
                            state = Exchange::ILifecycleManager::LifecycleState::ACTIVE;
                            LOGINFO("launchApp appInstanceId %s", it->second.appInstanceId.c_str());
                            status = mLifecycleManagerRemoteObject->SetTargetAppState(it->second.appInstanceId, state, intent);

                            if (status == Core::ERROR_NONE)
                            {
                                LOGINFO("Update App Info");
                                it->second.targetAppState = Exchange::IAppManager::AppLifecycleState::APP_STATE_ACTIVE;
                                it->second.appIntent = intent;
                            }
                            else
                            {
                                LOGERR("SetTargetAppState Failed");
                            }
                        }
                        else
                        {
                            if (fileExists(SUSPEND_POLICY_FILE) == true)
                            {
                                appManagerImplInstance->updateCurrentAction(appId, AppManagerImplementation::APP_ACTION_SUSPEND);
                                state = Exchange::ILifecycleManager::LifecycleState::SUSPENDED;
                            }
                            else
                            {
                                appManagerImplInstance->updateCurrentAction(appId, AppManagerImplementation::APP_ACTION_LAUNCH);
                                state = Exchange::ILifecycleManager::LifecycleState::ACTIVE;
                                string source = "";
                                appManagerImplInstance->handleOnAppLaunchRequest(appId, intent, source);
                            }
                            LOGINFO("spawnApp called ,state %u",state);
                            status = mLifecycleManagerRemoteObject->SpawnApp(appId, intent, state, runtimeConfigObject, launchArgs, appInstanceId, errorReason, success);

                            if (status == Core::ERROR_NONE)
                            {
                                LOGINFO("Update App Info");
                                it->second.appInstanceId   = std::move(appInstanceId);
                                it->second.appIntent       = intent;
                                it->second.packageInfo.type = AppManagerImplementation::APPLICATION_TYPE_INTERACTIVE;
                                it->second.targetAppState  =    (state == Exchange::ILifecycleManager::LifecycleState::SUSPENDED)
                                                                                                                                           ? Exchange::IAppManager::AppLifecycleState::APP_STATE_SUSPENDED
                                                                                                                                           : Exchange::IAppManager::AppLifecycleState::APP_STATE_ACTIVE;
                            }
                            else
                            {
                                LOGERR("spawnApp failed");
#ifdef ENABLE_AIMANAGERS_TELEMETRY_METRICS
                                appManagerTelemetryReporting.reportTelemetryErrorData(appId, AppManagerImplementation::APP_ACTION_LAUNCH, AppManagerImplementation::ERROR_SPAWN_APP);
#endif
                            }
                        }
                    }
                    else
                    {
#ifdef ENABLE_AIMANAGERS_TELEMETRY_METRICS
                        appManagerTelemetryReporting.reportTelemetryErrorData(appId, AppManagerImplementation::APP_ACTION_LAUNCH, AppManagerImplementation::ERROR_INTERNAL);
#endif
                    }
                }
                else
                {
#ifdef ENABLE_AIMANAGERS_TELEMETRY_METRICS
                    appManagerTelemetryReporting.reportTelemetryErrorData(appId, AppManagerImplementation::APP_ACTION_LAUNCH, AppManagerImplementation::ERROR_INTERNAL);
#endif
                }
                mAdminLock.Unlock();
            }
            return status;
        }

        /* PreloadApp invokes it */
        Core::hresult LifecycleInterfaceConnector::preLoadApp(const string& appId, const string& launchArgs, WPEFramework::Exchange::RuntimeConfig& runtimeConfigObject, string& error)
        {
            Core::hresult status = Core::ERROR_GENERAL;
            AppManagerImplementation *appManagerImplInstance = AppManagerImplementation::getInstance();
#ifdef ENABLE_AIMANAGERS_TELEMETRY_METRICS
            AppManagerTelemetryReporting& appManagerTelemetryReporting =AppManagerTelemetryReporting::getInstance();
#endif

            string intent = "";

            string appInstanceId = "";
            bool success = true;
            Exchange::ILifecycleManager::LifecycleState state = Exchange::ILifecycleManager::LifecycleState::UNLOADED;
            if (appId.empty())
            {
                LOGERR("appId is empty");
            }
            else
            {
                mAdminLock.Lock();
                /* Checking if mLifecycleManagerRemoteObject is not valid then create the object */
                if (nullptr == mLifecycleManagerRemoteObject)
                {
                    LOGINFO("Create LifecycleManager Remote store object");
                    if (Core::ERROR_NONE != createLifecycleManagerRemoteObject())
                    {
                        LOGERR("Failed to create LifecycleInterfaceConnector");
                    }
                }
                ASSERT (nullptr != mLifecycleManagerRemoteObject);
                if (nullptr != mLifecycleManagerRemoteObject)
                {
                    if (fileExists(SUSPEND_POLICY_FILE) == true)
                    {
                        appManagerImplInstance->updateCurrentAction(appId, AppManagerImplementation::APP_ACTION_SUSPEND);
                        state = Exchange::ILifecycleManager::LifecycleState::SUSPENDED;
                    }
                    else
                    {
                        appManagerImplInstance->updateCurrentAction(appId, AppManagerImplementation::APP_ACTION_PRELOAD);
                        state = Exchange::ILifecycleManager::LifecycleState::PAUSED;
                    }
                    status = mLifecycleManagerRemoteObject->SpawnApp(appId, intent, state, runtimeConfigObject, launchArgs, appInstanceId, error, success);
                    if (status == Core::ERROR_NONE)
                    {
                        LOGINFO("Update App Info");

                        /*Insert/update loaded app info*/
                        if (nullptr != appManagerImplInstance)
                        {
                            appManagerImplInstance->mAppInfo[appId].appInstanceId   = std::move(appInstanceId);
                            appManagerImplInstance->mAppInfo[appId].packageInfo.type = AppManagerImplementation::APPLICATION_TYPE_INTERACTIVE;
                            appManagerImplInstance->mAppInfo[appId].targetAppState  =    (state == Exchange::ILifecycleManager::LifecycleState::SUSPENDED)
                                                                                                                                       ? Exchange::IAppManager::AppLifecycleState::APP_STATE_SUSPENDED
                                                                                                                                       : Exchange::IAppManager::AppLifecycleState::APP_STATE_PAUSED;
                        }
                    }
                    else
                    {
                        LOGERR("PreLoad failed");
#ifdef ENABLE_AIMANAGERS_TELEMETRY_METRICS
                        appManagerTelemetryReporting.reportTelemetryErrorData(appId, AppManagerImplementation::APP_ACTION_PRELOAD, AppManagerImplementation::ERROR_SPAWN_APP);
#endif
                    }
                }
                else
                {
#ifdef ENABLE_AIMANAGERS_TELEMETRY_METRICS
                    appManagerTelemetryReporting.reportTelemetryErrorData(appId, AppManagerImplementation::APP_ACTION_PRELOAD, AppManagerImplementation::ERROR_INTERNAL);
#endif
                }
                mAdminLock.Unlock();
            }
            return status;
        }

        /* Close App invokes it */
        Core::hresult LifecycleInterfaceConnector::closeApp(const string& appId)
        {
            uint32_t status = Core::ERROR_GENERAL;
            std::string appInstanceId = "";
            std::string appIntent = "";
            AppManagerImplementation* appManagerImplInstance = AppManagerImplementation::getInstance();
#ifdef ENABLE_AIMANAGERS_TELEMETRY_METRICS
            AppManagerTelemetryReporting& appManagerTelemetryReporting =AppManagerTelemetryReporting::getInstance();
#endif
            bool isAppLoaded = false;

            LOGINFO("AppId retrieved: %s", appId.c_str());
            mAdminLock.Lock();

            if(nullptr != appManagerImplInstance)
            {
                for(auto appIterator = appManagerImplInstance->mAppInfo.begin(); appIterator != appManagerImplInstance->mAppInfo.end(); ++appIterator)
                {
                    if(appIterator->first.compare(appId) == 0)
                    {
                        appInstanceId = appIterator->second.appInstanceId;
                        appIntent = appIterator->second.appIntent;
                        isAppLoaded = true;

                        if(nullptr != mLifecycleManagerRemoteObject)
                        {
                            appManagerImplInstance->updateCurrentAction(appId, AppManagerImplementation::APP_ACTION_CLOSE);

			    mAppCurrentActionList[appId] = Exchange::IAppManager::AppLifecycleState::APP_STATE_TERMINATING;

                            status = mLifecycleManagerRemoteObject->SetTargetAppState(appInstanceId, Exchange::ILifecycleManager::LifecycleState::PAUSED, appIntent);

                            if(status == Core::ERROR_NONE)
                            {
                                LOGINFO("Requested PAUSED state for appId: %s. Waiting for PAUSED confirmation...", appId.c_str());

                                mAppIdAwaitingPause = appId;
                                mAdminLock.Unlock();
                                {
                                    std::unique_lock<std::mutex> lk(mStateMutex);
                                    mStateChangedCV.wait_for(lk, std::chrono::milliseconds(PAUSE_STATE_WAITTIME));
                                }

                                mAdminLock.Lock();
                                auto it = appManagerImplInstance->mAppInfo.find(appId);
                                if(it != appManagerImplInstance->mAppInfo.end() &&
                                    it->second.appNewState == Exchange::IAppManager::AppLifecycleState::APP_STATE_PAUSED)
                                {
                                    mAppIdAwaitingPause.clear();

                                    auto retryIt = appManagerImplInstance->mAppInfo.find(appId);
                                    if (retryIt != appManagerImplInstance->mAppInfo.end())
                                    {	
					    // Check for install/uninstall block.
					bool installUninstallBlocked = appManagerImplInstance->checkInstallUninstallBlock(appId);
					if (installUninstallBlocked)
					{
						LOGINFO("Blocked state found for appId: %s. Initiating TERMINATE.", appId.c_str());
						mAdminLock.Unlock();
					        Core::hresult terminateStatus = appManagerImplInstance->TerminateApp(appId);
						LOGINFO("TerminateApp returned status: %d", terminateStatus);
						mAdminLock.Lock();
					}
                                        /* Found the AppInfo; apply suspend/hibernate/unload logic */
					else if (fileExists(SUSPEND_POLICY_FILE))
                                        {
                                            LOGINFO("App with AppId: %s is suspendable", appId.c_str());
                                            retryIt->second.targetAppState = Exchange::IAppManager::AppLifecycleState::APP_STATE_SUSPENDED;
                                            status = mLifecycleManagerRemoteObject->SetTargetAppState(appInstanceId, Exchange::ILifecycleManager::LifecycleState::SUSPENDED, appIntent);

                                            if (status == Core::ERROR_NONE && fileExists(HIBERNATE_POLICY_FILE))
                                            {
                                                LOGINFO("App with AppId: %s is hibernatable", appId.c_str());
                                                retryIt->second.targetAppState = Exchange::IAppManager::AppLifecycleState::APP_STATE_HIBERNATED;
                                                status = mLifecycleManagerRemoteObject->SetTargetAppState(appInstanceId, Exchange::ILifecycleManager::LifecycleState::HIBERNATED, appIntent);

                                                if (status != Core::ERROR_NONE)
                                                {
                                                    LOGERR("Failed to apply hibernate policy for appId: %s", appId.c_str());
                                                }
                                            }
                                            else if (status != Core::ERROR_NONE)
                                            {
                                                LOGERR("Failed to apply suspend policy for appId: %s", appId.c_str());
                                            }
                                        }
                                        else
                                        {
                                            LOGINFO("App with AppId: %s is non suspendable. Keeping app in Paused state", appId.c_str());
                                        }
                                    }
                                    else
                                    {
                                        LOGERR("AppId: %s not found after PAUSED wait", appId.c_str());
#ifdef ENABLE_AIMANAGERS_TELEMETRY_METRICS
                                        appManagerTelemetryReporting.reportTelemetryErrorData(appId, AppManagerImplementation::APP_ACTION_CLOSE, AppManagerImplementation::ERROR_INTERNAL);
#endif
                                        status = Core::ERROR_GENERAL;
                                    }
                                }
                                else
                                {
                                    LOGERR("Timed out waiting for appId: %s to reach PAUSED state", appId.c_str());
#ifdef ENABLE_AIMANAGERS_TELEMETRY_METRICS
                                    appManagerTelemetryReporting.reportTelemetryErrorData(appId, AppManagerImplementation::APP_ACTION_CLOSE, AppManagerImplementation::ERROR_INTERNAL);
#endif
                                    status = Core::ERROR_GENERAL;
                                }
                            }
                            else
                            {
                                LOGERR("Failed to set PAUSED state for AppId: %s", appId.c_str());
#ifdef ENABLE_AIMANAGERS_TELEMETRY_METRICS
                                appManagerTelemetryReporting.reportTelemetryErrorData(appId, AppManagerImplementation::APP_ACTION_CLOSE, AppManagerImplementation::ERROR_SET_TARGET_APP_STATE);
#endif
                            }
                        }
                        else
                        {
#ifdef ENABLE_AIMANAGERS_TELEMETRY_METRICS
                            appManagerTelemetryReporting.reportTelemetryErrorData(appId, AppManagerImplementation::APP_ACTION_CLOSE, AppManagerImplementation::ERROR_INTERNAL);
#endif
                        }

                        break;
                    }
                }

                if (!isAppLoaded)
                {
                    LOGERR("AppId %s not found in database", appId.c_str());
#ifdef ENABLE_AIMANAGERS_TELEMETRY_METRICS
                    appManagerTelemetryReporting.reportTelemetryErrorData(appId, AppManagerImplementation::APP_ACTION_CLOSE, AppManagerImplementation::ERROR_INVALID_PARAMS);
#endif
                }
            }
            else
            {
                LOGERR("AppManagerImplementation instance is null");
#ifdef ENABLE_AIMANAGERS_TELEMETRY_METRICS
                appManagerTelemetryReporting.reportTelemetryErrorData(appId, AppManagerImplementation::APP_ACTION_CLOSE, AppManagerImplementation::ERROR_INTERNAL);
#endif
            }
            mAdminLock.Unlock();
            return status;
        }

        /* Terminate App invokes it */
        Core::hresult LifecycleInterfaceConnector::terminateApp(const string& appId)
        {
            uint32_t status = Core::ERROR_GENERAL;
            std::string appInstanceId = "";
            AppManagerImplementation* appManagerImplInstance = AppManagerImplementation::getInstance();
#ifdef ENABLE_AIMANAGERS_TELEMETRY_METRICS
            AppManagerTelemetryReporting& appManagerTelemetryReporting =AppManagerTelemetryReporting::getInstance();
#endif
            bool success = false;
            std::string errorReason = "";
            bool foundAppId = false;

            /* Retrieve the appId from the parameters object */
            LOGINFO("AppId retrieved: %s", appId.c_str());

            mAdminLock.Lock();
            if (nullptr != appManagerImplInstance)
            {
                for ( std::map<std::string, AppManagerImplementation::AppInfo>::iterator appIterator = appManagerImplInstance->mAppInfo.begin(); appIterator != appManagerImplInstance->mAppInfo.end(); appIterator++)
                {
                    if (appIterator->first.compare(appId) == 0)
                    {
                        foundAppId = true;
                        appInstanceId = appIterator->second.appInstanceId;
                        if (nullptr != mLifecycleManagerRemoteObject)
                        {
                            appManagerImplInstance->updateCurrentAction(appId, AppManagerImplementation::APP_ACTION_TERMINATE);
			    mAppCurrentActionList[appId] = Exchange::IAppManager::AppLifecycleState::APP_STATE_TERMINATING;
                            status = mLifecycleManagerRemoteObject->UnloadApp(appInstanceId, errorReason, success);
                            if (status != Core::ERROR_NONE)
                            {
                                LOGERR("UnloadApp failed with error reason: %s", errorReason.c_str());
#ifdef ENABLE_AIMANAGERS_TELEMETRY_METRICS
                                appManagerTelemetryReporting.reportTelemetryErrorData(appId, AppManagerImplementation::APP_ACTION_TERMINATE, AppManagerImplementation::ERROR_UNLOAD_APP);
#endif
                            }
                        }
                        break;
                    }
                }
                if (!foundAppId)
                {
                    LOGERR("AppId %s not found in database", appId.c_str());
#ifdef ENABLE_AIMANAGERS_TELEMETRY_METRICS
                    appManagerTelemetryReporting.reportTelemetryErrorData(appId, AppManagerImplementation::APP_ACTION_TERMINATE, AppManagerImplementation::ERROR_INVALID_PARAMS);
#endif
                }
            }
            else
            {
                LOGERR("appManagerImplInstance is null");
#ifdef ENABLE_AIMANAGERS_TELEMETRY_METRICS
                appManagerTelemetryReporting.reportTelemetryErrorData(appId, AppManagerImplementation::APP_ACTION_TERMINATE, AppManagerImplementation::ERROR_INTERNAL);
#endif
            }
            mAdminLock.Unlock();
            return status;
        }

        /* Kill App invokes it */
        Core::hresult LifecycleInterfaceConnector::killApp(const string& appId)
        {
            LOGINFO("killApp entered");
            Core::hresult result = Core::ERROR_GENERAL;
            AppManagerImplementation* appManagerImplInstance = AppManagerImplementation::getInstance();
#ifdef ENABLE_AIMANAGERS_TELEMETRY_METRICS
            AppManagerTelemetryReporting& appManagerTelemetryReporting =AppManagerTelemetryReporting::getInstance();
#endif

            if (appId.empty())
            {
                LOGERR("appId is empty");
                return result;
            }

            if (nullptr == mLifecycleManagerRemoteObject)
            {
                LOGINFO("Create LifecycleManager Remote store object");
                if (Core::ERROR_NONE != createLifecycleManagerRemoteObject())
                {
                    LOGERR("Failed to create LifecycleInterfaceConnector");
                }
            }
            ASSERT (nullptr != mLifecycleManagerRemoteObject);
            if (nullptr != mLifecycleManagerRemoteObject)
            {
                mAdminLock.Lock();
                string appInstanceId = GetAppInstanceId(appId);
                if (appInstanceId.empty())
                {
                    LOGERR("appInstanceId not found for appId '%s'", appId.c_str());
#ifdef ENABLE_AIMANAGERS_TELEMETRY_METRICS
                    appManagerTelemetryReporting.reportTelemetryErrorData(appId, AppManagerImplementation::APP_ACTION_KILL, AppManagerImplementation::ERROR_INVALID_PARAMS);
#endif
                }
                else
                {
                    string errorReason{};
                    bool success{};

                    appManagerImplInstance->updateCurrentAction(appId, AppManagerImplementation::APP_ACTION_KILL);
		    mAppCurrentActionList[appId] = Exchange::IAppManager::AppLifecycleState::APP_STATE_TERMINATING;
                    result = mLifecycleManagerRemoteObject->KillApp(appInstanceId, errorReason, success);

                    if (!(result == Core::ERROR_NONE && success))
                    {
                        LOGERR("killApp failed, result: %d, success: %d, errorReason: %s", result, success, errorReason.c_str());
#ifdef ENABLE_AIMANAGERS_TELEMETRY_METRICS
                        appManagerTelemetryReporting.reportTelemetryErrorData(appId, AppManagerImplementation::APP_ACTION_KILL, AppManagerImplementation::ERROR_KILL_APP);
#endif
                    }
                }
                mAdminLock.Unlock();
            }
            else
            {
#ifdef ENABLE_AIMANAGERS_TELEMETRY_METRICS
                appManagerTelemetryReporting.reportTelemetryErrorData(appId, AppManagerImplementation::APP_ACTION_KILL, AppManagerImplementation::ERROR_INTERNAL);
#endif
            }

            return result;
        }

        /* Send Intent invokes it */
        Core::hresult LifecycleInterfaceConnector::sendIntent(const string& appId, const string& intent)
        {
            LOGINFO("sendIntent Entered");
            Core::hresult result = Core::ERROR_GENERAL;

            if (appId.empty())
            {
                LOGERR("appId is empty");
                return result;
            }

            if (nullptr == mLifecycleManagerRemoteObject)
            {
                LOGINFO("Create LifecycleManager Remote store object");
                if (Core::ERROR_NONE != createLifecycleManagerRemoteObject())
                {
                    LOGERR("Failed to create LifecycleInterfaceConnector");
                }
            }

            if (nullptr != mLifecycleManagerRemoteObject)
            {
                mAdminLock.Lock();
                string appInstanceId = GetAppInstanceId(appId);
                if (appInstanceId.empty())
                {
                    LOGERR("appInstanceId not found for appId '%s'", appId.c_str());
                }
                else
                {
                    string errorReason{};
                    bool success{};

                    result = mLifecycleManagerRemoteObject->SendIntentToActiveApp(appInstanceId, intent, errorReason, success);

                    if (!(result == Core::ERROR_NONE && success))
                    {
                        LOGERR("sentIntent failed, result: %d, success: %d, errorReason: %s", result, success, errorReason.c_str());
                    }
                }
                mAdminLock.Unlock();
            }

            return result;
        }

        /* GetLoadedApps invokes it */
        Core::hresult LifecycleInterfaceConnector::getLoadedApps(Exchange::IAppManager::ILoadedAppInfoIterator*& apps)
        {
            LOGINFO("getLoadedApps Entered");
            Core::hresult result = Core::ERROR_GENERAL;
            AppManagerImplementation *appManagerImplInstance = AppManagerImplementation::getInstance();
            std::list<WPEFramework::Exchange::IAppManager::LoadedAppInfo> loadedAppInfoList;

            mAdminLock.Lock();
            /* Checking if mLifecycleManagerRemoteObject is not valid then create the object */
            if (nullptr == mLifecycleManagerRemoteObject)
            {
                LOGINFO("Create LifecycleManager Remote store object");
                if (Core::ERROR_NONE != createLifecycleManagerRemoteObject())
                {
                    LOGERR("Failed to create LifecycleInterfaceConnector");
                }
            }

            ASSERT (nullptr != mLifecycleManagerRemoteObject);
            if (nullptr != mLifecycleManagerRemoteObject)
            {
                JsonArray loadedAppsArray;
                string loadedApps = "";
                LOGINFO("Get Loaded Apps");
                result = mLifecycleManagerRemoteObject->GetLoadedApps(false, loadedApps);
                // Parse the string into a JSON array
                JsonArray loadedAppsJsonArray;
                if ((result != Core::ERROR_NONE) || loadedApps.empty() || !loadedAppsJsonArray.FromString(loadedApps))
                {
                    LOGERR("GetLoadedApps call: %s", (result != Core::ERROR_NONE) ? "Failed" : (loadedApps.empty() ? "returned empty list" : "format not a JSON string"));
                    goto End;
                }
                else
                {
                    LOGINFO("GetLoadedApps succeeded: %s", loadedApps.c_str());
                }

                auto getIntJsonField = [&](JsonObject& obj, const char* key) -> int {
                    return obj.HasLabel(key) ? static_cast<int>(obj[key].Number()) : 0;
                };

                // Iterate through each app JSON object in the array
                for (size_t i = 0; i < loadedAppsJsonArray.Length(); ++i)
                {
                    JsonObject loadedAppsObject = loadedAppsJsonArray[i].Object();
                    string appId = loadedAppsObject.HasLabel("appId")?loadedAppsObject["appId"].String():"";
                    LOGINFO("Loaded appId: %s", appId.c_str());
                    auto& appInfo = appManagerImplInstance->mAppInfo[appId];

                    Exchange::IAppManager::LoadedAppInfo loadedAppInfo = {};
		    loadedAppInfo.appId = appId;
                    loadedAppInfo.type = appManagerImplInstance->getInstallAppType(appInfo.packageInfo.type);
		    loadedAppInfo.appInstanceId = appInfo.appInstanceId = loadedAppsObject.HasLabel("appInstanceID")?loadedAppsObject["appInstanceID"].String():"";
		    loadedAppInfo.activeSessionId = appInfo.activeSessionId = loadedAppsObject.HasLabel("activeSessionId")?loadedAppsObject["activeSessionId"].String():"";

		    appInfo.targetAppState = mapAppLifecycleState(
                    static_cast<Exchange::ILifecycleManager::LifecycleState>(
                            getIntJsonField(loadedAppsObject, "targetLifecycleState")));
                    loadedAppInfo.targetLifecycleState = appInfo.targetAppState;
                    appInfo.appNewState = mapAppLifecycleState(
                        static_cast<Exchange::ILifecycleManager::LifecycleState>(
                            getIntJsonField(loadedAppsObject, "lifecycleState")));
                    loadedAppInfo.lifecycleState = appInfo.appNewState;

                    //Add loaded info
		    loadedAppInfoList.push_back(loadedAppInfo);
                }

                apps = Core::Service<RPC::IteratorType<Exchange::IAppManager::ILoadedAppInfoIterator>> \
		   ::Create<Exchange::IAppManager::ILoadedAppInfoIterator>(loadedAppInfoList);
		result = Core::ERROR_NONE;
            }
            else
            {
                LOGERR("mLifecycleManagerRemoteObject is nullptr");
            }
End:
            mAdminLock.Unlock();
            return result;
        }

        Exchange::IAppManager::AppLifecycleState WPEFramework::Plugin::LifecycleInterfaceConnector::mapAppLifecycleState(Exchange::ILifecycleManager::LifecycleState state)
        {
            Exchange::IAppManager::AppLifecycleState result = Exchange::IAppManager::AppLifecycleState::APP_STATE_UNKNOWN;
            switch(state)
            {
                case Exchange::ILifecycleManager::LifecycleState::UNLOADED:
                    LOGINFO("UNLOADED state %u", state);
                    result = Exchange::IAppManager::AppLifecycleState::APP_STATE_UNLOADED;
                    break;
                case Exchange::ILifecycleManager::LifecycleState::LOADING:
                    LOGINFO("LOADING state %u", state);
                    result = Exchange::IAppManager::AppLifecycleState::APP_STATE_LOADING;
                    break;
                case Exchange::ILifecycleManager::LifecycleState::ACTIVE:
                    LOGINFO("ACTIVE state %u", state);
                    result = Exchange::IAppManager::AppLifecycleState::APP_STATE_ACTIVE;
                    break;
                case Exchange::ILifecycleManager::LifecycleState::PAUSED:
                    LOGINFO("PAUSED state %u", state);
                    result = Exchange::IAppManager::AppLifecycleState::APP_STATE_PAUSED;
                    break;
                case Exchange::ILifecycleManager::LifecycleState::INITIALIZING:
                    LOGINFO("INITIALIZING state %u", state);
                    result = Exchange::IAppManager::AppLifecycleState::APP_STATE_INITIALIZING;
                    break;
                case Exchange::ILifecycleManager::LifecycleState::SUSPENDED:
                    LOGINFO("SUSPENDED state %u", state);
                    result = Exchange::IAppManager::AppLifecycleState::APP_STATE_SUSPENDED;
                    break;
                case Exchange::ILifecycleManager::LifecycleState::TERMINATING:
                    LOGINFO("TERMINATING state %u", state);
                    result = Exchange::IAppManager::AppLifecycleState::APP_STATE_TERMINATING;
                    break;
                case Exchange::ILifecycleManager::LifecycleState::HIBERNATED:
                    LOGINFO("HIBERNATED state %u", state);
                    result = Exchange::IAppManager::AppLifecycleState::APP_STATE_HIBERNATED;
                    break;
                default:
                    LOGWARN("Unknown state %u", state);
                    result = Exchange::IAppManager::AppLifecycleState::APP_STATE_UNKNOWN;
                    break;
            }
            return result;
        }

        void LifecycleInterfaceConnector::OnAppLifecycleStateChanged(const string& appId, const string& appInstanceId, const Exchange::ILifecycleManager::LifecycleState oldState, const Exchange::ILifecycleManager::LifecycleState newState, const string& navigationIntent)
        {
            AppManagerImplementation*appManagerImplInstance = AppManagerImplementation::getInstance();
            Exchange::IAppManager::AppLifecycleState oldAppState = mapAppLifecycleState(oldState);
            Exchange::IAppManager::AppLifecycleState newAppState = mapAppLifecycleState(newState);
            bool shouldNotify = false;

            LOGINFO("OnAppLifecycleStateChanged event triggered ***\n");

            if (newAppState == Exchange::IAppManager::APP_STATE_UNKNOWN ||
                oldAppState == Exchange::IAppManager::APP_STATE_UNKNOWN)
            {
                LOGINFO("Skipping notification: new or old app state is UNKNOWN");
                return;
            }

            if(nullptr != appManagerImplInstance)
            {
                Core::SafeSyncType<Core::CriticalSection> lock(mAdminLock);
                std::map<std::string, AppManagerImplementation::AppInfo>::iterator it;
                for (it = appManagerImplInstance->mAppInfo.begin(); it != appManagerImplInstance->mAppInfo.end(); ++it)
                {
                    if((it->first.compare(appId) == 0) && (it->second.appInstanceId.compare(appInstanceId) == 0))
                    {
                        it->second.appOldState = oldAppState;
                        it->second.appNewState = newAppState;
                        it->second.appLifecycleState = newState;
                        it->second.appIntent = navigationIntent;

                        if (oldState == Exchange::ILifecycleManager::LifecycleState::ACTIVE ||
                            newState == Exchange::ILifecycleManager::LifecycleState::ACTIVE)
                        {
                            struct timespec stateChangeTime;
                            if (timespec_get(&stateChangeTime, TIME_UTC) != 0)
                            {
                                it->second.lastActiveStateChangeTime = stateChangeTime;
                            }
                            else
                            {
                                LOGERR("Unable to update the active state change time for appInstanceId: %s", appInstanceId.c_str());
                            }
                        }

                        if (newState == Exchange::ILifecycleManager::LifecycleState::ACTIVE)
                        {
                            gAppsActiveCounter++;
                            it->second.lastActiveIndex = gAppsActiveCounter;
                        }
                        if (newAppState == Exchange::IAppManager::AppLifecycleState::APP_STATE_PAUSED)
                        {
                            if (appId == mAppIdAwaitingPause)
                            {
                                std::lock_guard<std::mutex> lk(mStateMutex);
                                mStateChangedCV.notify_all();
                            }
                        }
                        break;
                    }
                }
                shouldNotify = ((newAppState == Exchange::IAppManager::AppLifecycleState::APP_STATE_LOADING) ||
                                       (newAppState == Exchange::IAppManager::AppLifecycleState::APP_STATE_ACTIVE) ||
                                       (newAppState == Exchange::IAppManager::AppLifecycleState::APP_STATE_PAUSED) ||
                                       (newAppState == Exchange::IAppManager::AppLifecycleState::APP_STATE_SUSPENDED) ||
                                       (newAppState == Exchange::IAppManager::AppLifecycleState::APP_STATE_HIBERNATED) ||
                                       (newAppState == Exchange::IAppManager::AppLifecycleState::APP_STATE_UNLOADED));

                LOGINFO("shouldNotify %d for Appstate %u",shouldNotify, newAppState);

                if(shouldNotify)
                {
                    if(newAppState == Exchange::IAppManager::AppLifecycleState::APP_STATE_UNLOADED)
		    {
                        if (mAppCurrentActionList[appId] == Exchange::IAppManager::AppLifecycleState::APP_STATE_TERMINATING)
			{
			    //Normal close: Unlode event from App manager
			    LOGINFO("Terminate event from plugin");
			    appManagerImplInstance->handleOnAppLifecycleStateChanged(appId, appInstanceId, newAppState, oldAppState, Exchange::IAppManager::AppErrorReason::APP_ERROR_NONE);
			}
			else
			{
			    //Upnormal close: No unload event from app manager
			    LOGINFO("Terminate event due to app crash");
			    appManagerImplInstance->handleOnAppLifecycleStateChanged(appId, appInstanceId, newAppState, oldAppState, Exchange::IAppManager::AppErrorReason::APP_ERROR_ABORT);
			}
			mAppCurrentActionList.erase(appId);
		    }
		    else
		    {
                        appManagerImplInstance->handleOnAppLifecycleStateChanged(appId, appInstanceId, newAppState, oldAppState, Exchange::IAppManager::AppErrorReason::APP_ERROR_NONE);
		    }
                }

                if(newAppState == Exchange::IAppManager::AppLifecycleState::APP_STATE_UNLOADED)
                {
                    appManagerImplInstance->handleOnAppUnloaded(appId, appInstanceId);
                }

#ifdef ENABLE_AIMANAGERS_TELEMETRY_METRICS
                AppManagerTelemetryReporting::getInstance().reportTelemetryDataOnStateChange(appId, newState);
#endif
            }
        }

        void LifecycleInterfaceConnector::OnAppStateChanged(const string& appId, Exchange::ILifecycleManager::LifecycleState state, const string& errorReason)
        {
            string appInstanceId = "";
            Exchange::IAppManager::AppLifecycleState currentAppState = Exchange::IAppManager::AppLifecycleState::APP_STATE_UNLOADED;
            AppManagerImplementation*appManagerImplInstance = AppManagerImplementation::getInstance();
            Exchange::IAppManager::AppErrorReason errorCode = Exchange::IAppManager::AppErrorReason::APP_ERROR_NONE;
            LOGINFO("OnAppStateChanged event triggered for appId %s: state=%d, errorReason=%s", appId.c_str(), static_cast<int>(state), errorReason.c_str());

            if(nullptr == appManagerImplInstance)
            {
                LOGERR("Invalid AppManager Implementation Instance");
            }
            else
            {
                if (!appId.empty())
                {
                    auto it = appManagerImplInstance->mAppInfo.find(appId);
                    if(it != appManagerImplInstance->mAppInfo.end())
                    {
                        appInstanceId = it->second.appInstanceId;
                        currentAppState = it->second.appNewState;
                    }
                    else
                    {
                        LOGERR("appId not found in database");
                    }
                }

                if (!errorReason.empty())
                {
                    errorCode = mapErrorReason(errorReason);
                    appManagerImplInstance->handleOnAppLifecycleStateChanged(appId, appInstanceId, Exchange::IAppManager::AppLifecycleState::APP_STATE_UNLOADED,
                        currentAppState, errorCode);
                    LOGINFO("Notified error event for appId %s: currentAppState=%d errorCode %d", appId.c_str(), static_cast<int>(currentAppState), static_cast<int>(errorCode));
                }
            }
        }

        Exchange::IAppManager::AppErrorReason LifecycleInterfaceConnector::mapErrorReason(const string& errorReason)
        {
            Exchange::IAppManager::AppErrorReason errorCode = Exchange::IAppManager::AppErrorReason::APP_ERROR_NONE;
            if(!errorReason.empty())
            {
                if(!errorReason.compare("ERROR_CREATE_DISPLAY"))
                {
                    errorCode = Exchange::IAppManager::AppErrorReason::APP_ERROR_CREATE_DISPLAY;
                }
                else if(!errorReason.compare("ERROR_DOBBY_SPEC"))
                {
                    errorCode = Exchange::IAppManager::AppErrorReason::APP_ERROR_DOBBY_SPEC;
                }
                else if(!errorReason.compare("ERROR_INVALID_PARAM"))
                {
                    errorCode = Exchange::IAppManager::AppErrorReason::APP_ERROR_INVALID_PARAM;
                }
                else
                {
                    errorCode = Exchange::IAppManager::AppErrorReason::APP_ERROR_UNKNOWN;
                }
            }
            return errorCode;
        }

        /* Returns appInstanceId for appId. Does not synchronize access to LoadedAppInfo.
         * Caller should take care about the synchronization.
         */
        string LifecycleInterfaceConnector::GetAppInstanceId(const string& appId) const
        {
            AppManagerImplementation* appManagerImpl = AppManagerImplementation::getInstance();
            if (!appManagerImpl)
                return {};

            auto it = appManagerImpl->mAppInfo.find(appId);
            if (it == appManagerImpl->mAppInfo.end())
                return {};
            else
                return it->second.appInstanceId;
        }

        void LifecycleInterfaceConnector::removeAppInfoByAppId(const string& appId)
        {
            AppManagerImplementation* appManagerImpl = AppManagerImplementation::getInstance();
            if (!appManagerImpl)
                return;

            auto it = appManagerImpl->mAppInfo.find(appId);
            if (it != appManagerImpl->mAppInfo.end())
            {
                LOGINFO("appId %s is removed from database", appId.c_str());
                appManagerImpl->mAppInfo.erase(it);
            }
            else
            {
                LOGERR("AppInfo for appId '%s' not found", appId.c_str());
            }
        }

        bool LifecycleInterfaceConnector::fileExists(const char* pFileName)
        {
            bool isRegular = false;

            if (pFileName != nullptr)
            {
                struct stat fileStat;
                if (stat(pFileName, &fileStat) == 0)
                {
                    if (S_ISREG(fileStat.st_mode))
                    {
                        isRegular = true;
                        LOGINFO("File: '%s' exists", pFileName);
                    }
                    else
                    {
                        LOGINFO("File: '%s' exists but is not a regular file", pFileName);
                    }
                }
                else
                {
                    LOGINFO("File: Failed to stat file '%s'", pFileName);
                }
            }
           else
            {
                LOGINFO("Filename pointer is null");
            }

            return isRegular;
        }
     } /* namespace Plugin */
} /* namespace WPEFramework */
