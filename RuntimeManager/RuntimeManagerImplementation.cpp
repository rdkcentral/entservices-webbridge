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

#include "RuntimeManagerImplementation.h"
#include "DobbySpecGenerator.h"
#include <errno.h>
#include <fstream>

namespace WPEFramework
{
    namespace Plugin
    {
        SERVICE_REGISTRATION(RuntimeManagerImplementation, 1, 0);
        RuntimeManagerImplementation* RuntimeManagerImplementation::_instance = nullptr;

        RuntimeManagerImplementation::RuntimeManagerImplementation()
        : mRuntimeManagerImplLock()
        , mCurrentservice(nullptr)
        , mOciContainerObject(nullptr)
        , mStorageManagerObject(nullptr)
        , mWindowManagerConnector(nullptr)
        , mDobbyEventListener(nullptr)
        , mUserIdManager(nullptr)
        , mRuntimeAppPortal("")
#ifdef ENABLE_AIMANAGERS_TELEMETRY_METRICS
        , mTelemetryMetricsObject(nullptr)
#endif
        {
            LOGINFO("Create RuntimeManagerImplementation Instance");
            if (nullptr == RuntimeManagerImplementation::_instance)
            {
                RuntimeManagerImplementation::_instance = this;
            }
#ifdef RIALTO_IN_DAC_FEATURE_ENABLED
        LOGWARN("Creating rialto connector");
        RialtoConnector *rialtoBridge = new RialtoConnector();
        mRialtoConnector = std::shared_ptr<RialtoConnector>(rialtoBridge);
#endif // RIALTO_IN_DAC_FEATURE_ENABLED
        }

        RuntimeManagerImplementation* RuntimeManagerImplementation::getInstance()
        {
            return _instance;
        }

        RuntimeManagerImplementation::~RuntimeManagerImplementation()
        {
            LOGINFO("Call RuntimeManagerImplementation destructor");

            if (nullptr != mCurrentservice)
            {
               mCurrentservice->Release();
               mCurrentservice = nullptr;
            }

            if (nullptr != mStorageManagerObject)
            {
                releaseStorageManagerPluginObject();
            }


            if (nullptr != mWindowManagerConnector)
            {
                mWindowManagerConnector->releasePlugin();
                delete mWindowManagerConnector;
                mWindowManagerConnector = nullptr;
            }

            if (nullptr != mUserIdManager)
            {
                delete mUserIdManager;
                mUserIdManager = nullptr;
            }

            if(nullptr != mOciContainerObject)
            {
                releaseOCIContainerPluginObject();
            }
        }

        Core::hresult RuntimeManagerImplementation::Register(Exchange::IRuntimeManager::INotification *notification)
        {
            ASSERT (nullptr != notification);

            Core::SafeSyncType<Core::CriticalSection> lock(mRuntimeManagerImplLock);

            /* Make sure we can't register the same notification callback multiple times */
            if (std::find(mRuntimeManagerNotification.begin(), mRuntimeManagerNotification.end(), notification) == mRuntimeManagerNotification.end())
            {
                LOGINFO("Register notification");
                mRuntimeManagerNotification.push_back(notification);
                notification->AddRef();
            }

            return Core::ERROR_NONE;
        }

        Core::hresult RuntimeManagerImplementation::Unregister(Exchange::IRuntimeManager::INotification *notification )
        {
            Core::hresult status = Core::ERROR_GENERAL;

            ASSERT (nullptr != notification);

            Core::SafeSyncType<Core::CriticalSection> lock(mRuntimeManagerImplLock);

            /* Make sure we can't unregister the same notification callback multiple times */
            auto itr = std::find(mRuntimeManagerNotification.begin(), mRuntimeManagerNotification.end(), notification);
            if (itr != mRuntimeManagerNotification.end())
            {
                (*itr)->Release();
                LOGINFO("Unregister notification");
                mRuntimeManagerNotification.erase(itr);
                status = Core::ERROR_NONE;
            }
            else
            {
                LOGERR("notification not found");
            }

            return status;
        }

        void RuntimeManagerImplementation::dispatchEvent(RuntimeEventType event, const JsonValue &params)
        {
            Core::IWorkerPool::Instance().Submit(Job::Create(this, event, params));
        }

        void RuntimeManagerImplementation::Dispatch(RuntimeEventType event, const JsonValue params)
        {
            Core::SafeSyncType<Core::CriticalSection> lock(mRuntimeManagerImplLock);

            std::list<Exchange::IRuntimeManager::INotification*>::const_iterator index(mRuntimeManagerNotification.begin());

            JsonObject obj = params.Object();
            string appIdFromContainer = obj["containerId"].String();
            if (!mRuntimeAppPortal.empty() && appIdFromContainer.find(mRuntimeAppPortal) == 0) // TODO improve logic of fetching appInstanceId
            {
                appIdFromContainer.erase(0, mRuntimeAppPortal.length());
            }
            string appInstanceId = std::move(appIdFromContainer);
            string eventName = obj["eventName"].String();
            LOGINFO("Dispatching event[%s] for appInstanceId[%s]", eventName.c_str(), appInstanceId.c_str());

            switch (event)
            {
                case RUNTIME_MANAGER_EVENT_STATECHANGED:
                while (index != mRuntimeManagerNotification.end())
                {
                    string containerState = obj["state"];
                    int containerStateInt = std::stoi(containerState);
                    RuntimeState state = static_cast<RuntimeState>(containerStateInt);
                    LOGINFO("RuntimeManagerImplementation::Dispatch: state[%d]", state);
                    (*index)->OnStateChanged(appInstanceId, state);
                    ++index;
                }
                break;

                case RUNTIME_MANAGER_EVENT_CONTAINERSTARTED:
                {
#ifdef ENABLE_AIMANAGERS_TELEMETRY_METRICS
                    auto it = mRuntimeAppInfo.find(appInstanceId);
                    if (it != mRuntimeAppInfo.end())
                    {
                        RuntimeAppInfo& appInfo = it->second;

                        if (appInfo.requestType == REQUEST_TYPE_LAUNCH)
                        {
                            recordTelemetryData(TELEMETRY_MARKER_LAUNCH_TIME, appInfo.appId, appInfo.requestTime);
                        }
                    }
                    else
                    {
                        LOGERR("RuntimeAppInfo not found for appInstanceId: %s", appInstanceId.c_str());
                    }
#endif
                    while (index != mRuntimeManagerNotification.end())
                    {
                        (*index)->OnStarted(appInstanceId);
                        ++index;
                    }
                    break;
                }

                case RUNTIME_MANAGER_EVENT_CONTAINERSTOPPED:
                {
#ifdef ENABLE_AIMANAGERS_TELEMETRY_METRICS
                    auto it = mRuntimeAppInfo.find(appInstanceId);
                    if (it != mRuntimeAppInfo.end())
                    {
                        RuntimeAppInfo& appInfo = it->second;
                        if (appInfo.requestType == REQUEST_TYPE_TERMINATE || appInfo.requestType == REQUEST_TYPE_KILL)
                        {
                            recordTelemetryData(TELEMETRY_MARKER_CLOSE_TIME, appInfo.appId, appInfo.requestTime);
                        }
                    }
                    else
                    {
                        LOGERR("RuntimeAppInfo not found for appInstanceId: %s", appInstanceId.c_str());
                    }
#endif
                    while (index != mRuntimeManagerNotification.end())
                    {
                        (*index)->OnTerminated(appInstanceId);
                        ++index;
                    }
                break;
                }

                case RUNTIME_MANAGER_EVENT_CONTAINERFAILED:
                while (index != mRuntimeManagerNotification.end())
                {
                    string error = obj["errorCode"].String();
                    (*index)->OnFailure(appInstanceId, error);
                    ++index;
                }
                break;

                default:
                    LOGWARN("Event[%u] not handled", event);
                break;
            }
        }

        uint32_t RuntimeManagerImplementation::Configure(PluginHost::IShell* service)
        {
            uint32_t result = Core::ERROR_GENERAL;

            if (service != nullptr)
            {
                Core::SafeSyncType<Core::CriticalSection> lock(mRuntimeManagerImplLock);

                mCurrentservice = service;
                mCurrentservice->AddRef();

#ifdef ENABLE_AIMANAGERS_TELEMETRY_METRICS
                if (nullptr == (mTelemetryMetricsObject = mCurrentservice->QueryInterfaceByCallsign<WPEFramework::Exchange::ITelemetryMetrics>("org.rdk.TelemetryMetrics")))
                {
                    LOGERR("mTelemetryMetricsObject is null \n");
                }
                else
                {
                    LOGINFO("created TelemetryMetrics Object");
                }
#endif
                /* Create Storage Manager Plugin Object */
                if (Core::ERROR_NONE != createStorageManagerPluginObject())
                {
                    LOGERR("Failed to create Storage Manager Object");
                }

                /* Create Window Manager Plugin Object */
                mWindowManagerConnector = new WindowManagerConnector();
                if (false == mWindowManagerConnector->initializePlugin(service))
                {
                    LOGERR("Failed to create Window Manager Connector Object");
                }

                mUserIdManager = new UserIdManager();

                if (Core::ERROR_NONE != createOCIContainerPluginObject())
                {
                    LOGERR("Failed to create OCIContainerPluginObject");
                }
                else
                {
                    LOGINFO("created OCIContainerPluginObject");
                    result = Core::ERROR_NONE;
                }
                RuntimeManagerImplementation::Configuration config;
                config.FromString(service->ConfigLine());
                if (!config.runtimeAppPortal.Value().empty())
                {
                    mRuntimeAppPortal = config.runtimeAppPortal.Value();
                }
                LOGINFO("runtimeAppPortal=%s", mRuntimeAppPortal.c_str());
            }
            else
            {
                LOGERR("service is null");
            }
            return result;
        }



        Core::hresult RuntimeManagerImplementation::createOCIContainerPluginObject()
        {
            #define MAX_OCI_OBJECT_CREATION_RETRIES 2

            Core::hresult status = Core::ERROR_GENERAL;
            uint8_t retryCount = 0;

            if (nullptr == mCurrentservice)
            {
                LOGERR("mCurrentservice is null");
                goto err_ret;
            }

            do
            {
                mOciContainerObject = mCurrentservice->QueryInterfaceByCallsign<WPEFramework::Exchange::IOCIContainer>("org.rdk.OCIContainer");
                if (nullptr == mOciContainerObject)
                {
                    LOGERR("mOciContainerObject is null (Attempt %d)", retryCount + 1);
                    retryCount++;
                    std::this_thread::sleep_for(std::chrono::milliseconds(200));
                }
                else
                {
                    LOGINFO("Successfully created OCI Container Object");
                    status = Core::ERROR_NONE;
                    /* Initialize OCIContainerNotification Connector to listen to Dobby Events */
                    mDobbyEventListener = new DobbyEventListener();
                    if(nullptr != mDobbyEventListener)
                    {
                        if (false == mDobbyEventListener->initialize(mCurrentservice, this, mOciContainerObject))
                        {
                            LOGERR("Failed to initialize DobbyEventListener");
                        }
                        break;
                    }
                }
            } while (retryCount < MAX_OCI_OBJECT_CREATION_RETRIES);

            if (status != Core::ERROR_NONE)
            {
                LOGERR("Failed to create OCIContainer Object after %d attempts", MAX_OCI_OBJECT_CREATION_RETRIES);
            }
err_ret:
            return status;
        }

        void RuntimeManagerImplementation::releaseOCIContainerPluginObject()
        {
            ASSERT(nullptr != mOciContainerObject);
            if(mOciContainerObject)
            {
                LOGINFO("releaseOCIContainerPluginObject\n");
                /* Deinitialize DobbyEventListener */
                if (nullptr != mDobbyEventListener)
                {
                    mDobbyEventListener->deinitialize();
                    delete mDobbyEventListener;
                    mDobbyEventListener = nullptr;
                }
                mOciContainerObject->Release();
                mOciContainerObject = nullptr;
            }
        }

        Core::hresult RuntimeManagerImplementation::createStorageManagerPluginObject()
        {
            #define MAX_STORAGE_MANAGER_OBJECT_CREATION_RETRIES 2

            Core::hresult status = Core::ERROR_GENERAL;
            uint8_t retryCount = 0;

            if (nullptr == mCurrentservice)
            {
                LOGERR("mCurrentservice is null");
            }
            else
            {
                do
                {
                    mStorageManagerObject = mCurrentservice->QueryInterfaceByCallsign<WPEFramework::Exchange::IStorageManager>("org.rdk.StorageManager");

                    if (nullptr == mStorageManagerObject)
                    {
                        LOGERR("storageManagerObject is null (Attempt %d)", retryCount + 1);
                        retryCount++;
                        std::this_thread::sleep_for(std::chrono::milliseconds(200));
                    }
                    else
                    {
                        LOGINFO("Successfully created Storage Manager Object");
                        status = Core::ERROR_NONE;
                        break;
                    }
                } while (retryCount < MAX_STORAGE_MANAGER_OBJECT_CREATION_RETRIES);

                if (status != Core::ERROR_NONE)
                {
                    LOGERR("Failed to create Storage Manager Object after %d attempts", MAX_STORAGE_MANAGER_OBJECT_CREATION_RETRIES);
                }
            }
            return status;
        }

        void RuntimeManagerImplementation::releaseStorageManagerPluginObject()
        {
            ASSERT(nullptr != mStorageManagerObject);
            if(mStorageManagerObject)
            {
                LOGINFO("Storage Manager object released\n");
                mStorageManagerObject->Release();
                mStorageManagerObject = nullptr;
            }
        }

/*
* @brief : Returns the storage information for a given app id using Storage Manager plugin interface
*/
        Core::hresult RuntimeManagerImplementation::getAppStorageInfo(const string& appId, AppStorageInfo& appStorageInfo)
        {
            Core::hresult status = Core::ERROR_GENERAL;

            if (appId.empty())
            {
                LOGERR("Invalid appId");
            }
            else
            {
                /* Re-attempting to create Storage Manager Object if the previous attempt failed (i.e., object is null) */
                if (nullptr == mStorageManagerObject)
                {
                    if (Core::ERROR_NONE != createStorageManagerPluginObject())
                    {
                        LOGERR("Re-attempt failed to create Storage Manager Object");
                    }
                }

                if (nullptr != mStorageManagerObject)
                {
                    if (Core::ERROR_NONE == (status = mStorageManagerObject->GetStorage(appId, appStorageInfo.userId, appStorageInfo.groupId,
                        appStorageInfo.path, appStorageInfo.size, appStorageInfo.used)))
                    {
                        LOGINFO("Received Storage Manager info for %s [path %s, userId %d, groupId %d, size %d, used %d]",
                            appId.c_str(), appStorageInfo.path.c_str(), appStorageInfo.userId,
                            appStorageInfo.groupId, appStorageInfo.size, appStorageInfo.used);
                    }
                    else
                    {
                        LOGERR("Failed to get Storage Manager info");
                    }
                }
            }

            return status;
        }

        bool RuntimeManagerImplementation::generate(const ApplicationConfiguration& config, const WPEFramework::Exchange::RuntimeConfig& runtimeConfigObject, std::string& dobbySpec)
        {
            DobbySpecGenerator generator;
            return generator.generate(config, runtimeConfigObject, dobbySpec);
        }

        Exchange::IRuntimeManager::RuntimeState RuntimeManagerImplementation::getRuntimeState(const string& appInstanceId)
        {
            Exchange::IRuntimeManager::RuntimeState runtimeState = Exchange::IRuntimeManager::RUNTIME_STATE_UNKNOWN;

            Core::SafeSyncType<Core::CriticalSection> lock(mRuntimeManagerImplLock);

            if (!appInstanceId.empty())
            {
                if(mRuntimeAppInfo.find(appInstanceId) == mRuntimeAppInfo.end())
                {
                   LOGERR("Missing appInstanceId[%s] in RuntimeAppInfo", appInstanceId.c_str());
                }
                else
                {
                   runtimeState = mRuntimeAppInfo[appInstanceId].containerState;
                }
            }
            else
            {
                LOGERR("appInstanceId param is missing");
            }

            return runtimeState;
        }

        bool RuntimeManagerImplementation::isOCIPluginObjectValid(void)
        {
            return (mOciContainerObject != nullptr) ||
                      (createOCIContainerPluginObject() == Core::ERROR_NONE);
        }

        std::string RuntimeManagerImplementation::getContainerId(const string& appInstanceId)
        {
            string containerId = "";

            if (!appInstanceId.empty())
            {
                containerId = mRuntimeAppPortal + appInstanceId;
            }
            return containerId;
        }
        Core::hresult RuntimeManagerImplementation::Run(const string& appId, const string& appInstanceId, const uint32_t userId, const uint32_t groupId, IValueIterator* const& ports, IStringIterator* const& paths, IStringIterator* const& debugSettings, const WPEFramework::Exchange::RuntimeConfig& runtimeConfigObject)
        {
            Core::hresult status = Core::ERROR_GENERAL;
            RuntimeAppInfo runtimeAppInfo;
            std::string xdgRuntimeDir = "";
            std::string waylandDisplay = "";
            std::string dobbySpec;
            AppStorageInfo appStorageInfo;
            int32_t descriptor = -1;
            std::string errorReason = "";
            bool success = false;
            std::string westerosSocket = "";
            ApplicationConfiguration config;
            config.mAppId = appId;
            config.mAppInstanceId = appInstanceId;
            bool displayResult = false;
            bool notifyParamCheckFailure = false;
            std::string errorCode = "";

#ifdef ENABLE_AIMANAGERS_TELEMETRY_METRICS
            /* Get current timestamp at the start of run for telemetry */
            time_t requestTime = getCurrentTimestamp();
#endif

            JsonObject eventData;
            eventData["containerId"] = appInstanceId;
            eventData["state"] = static_cast<int>(RUNTIME_STATE_STARTING);
            eventData["eventName"] = "onContainerStateChanged";
            dispatchEvent(RuntimeManagerImplementation::RuntimeEventType::RUNTIME_MANAGER_EVENT_STATECHANGED, eventData);

            mRuntimeManagerImplLock.Lock();

            uid_t uid = mUserIdManager->getUserId(appId);
            gid_t gid = mUserIdManager->getAppsGid();

            std::ifstream inFile("/tmp/specchange");
            if (inFile.good())
            {
                uid = 30490;
            }
            config.mUserId = uid;
            config.mGroupId = gid;

            if (ports)
            {
                std::uint32_t port;
                while (ports->Next(port))
                {
                    config.mPorts.push_back(port);
                }
            }

            // if (paths)
            // {
            //     std::string path;
            //     while (paths->Next(path))
            //     {
            //         config.mPaths.push_back(path);
            //     }
            // }

            if (debugSettings)
            {
                std::string debugSetting;
                while (debugSettings->Next(debugSetting))
                {
                    config.mDebugSettings.push_back(debugSetting);
                }
            }

            LOGINFO("ApplicationConfiguration populated for InstanceId: %s", appInstanceId.c_str());

            if (runtimeConfigObject.envVariables.empty())
            {
                LOGERR("envVariables is empty inside Run()");
            }

            if (!appId.empty())
            {
                appStorageInfo.userId = userId;
                appStorageInfo.groupId = groupId;
                if (Core::ERROR_NONE == getAppStorageInfo(appId, appStorageInfo))
                {
                    config.mAppStorageInfo.path = std::move(appStorageInfo.path);
                    config.mAppStorageInfo.userId = userId;
                    config.mAppStorageInfo.groupId = groupId;
                    config.mAppStorageInfo.size = std::move(appStorageInfo.size);
                    config.mAppStorageInfo.used = std::move(appStorageInfo.used);
                }
            }

            /* Creating Display */
            if(nullptr != mWindowManagerConnector)
            {

                mWindowManagerConnector->getDisplayInfo(appInstanceId, xdgRuntimeDir, waylandDisplay);
                displayResult = mWindowManagerConnector->createDisplay(appInstanceId, waylandDisplay, uid, gid);
                if(false == displayResult)
                {
                    LOGERR("Failed to create display");
                    status = Core::ERROR_GENERAL;
                }
                else
                {
                    LOGINFO("Display [%s] created successfully", waylandDisplay.c_str());
                }

            }
            else
            {
                LOGERR("WindowManagerConnector is null");
                status = Core::ERROR_GENERAL;
            }

            if (!xdgRuntimeDir.empty() && !waylandDisplay.empty())
            {
                westerosSocket = xdgRuntimeDir + "/" + waylandDisplay;
                config.mWesterosSocketPath = westerosSocket;
            }

            bool legacyContainer = true;
#ifdef RIALTO_IN_DAC_FEATURE_ENABLED
             mRialtoConnector->initialize();
            if (mRialtoConnector->createAppSession(appId,westerosSocket, appId))
            {
               if (!mRialtoConnector->waitForStateChange(appId,RialtoServerStates::ACTIVE, RIALTO_TIMEOUT_MILLIS))
                {
                  LOGWARN(" Rialto app session not ready. ");
                  status = Core::ERROR_GENERAL;
                }
            }
            else
            {
               LOGWARN(" Rialto app session not ready. ");
               status = Core::ERROR_GENERAL;
            }
            legacyContainer = false;
#endif // RIALTO_IN_DAC_FEATURE_ENABLED
            LOGINFO("legacyContainer: %s", legacyContainer ? "true" : "false");
            if (xdgRuntimeDir.empty() || waylandDisplay.empty() || !displayResult)
            {
                LOGERR("Missing required environment variables: XDG_RUNTIME_DIR=%s, WAYLAND_DISPLAY=%s createDisplay %s",
                    xdgRuntimeDir.empty() ? "NOT FOUND" : xdgRuntimeDir.c_str(),
                    waylandDisplay.empty() ? "NOT FOUND" : waylandDisplay.c_str(),
                    displayResult ? "Success" : "Failed");
                status = Core::ERROR_GENERAL;
                errorCode = "ERROR_CREATE_DISPLAY";
                notifyParamCheckFailure = true;
            }
            /* Generate dobbySpec */
            else if (legacyContainer && false == RuntimeManagerImplementation::generate(config, runtimeConfigObject, dobbySpec))
            {
                LOGERR("Failed to generate dobbySpec");
                status = Core::ERROR_GENERAL;
                errorCode = "ERROR_DOBBY_SPEC";
                notifyParamCheckFailure = true;
            }
            else
            {
                /* Generated dobbySpec */
                LOGINFO("Generated dobbySpec: %s", dobbySpec.c_str());

                LOGINFO("Environment Variables: XDG_RUNTIME_DIR=%s, WAYLAND_DISPLAY=%s",
                     xdgRuntimeDir.c_str(), waylandDisplay.c_str());
                std::string command = "";
                std::string appPath = runtimeConfigObject.unpackedPath;
                if(isOCIPluginObjectValid())
                {
                    string containerId = getContainerId(appInstanceId);
                    if (!containerId.empty())
                    {
                        if(legacyContainer)
                            status =  mOciContainerObject->StartContainerFromDobbySpec(containerId, dobbySpec, command, westerosSocket, descriptor, success, errorReason);
                        else
                            status = mOciContainerObject->StartContainer(containerId, appPath, command, westerosSocket, descriptor, success, errorReason);

                        if ((success == false) || (status != Core::ERROR_NONE))
                        {
                            LOGERR("Failed to Run Container %s",errorReason.c_str());
                        }
                        else
                        {
                            LOGINFO("Update Info for %s",appInstanceId.c_str());
                            if (!appId.empty())
                            {
                                runtimeAppInfo.appId = std::move(appId);
                            }
                            runtimeAppInfo.appInstanceId = std::move(appInstanceId);
                            runtimeAppInfo.descriptor = std::move(descriptor);
                            runtimeAppInfo.containerState = Exchange::IRuntimeManager::RUNTIME_STATE_STARTING;
#ifdef ENABLE_AIMANAGERS_TELEMETRY_METRICS
                            /* Store request time and type in runtime app info map */
                            runtimeAppInfo.requestTime = requestTime;
                            runtimeAppInfo.requestType = REQUEST_TYPE_LAUNCH;
#endif
                            /* Insert/update runtime app info */
                            mRuntimeAppInfo[runtimeAppInfo.appInstanceId] = std::move(runtimeAppInfo);
                        }
                    }
                    else
                    {
                        LOGERR("appInstanceId is not found ");
                        errorCode = "ERROR_INVALID_PARAM";
                        notifyParamCheckFailure = true;
                    }
                }
                else
                {
                    LOGERR("OCI Plugin object is not valid. Aborting Run.");
                }
            }
            mRuntimeManagerImplLock.Unlock();
            if(notifyParamCheckFailure)
            {
                notifyParameterCheckFailure(appInstanceId, errorCode);
            }
            return status;
        }

        Core::hresult RuntimeManagerImplementation::Hibernate(const string& appInstanceId)
        {
            Core::hresult status = Core::ERROR_GENERAL;
            std::string options = "";
            std::string errorReason = "";
#ifdef ENABLE_AIMANAGERS_TELEMETRY_METRICS
            std::string appId = "";
#endif
            bool success = false;

#ifdef ENABLE_AIMANAGERS_TELEMETRY_METRICS
            /* Get current timestamp at the start of hibernate for telemetry */
            time_t requestTime = getCurrentTimestamp();
#endif

            mRuntimeManagerImplLock.Lock();

            if(isOCIPluginObjectValid())
            {
               string containerId = getContainerId(appInstanceId);
                if (!containerId.empty())
                {
                    status =  mOciContainerObject->HibernateContainer(containerId, options, success, errorReason);
                    if ((success == false))
                    {
                        LOGERR("Failed to HibernateContainer %s",errorReason.c_str());
                        status = Core::ERROR_GENERAL; //todo return proper error code in OCIContainerPlugin
                    }
                    else
                    {
                        if (mRuntimeAppInfo.find(appInstanceId) != mRuntimeAppInfo.end())
                        {
                            mRuntimeAppInfo[appInstanceId].containerState = Exchange::IRuntimeManager::RUNTIME_STATE_HIBERNATING;
#ifdef ENABLE_AIMANAGERS_TELEMETRY_METRICS
                            appId = mRuntimeAppInfo[appInstanceId].appId;
#endif
                        }
                    }
                }
                else
                {
                    LOGERR("appInstanceId is not found or mOciContainerObject is not ready");
                }
            }
            else
            {
                LOGERR("OCI Plugin object is not valid. Aborting Hibernate.");
            }
            mRuntimeManagerImplLock.Unlock();

#ifdef ENABLE_AIMANAGERS_TELEMETRY_METRICS
            recordTelemetryData(TELEMETRY_MARKER_HIBERNATE_TIME, appId, requestTime);
#endif

            return status;
        }

        Core::hresult RuntimeManagerImplementation::Wake(const string& appInstanceId, const RuntimeState runtimeState)
        {
            Core::hresult status = Core::ERROR_GENERAL;
            std::string errorReason = "";
#ifdef ENABLE_AIMANAGERS_TELEMETRY_METRICS
            std::string appId = "";
#endif
            bool success = false;

#ifdef ENABLE_AIMANAGERS_TELEMETRY_METRICS
            /* Get current timestamp at the start of wake for telemetry */
            time_t requestTime = getCurrentTimestamp();
#endif

            mRuntimeManagerImplLock.Lock();
            if(isOCIPluginObjectValid())
            {
                string containerId = getContainerId(appInstanceId);
                if (!containerId.empty())
                {
                    RuntimeState currentRuntimeState = getRuntimeState(appInstanceId);
                    if (Exchange::IRuntimeManager::RUNTIME_STATE_HIBERNATING == currentRuntimeState ||
                        Exchange::IRuntimeManager::RUNTIME_STATE_HIBERNATED == currentRuntimeState)
                    {
                        status =  mOciContainerObject->WakeupContainer(containerId, success, errorReason);
                        if ((success == false) || (status != Core::ERROR_NONE))
                        {
                            LOGERR("Failed to WakeupContainer %s",errorReason.c_str());
                        }
                        else
                        {
                            if (mRuntimeAppInfo.find(appInstanceId) != mRuntimeAppInfo.end())
                            {
                                mRuntimeAppInfo[appInstanceId].containerState = Exchange::IRuntimeManager::RUNTIME_STATE_WAKING;
#ifdef ENABLE_AIMANAGERS_TELEMETRY_METRICS
                                appId = mRuntimeAppInfo[appInstanceId].appId;
#endif
                            }
                        }
                    }
                    else
                    {
                        LOGERR("Container is Not in Hibernating/Hiberanted state");
                    }
                }
                else
                {
                    LOGERR("appInstanceId is not found ");
                }
            }
            else
            {
                LOGERR("OCI Plugin object is not valid. Aborting Wake.");
            }
            mRuntimeManagerImplLock.Unlock();

#ifdef ENABLE_AIMANAGERS_TELEMETRY_METRICS
            recordTelemetryData(TELEMETRY_MARKER_WAKE_TIME, appId, requestTime);
#endif

            return status;
        }

        Core::hresult RuntimeManagerImplementation::Suspend(const string& appInstanceId)
        {
            Core::hresult status = Core::ERROR_GENERAL;
            std::string errorReason = "";
#ifdef ENABLE_AIMANAGERS_TELEMETRY_METRICS
            std::string appId = "";
#endif
            bool success = false;

#ifdef ENABLE_AIMANAGERS_TELEMETRY_METRICS
            /* Get current timestamp at the start of suspend for telemetry */
            time_t requestTime = getCurrentTimestamp();
#endif

            mRuntimeManagerImplLock.Lock();

            if(isOCIPluginObjectValid())
            {
                string containerId = getContainerId(appInstanceId);

                if (!containerId.empty())
                {
                    status =  mOciContainerObject->PauseContainer(containerId, success, errorReason);
                    if ((success == false) || (status != Core::ERROR_NONE))
                    {
                        LOGERR("Failed to PauseContainer %s",errorReason.c_str());
                    }
#ifdef ENABLE_AIMANAGERS_TELEMETRY_METRICS
                    else
                    {
                        if (mRuntimeAppInfo.find(appInstanceId) != mRuntimeAppInfo.end())
                        {
                            appId = mRuntimeAppInfo[appInstanceId].appId;
                        }
                    }
#endif
                }
                else
                {
                    LOGERR("appInstanceId is not found ");
                }
            }
            else
            {
                LOGERR("OCI Plugin object is not valid. Aborting Suspend.");
            }
            mRuntimeManagerImplLock.Unlock();

#ifdef ENABLE_AIMANAGERS_TELEMETRY_METRICS
            recordTelemetryData(TELEMETRY_MARKER_SUSPEND_TIME, appId, requestTime);
#endif

            return status;
        }

        Core::hresult RuntimeManagerImplementation::Resume(const string& appInstanceId)
        {
            Core::hresult status = Core::ERROR_GENERAL;
            std::string errorReason = "";
#ifdef ENABLE_AIMANAGERS_TELEMETRY_METRICS
            std::string appId = "";
#endif
            bool success = false;

#ifdef ENABLE_AIMANAGERS_TELEMETRY_METRICS
            /* Get current timestamp at the start of resume for telemetry */
            time_t requestTime = getCurrentTimestamp();
#endif

            mRuntimeManagerImplLock.Lock();
            if(isOCIPluginObjectValid())
            {
                string containerId = getContainerId(appInstanceId);

                if (!containerId.empty())
                {
                    status =  mOciContainerObject->ResumeContainer(containerId, success, errorReason);
                    if ((success == false) || (status != Core::ERROR_NONE))
                    {
                        LOGERR("Failed to ResumeContainer %s",errorReason.c_str());
                    }
#ifdef ENABLE_AIMANAGERS_TELEMETRY_METRICS
                    else
                    {
                        if (mRuntimeAppInfo.find(appInstanceId) != mRuntimeAppInfo.end())
                        {
                            appId = mRuntimeAppInfo[appInstanceId].appId;
                        }
                    }
#endif
                }
                else
                {
                    LOGERR("appInstanceId is empty ");
                }
            }
            else
            {
                LOGERR("OCI Plugin object is not valid. Aborting Resume.");
            }
            mRuntimeManagerImplLock.Unlock();

#ifdef ENABLE_AIMANAGERS_TELEMETRY_METRICS
            recordTelemetryData(TELEMETRY_MARKER_RESUME_TIME, appId, requestTime);
#endif

            return status;
        }

        Core::hresult RuntimeManagerImplementation::Terminate(const string& appInstanceId)
        {
            Core::hresult status = Core::ERROR_GENERAL;
            std::string errorReason = "";
            bool success = false;

#ifdef ENABLE_AIMANAGERS_TELEMETRY_METRICS
            /* Get current timestamp at the start of terminate for telemetry */
            time_t requestTime = getCurrentTimestamp();
#endif

            mRuntimeManagerImplLock.Lock();

#ifdef ENABLE_AIMANAGERS_TELEMETRY_METRICS
            auto it = mRuntimeAppInfo.find(appInstanceId);
            if (it != mRuntimeAppInfo.end())
            {
                it->second.requestTime = requestTime;
                it->second.requestType = REQUEST_TYPE_TERMINATE;
            }
            else
            {
                LOGERR("Terminate called for unknown appInstanceId: %s, skipping telemetry update", appInstanceId.c_str());
            }
#endif
            if(isOCIPluginObjectValid())
            {
                string containerId = getContainerId(appInstanceId);

                if (!containerId.empty())
                {
                    status =  mOciContainerObject->StopContainer(containerId, false, success, errorReason);
                    if (errorReason.compare("Container not found") == 0)
                    {
                        LOGINFO("Container is not running, no need to StopContainer");
                        status = Core::ERROR_NONE;
                        mUserIdManager->clearUserId(appInstanceId);
                    }
                    else if ((success == false) || (status != Core::ERROR_NONE))
                    {
                        LOGERR("StopContainer failed to terminate %s",errorReason.c_str());
                    }
                    else
                    {
                        mUserIdManager->clearUserId(appInstanceId);
                        if (mRuntimeAppInfo.find(appInstanceId) != mRuntimeAppInfo.end())
                        {
                            mRuntimeAppInfo[appInstanceId].containerState = Exchange::IRuntimeManager::RUNTIME_STATE_TERMINATING;
                        }
                    }
                }
                else
                {
                    LOGERR("appInstanceId is not found");
                }
            }
            else
            {
                LOGERR("OCI Plugin object is not valid. Aborting Terminate.");
            }
#ifdef RIALTO_IN_DAC_FEATURE_ENABLED
            LOGINFO("Rialto session deactivate on terminate.");
            mRialtoConnector->deactivateSession(mRuntimeAppInfo[appInstanceId].appId);
            if (!mRialtoConnector->waitForStateChange(mRuntimeAppInfo[appInstanceId].appId,RialtoServerStates::NOT_RUNNING, RIALTO_TIMEOUT_MILLIS))
            {
               LOGERR("Rialto session state change failed when changing to not running.");
               status = Core::ERROR_GENERAL;
            }
#endif // RIALTO_IN_DAC_FEATURE_ENABLED
            mRuntimeManagerImplLock.Unlock();
            return status;
        }

        Core::hresult RuntimeManagerImplementation::Kill(const string& appInstanceId)
        {
            Core::hresult status = Core::ERROR_GENERAL;
            std::string errorReason = "";
            bool success = false;

#ifdef ENABLE_AIMANAGERS_TELEMETRY_METRICS
            /* Get current timestamp at the start of terminate for telemetry */
            time_t requestTime = getCurrentTimestamp();
#endif

            mRuntimeManagerImplLock.Lock();

#ifdef ENABLE_AIMANAGERS_TELEMETRY_METRICS
            auto it = mRuntimeAppInfo.find(appInstanceId);
            if (it != mRuntimeAppInfo.end())
            {
                it->second.requestTime = requestTime;
                it->second.requestType = REQUEST_TYPE_KILL;
            }
            else
            {
                LOGERR("Kill called for unknown appInstanceId: %s, skipping telemetry update", appInstanceId.c_str());
            }
#endif
            if(isOCIPluginObjectValid())
            {
                string containerId = getContainerId(appInstanceId);

                if (!containerId.empty())
                {
                    status =  mOciContainerObject->StopContainer(containerId, true, success, errorReason);
                    if ((success == false) || (status != Core::ERROR_NONE))
                    {
                        LOGERR("Failed to StopContainer for Kill %s",errorReason.c_str());
                    }
                    else
                    {
                        mUserIdManager->clearUserId(appInstanceId);
                        if (mRuntimeAppInfo.find(appInstanceId) != mRuntimeAppInfo.end())
                        {
                            mRuntimeAppInfo[appInstanceId].containerState = Exchange::IRuntimeManager::RUNTIME_STATE_TERMINATING;
                        }
                    }
                }
                else
                {
                    LOGERR("appInstanceId is not found");
                }
            }
            else
            {
                LOGERR("OCI Plugin object is not valid. Aborting Kill.");
            }
#ifdef RIALTO_IN_DAC_FEATURE_ENABLED
            LOGINFO("Rialto Session deactivate on kill..");
            mRialtoConnector->deactivateSession(mRuntimeAppInfo[appInstanceId].appId);
            if (!mRialtoConnector->waitForStateChange(mRuntimeAppInfo[appInstanceId].appId,RialtoServerStates::NOT_RUNNING, RIALTO_TIMEOUT_MILLIS))
            {
               LOGERR("Rialto session state change failed when changing to not running ");
               status = Core::ERROR_GENERAL;
            }
#endif // RIALTO_IN_DAC_FEATURE_ENABLED
            mRuntimeManagerImplLock.Unlock();
            return status;
        }

        Core::hresult RuntimeManagerImplementation::GetInfo(const string& appInstanceId, string& info)
        {
            Core::hresult status = Core::ERROR_GENERAL;
            LOGINFO("Entered GetInfo Implementation");
            std::string errorReason = "";
            bool success = false;

            mRuntimeManagerImplLock.Lock();

            if(isOCIPluginObjectValid())
            {
                string containerId = getContainerId(appInstanceId);

                if (!containerId.empty())
                {
                    status =  mOciContainerObject->GetContainerInfo(containerId, info, success, errorReason);
                    if ((success == false) || (status != Core::ERROR_NONE))
                    {
                        LOGERR("Failed to GetContainerInfo %s",errorReason.c_str());
                    }
                    else
                    {
                        LOGINFO("GetContainerInfo is success");
                    }
                }
                else
                {
                    LOGERR("appInstanceId is not found or mOciContainerObject is not ready");
                }
            }
            else
            {
                LOGERR("OCI Plugin object is not valid. Aborting GetInfo.");
            }
            mRuntimeManagerImplLock.Unlock();
            return status;
        }

        Core::hresult RuntimeManagerImplementation::Annotate(const string& appInstanceId, const string& key, const string& value)
        {
            Core::hresult status = Core::ERROR_GENERAL;
            std::string errorReason = "";
            bool success = false;

            mRuntimeManagerImplLock.Lock();

            if(isOCIPluginObjectValid())
            {
                string containerId = getContainerId(appInstanceId);

                if (!containerId.empty())
                {
                    if (key.empty())
                    {
                        LOGERR("Annotate: key is empty");
                    }
                    else
                    {
                        status =  mOciContainerObject->Annotate(containerId, key, value, success, errorReason);
                        if ((success == false) || (status != Core::ERROR_NONE))
                        {
                            LOGERR("Failed to Annotate property key: %s value: %s errorReason %s",key.c_str(), value.c_str(), errorReason.c_str());
                        }
                    }
                }
                else
                {
                    LOGERR("appInstanceId is empty ");
                }
            }
            else
            {
                LOGERR("OCI Plugin object is not valid. Aborting GetInfo.");
            }
            mRuntimeManagerImplLock.Unlock();
            return status;
        }

        Core::hresult RuntimeManagerImplementation::Mount()
        {
            Core::hresult status = Core::ERROR_NONE;

            LOGINFO("Mount Implementation - Stub!");

            return status;
        }

        Core::hresult RuntimeManagerImplementation::Unmount()
        {
            Core::hresult status = Core::ERROR_NONE;

            LOGINFO("Unmount Implementation - Stub!");

            return status;
        }

        void RuntimeManagerImplementation::onOCIContainerStartedEvent(std::string name, JsonObject& data)
        {
            dispatchEvent(RuntimeManagerImplementation::RuntimeEventType::RUNTIME_MANAGER_EVENT_CONTAINERSTARTED, data);
        }

        void RuntimeManagerImplementation::onOCIContainerStoppedEvent(std::string name, JsonObject& data)
        {
            dispatchEvent(RuntimeManagerImplementation::RuntimeEventType::RUNTIME_MANAGER_EVENT_CONTAINERSTOPPED, data);
        }

        void RuntimeManagerImplementation::onOCIContainerFailureEvent(std::string name, JsonObject& data)
        {
            dispatchEvent(RuntimeManagerImplementation::RuntimeEventType::RUNTIME_MANAGER_EVENT_CONTAINERFAILED, data);
        }

        void RuntimeManagerImplementation::onOCIContainerStateChangedEvent(std::string name, JsonObject& data)
        {
            dispatchEvent(RuntimeManagerImplementation::RuntimeEventType::RUNTIME_MANAGER_EVENT_STATECHANGED, data);
        }

        void RuntimeManagerImplementation::notifyParameterCheckFailure(const string& appInstanceId, const string& errorCode)
        {
            JsonObject data;
            data["containerId"] = getContainerId(appInstanceId);
            data["errorCode"] = errorCode;
            data["eventName"] = "onParameterCheckFailed";
            dispatchEvent(RuntimeManagerImplementation::RuntimeEventType::RUNTIME_MANAGER_EVENT_CONTAINERFAILED, data);
        }

#ifdef ENABLE_AIMANAGERS_TELEMETRY_METRICS

        time_t RuntimeManagerImplementation::getCurrentTimestamp()
        {
            timespec ts;
            clock_gettime(CLOCK_MONOTONIC, &ts);
            return (((time_t)ts.tv_sec * 1000) + ((time_t)ts.tv_nsec / 1000000));
        }

        RuntimeManagerImplementation::TelemetryMarker RuntimeManagerImplementation::getTelemetryMarker(const std::string& marker)
        {
            if (marker == TELEMETRY_MARKER_LAUNCH_TIME)
                return TELEMETRY_MARKER_LAUNCH;
            else if (marker == TELEMETRY_MARKER_CLOSE_TIME)
                return TELEMETRY_MARKER_CLOSE;
            else if (marker == TELEMETRY_MARKER_RESUME_TIME)
                return TELEMETRY_MARKER_RESUME;
            else if (marker == TELEMETRY_MARKER_SUSPEND_TIME)
                return TELEMETRY_MARKER_SUSPEND;
            else if (marker == TELEMETRY_MARKER_HIBERNATE_TIME)
                return TELEMETRY_MARKER_HIBERNATE;
            else if (marker == TELEMETRY_MARKER_WAKE_TIME)
                return TELEMETRY_MARKER_WAKE;
            else
                return TELEMETRY_MARKER_UNKNOWN;
        }

        void RuntimeManagerImplementation::recordTelemetryData(const std::string& marker, const std::string& appId, uint64_t requestTime)
        {
            /* End time for telemetry */
            time_t currentTime = getCurrentTimestamp();
            LOGERR("End time for %s: %lu", marker.c_str(), currentTime);

            JsonObject jsonParam;
            std::string telemetryMetrics = "";

            int duration = static_cast<int>(currentTime - requestTime);
            TelemetryMarker telemetryMarker = getTelemetryMarker(marker);

            /* Determine the telemetry JSON key */
            switch(telemetryMarker)
            {
                case TELEMETRY_MARKER_RESUME:
                    jsonParam["runtimeManagerResumeTime"] = duration;
                    break;
                case TELEMETRY_MARKER_SUSPEND:
                    jsonParam["runtimeManagerSuspendTime"] = duration;
                    break;
                case TELEMETRY_MARKER_HIBERNATE:
                    jsonParam["runtimeManagerHibernateTime"] = duration;
                    break;
                case TELEMETRY_MARKER_WAKE:
                    jsonParam["runtimeManagerWakeTime"] = duration;
                    break;
                case TELEMETRY_MARKER_LAUNCH:
                    jsonParam["runtimeManagerRunTime"] = duration;
                    break;
                case TELEMETRY_MARKER_CLOSE:
                    jsonParam["runtimeManagerTerminateTime"] = duration;
                    break;
                default:
                    LOGERR("Unknown telemetry marker: %s", marker.c_str());
                    return;
            }
            jsonParam["appId"] = appId;
            jsonParam.ToString(telemetryMetrics);

            if(nullptr != mTelemetryMetricsObject)
            {
                LOGINFO("Record appId %s marker %s start time %d",appId.c_str(), marker.c_str(), duration);
                mTelemetryMetricsObject->Record(appId, telemetryMetrics, marker);
            }
        }
#endif
    } /* namespace Plugin */
} /* namespace WPEFramework */
