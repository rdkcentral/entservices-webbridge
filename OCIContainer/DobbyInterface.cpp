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

#include <sys/stat.h>

#include "DobbyInterface.h"

#include <Dobby/DobbyProxy.h>
#include <Dobby/IpcService/IpcFactory.h>
#include <omi_proxy.hpp>
#include "UtilsJsonRpc.h"

namespace WPEFramework
{
namespace Plugin
{
namespace WPEC = WPEFramework::Core;
namespace WPEJ = WPEFramework::Core::JSON;

DobbyInterface::DobbyInterface(): mEventHandler(nullptr)
{
}

DobbyInterface::~DobbyInterface()
{
}

bool DobbyInterface::initialize(IEventHandler* eventHandler)
{
    mEventHandler = eventHandler;
    mIpcService = AI_IPC::createIpcService("unix:path=/var/run/dbus/system_bus_socket", "com.sky.dobby.thunder");

    if (!mIpcService)
    {
        LOGERR("Failed to create IPC service");
        return false;
    }
    else
    {
        // Start the IPCService which kicks off the event dispatcher thread
        mIpcService->start();
    }

    // Create a DobbyProxy remote service that wraps up the dbus API
    // calls to the Dobby daemon
    mDobbyProxy = std::make_shared<DobbyProxy>(mIpcService, DOBBY_SERVICE, DOBBY_OBJECT);

    // Register a state change event listener
    mEventListenerId = mDobbyProxy->registerListener(stateListener, static_cast<const void*>(this));

    mOmiProxy = std::make_shared<omi::OmiProxy>();

    mOmiListenerId = mOmiProxy->registerListener(omiErrorListener, static_cast<const void*>(this));

    return true;
}

void DobbyInterface::terminate()
{
    mEventHandler = nullptr;
    mDobbyProxy->unregisterListener(mEventListenerId);
    mOmiProxy->unregisterListener(mOmiListenerId);
}

bool DobbyInterface::listContainers(string& containers, string& errorReason)
{
    if (nullptr == mDobbyProxy)
    {
        errorReason = "dobby environment is not ready";
        return false;
    }
    // Get the running containers from Dobby
    std::list<std::pair<int32_t, std::string>> runningContainers;
    JsonObject containerJson;
    JsonArray containerArray;

    runningContainers = mDobbyProxy->listContainers();

    // Build the response if containers were found
    if (!runningContainers.empty())
    {
        for (const std::pair<int32_t, std::string>& c : runningContainers)
        {
            containerJson["Descriptor"] = c.first;
            containerJson["Id"] = c.second;

            containerArray.Add(containerJson);
        }
    }

    if (!WPEFramework::Core::JSON::IElement::ToString(containerArray, containers))
    {
        errorReason = "Failed to convert Dobby spec to string";
        return false;
    }
    return true;
}

bool DobbyInterface::getContainerState(const string& containerId, Exchange::IOCIContainer::ContainerState& state, string& errorReason)
{
    if (nullptr == mDobbyProxy)
    {
        errorReason = "dobby environment is not ready";
        return false;
    }

    int cd = GetContainerDescriptorFromId(containerId);
    if (cd < 0)
    {
        state = Exchange::IOCIContainer::ContainerState::INVALID;
        return false;
    }

    // We got a state back successfully, work out what that means in English
    switch (static_cast<IDobbyProxyEvents::ContainerState>(mDobbyProxy->getContainerState(cd)))
    {
    case IDobbyProxyEvents::ContainerState::Invalid:
        state = Exchange::IOCIContainer::ContainerState::INVALID;
        break;
    case IDobbyProxyEvents::ContainerState::Starting:
        state = Exchange::IOCIContainer::ContainerState::STARTING;
        break;
    case IDobbyProxyEvents::ContainerState::Running:
        state = Exchange::IOCIContainer::ContainerState::RUNNING;
        break;
    case IDobbyProxyEvents::ContainerState::Stopping:
        state = Exchange::IOCIContainer::ContainerState::STOPPING;
        break;
    case IDobbyProxyEvents::ContainerState::Paused:
        state = Exchange::IOCIContainer::ContainerState::PAUSED;
        break;
    case IDobbyProxyEvents::ContainerState::Stopped:
        state = Exchange::IOCIContainer::ContainerState::STOPPED;
        break;
    case IDobbyProxyEvents::ContainerState::Hibernating:
        state = Exchange::IOCIContainer::ContainerState::HIBERNATING;
        break;
    case IDobbyProxyEvents::ContainerState::Hibernated:
        state = Exchange::IOCIContainer::ContainerState::HIBERNATED;
        break;
    case IDobbyProxyEvents::ContainerState::Awakening:
        state = Exchange::IOCIContainer::ContainerState::AWAKENING;
        break;
    default:
        state = Exchange::IOCIContainer::ContainerState::INVALID;
        break;
    }

    return true;
}

bool DobbyInterface::getContainerInfo(const string& containerId, string& info, string& errorReason)
{
    if (nullptr == mDobbyProxy)
    {
        errorReason = "dobby environment is not ready";
        return false;
    }
    std::string id = containerId;

    int cd = GetContainerDescriptorFromId(id);
    if (cd < 0)
    {
        return false;
    }

    info = mDobbyProxy->getContainerInfo(cd);

    if (info.empty())
    {
        errorReason = "Failed to get info for container";
        return false;
    }

    return true;
}

static bool is_encrypted(const std::string &bundlePath) {
    struct stat st;

    std::string str = bundlePath + "rootfs.img";

    if (stat(str.c_str(), &st))
    {
        return false;
    }

    if (!(st.st_mode & S_IFREG || st.st_mode & S_IFLNK))
    {
        return false;
    }

    str = bundlePath + "config.json.jwt";

    if (stat(str.c_str(), &st))
    {
        return false;
    }

    if (!(st.st_mode & S_IFREG || st.st_mode & S_IFLNK))
    {
        return false;
    }

    return true;
}

bool DobbyInterface::startContainer(const string& containerId, const string& bundlePath, const string& command, const string& westerosSocket, int32_t& descriptor, string& errorReason)
{
    LOGINFO("Start container from OCI bundle");
    if (nullptr == mDobbyProxy)
    {
        errorReason = "dobby environment is not ready";
        return false;
    }

    std::string id = containerId;

    // Can be used to pass file descriptors to container construction.
    // Currently unsupported, see DobbyProxy::startContainerFromBundle().
    std::list<int> emptyList;

    std::string containerPath;

    const bool encrypted = is_encrypted(bundlePath);

    if (encrypted && !mOmiProxy->mountCryptedBundle(id,
                                       bundlePath + "rootfs.img",
                                       bundlePath + "config.json.jwt",
                                       containerPath))
    {
        LOGERR("Failed to start container - sync mount request to omi failed.");
        errorReason = "mount failed";
        return false;
    }

    if (encrypted)
    {
        LOGINFO("Mount request to omi succeeded, contenerPath: %s", containerPath.c_str());
    }


    // If no additional arguments, start the container
    if ((command == "null" || command.empty()) && (westerosSocket == "null" || westerosSocket.empty()))
    {
        LOGINFO("startContainerFromBundle: id: %s, containerPath: %s", id.c_str(), encrypted ? containerPath.c_str() : bundlePath.c_str());
        descriptor = mDobbyProxy->startContainerFromBundle(id, encrypted ? containerPath : bundlePath, emptyList);
    }
    else
    {
        std::string commandModified = command, westerosSocketModified = westerosSocket;
        // Dobby expects empty strings if values not set
        if (command == "null" || command.empty())
        {
            commandModified = "";
        }
        if (westerosSocket == "null" || westerosSocket.empty())
        {
            westerosSocketModified = "";
        }

        LOGINFO("startContainerFromBundle: id: %s, containerPath: %s, command: %s, westerosSocket: %s", id.c_str(), encrypted ? containerPath.c_str() : bundlePath.c_str(), commandModified.c_str(), westerosSocketModified.c_str());
        descriptor = mDobbyProxy->startContainerFromBundle(id, encrypted ? containerPath : bundlePath, emptyList, commandModified, westerosSocketModified);
    }

    // startContainer returns -1 on failure
    if (descriptor <= 0)
    {
        LOGERR("Failed to start container - internal Dobby error.");

        if (encrypted && !mOmiProxy->umountCryptedBundle(id.c_str()))
        {
            LOGERR("Failed to umount container %s - sync unmount request to omi failed.", id.c_str());
            errorReason = "dobby start failed, unmount failed";
        }
        else
        {
            errorReason = "dobby start failed";
        }

        return false;
    }

    return true;
}

bool DobbyInterface::startContainerFromDobbySpec(const string& containerId, const string& dobbySpec, const string& command, const string& westerosSocket, int32_t& descriptor, string& errorReason)
{
    LOGINFO("Start container from Dobby spec");
    if (nullptr == mDobbyProxy)
    {
        errorReason = "dobby environment is not ready";
        return false;
    }

    // To start a container, we need a Dobby spec and an ID for the container
    std::string id = containerId;

    // Can be used to pass file descriptors to container construction.
    // Currently unsupported, see DobbyProxy::startContainerFromSpec().
    std::list<int> emptyList;

    // If no additional arguments, start the container
    if ((command == "null" || command.empty()) && (westerosSocket == "null" || westerosSocket.empty()))
    {
        descriptor = mDobbyProxy->startContainerFromSpec(id, dobbySpec, emptyList);
    }
    else
    {
        std::string commandModified = command, westerosSocketModified = westerosSocket;
        // Dobby expects empty strings if values not set
        if (command == "null" || command.empty())
        {
            commandModified = "";
        }
        if (westerosSocket == "null" || westerosSocket.empty())
        {
            westerosSocketModified = "";
        }
        descriptor = mDobbyProxy->startContainerFromSpec(id, dobbySpec, emptyList, commandModified, westerosSocketModified);
    }

    // startContainer returns -1 on failure
    if (descriptor <= 0)
    {
        LOGERR("Failed to start container - internal Dobby error.");
        return false;
    }

    return true;
}

bool DobbyInterface::stopContainer(const string& containerId, bool force, string& errorReason)
{
    LOGINFO("Stop container");
    if (nullptr == mDobbyProxy)
    {
        errorReason = "dobby environment is not ready";
        return false;
    }
    // Need to have an ID to stop
    std::string id = containerId;

    int cd = GetContainerDescriptorFromId(id);
    if (cd < 0)
    {
        errorReason = "Container not found";
        return false;
    }

    bool forceStop = force;
    LOGINFO("Force stop is %d", forceStop);

    bool stoppedSuccessfully = mDobbyProxy->stopContainer(cd, forceStop);

    if (!stoppedSuccessfully)
    {
        LOGERR("Failed to stop container - internal Dobby error.");
        return false;
    }

    return true;
}

bool DobbyInterface::pauseContainer(const string& containerId, string& errorReason)
{
    LOGINFO("Pause container");
    if (nullptr == mDobbyProxy)
    {
        errorReason = "dobby environment is not ready";
        return false;
    }

    // Need to have an ID to pause
    std::string id = containerId;

    int cd = GetContainerDescriptorFromId(id);
    if (cd < 0)
    {
        return false;
    }

    bool pausedSuccessfully = mDobbyProxy->pauseContainer(cd);

    if (!pausedSuccessfully)
    {
        LOGERR("Failed to pause container - internal Dobby error.");
        return false;
    }

    return true;
}

bool DobbyInterface::resumeContainer(const string& containerId, string& errorReason)
{
    LOGINFO("Resume container");
    if (nullptr == mDobbyProxy)
    {
        errorReason = "dobby environment is not ready";
        return false;
    }

    // Need to have an ID to resume
    std::string id = containerId;

    int cd = GetContainerDescriptorFromId(id);
    if (cd < 0)
    {
        return false;
    }

    bool resumedSuccessfully = mDobbyProxy->resumeContainer(cd);

    if (!resumedSuccessfully)
    {
        LOGERR("Failed to resume container - internal Dobby error.");
        return false;
    }

    return true;
}

bool DobbyInterface::hibernateContainer(const string& containerId, const string& options, string& errorReason)
{
    LOGINFO("Hibernate container");
    if (nullptr == mDobbyProxy)
    {
        errorReason = "dobby environment is not ready";
        return false;
    }

    // Need to have an ID to resume
    std::string id = containerId;

    int cd = GetContainerDescriptorFromId(id);
    if (cd < 0)
    {
        return false;
    }

    bool hibernateSuccessfully = mDobbyProxy->hibernateContainer(cd, options);

    if (!hibernateSuccessfully)
    {
        LOGERR("Failed to hibernate container - internal Dobby error.");
        return false;
    }

    return true;
}

bool DobbyInterface::wakeupContainer(const string& containerId, string& errorReason)
{
    LOGINFO("wakeup container");
    if (nullptr == mDobbyProxy)
    {
        errorReason = "dobby environment is not ready";
        return false;
    }

    // Need to have an ID to resume
    std::string id = containerId;

    int cd = GetContainerDescriptorFromId(id);
    if (cd < 0)
    {
        return false;
    }

    bool wakedUpSuccessfully = mDobbyProxy->wakeupContainer(cd);

    if (!wakedUpSuccessfully)
    {
        LOGERR("Failed to wakeup container - internal Dobby error.");
        return false;
    }

    return true;
}

bool DobbyInterface::executeCommand(const string& containerId, const string& options, const string& command, string& errorReason)
{
    LOGINFO("Execute command inside container");
    if (nullptr == mDobbyProxy)
    {
        errorReason = "dobby environment is not ready";
        return false;
    }

    std::string id = containerId;

    int cd = GetContainerDescriptorFromId(id);
    if (cd < 0)
    {
        return false;
    }

    bool executedSuccessfully = mDobbyProxy->execInContainer(cd, options, command);

    if (!executedSuccessfully)
    {
        LOGERR("Failed to execute command in container - internal Dobby error.");
        return false;
    }

    return true;
}

bool DobbyInterface::annotate(const string& containerId, const string& key, const string& value, string& errorReason)
{
    LOGINFO("Add annotation in container");
    if (nullptr == mDobbyProxy)
    {
        errorReason = "dobby environment is not ready";
        return false;
    }

    std::string id = containerId;

    int cd = GetContainerDescriptorFromId(id);
    if (cd < 0)
    {
        return false;
    }

    bool annotatedSuccessfully = mDobbyProxy->addAnnotation(cd, key, value);

    if (!annotatedSuccessfully)
    {
        LOGERR("Failed to add annotation in container - internal Dobby error.");
        return false;
    }

    return true;
}

bool DobbyInterface::removeAnnotation(const string& containerId, const string& key, string& errorReason)
{
    LOGINFO("remove annotation in container");
    if (nullptr == mDobbyProxy)
    {
        errorReason = "dobby environment is not ready";
        return false;
    }

    std::string id = containerId;

    int cd = GetContainerDescriptorFromId(id);
    if (cd < 0)
    {
        return false;
    }

    bool removeAnnotationSuccessfully = mDobbyProxy->removeAnnotation(cd, key);

    if (!removeAnnotationSuccessfully)
    {
        LOGERR("Failed to remove annotation in container - internal Dobby error.");
        return false;
    }

    return true;
}

bool DobbyInterface::mount(const string& containerId, const string& source, const string& target, const string& type, const string& options, string& errorReason)
{
    LOGINFO("mount in container");
    if (nullptr == mDobbyProxy)
    {
        errorReason = "dobby environment is not ready";
        return false;
    }

    std::string id = containerId;

    int cd = GetContainerDescriptorFromId(id);
    if (cd < 0)
    {
        return false;
    }

    std::vector<std::string> mountFlags;
    //TODO Populate mount flags and data
    bool mountSuccessfully = mDobbyProxy->addContainerMount(cd, source, target, mountFlags, "");

    if (!mountSuccessfully)
    {
        LOGERR("Failed to mount in container - internal Dobby error.");
        return false;
    }

    return true;
}

bool DobbyInterface::unmount(const string& containerId, const string& target, string& errorReason)
{
    LOGINFO("unmount from container");
    if (nullptr == mDobbyProxy)
    {
        errorReason = "dobby environment is not ready";
        return false;
    }

    std::string id = containerId;

    int cd = GetContainerDescriptorFromId(id);
    if (cd < 0)
    {
        return false;
    }

    bool unmountSuccessfully = mDobbyProxy->removeContainerMount(cd, target);

    if (!unmountSuccessfully)
    {
        LOGERR("Failed to unmount from container - internal Dobby error.");
        return false;
    }

    return true;
}

/**
 * @brief Send an event notifying that a container has started.
 *
 * @param descriptor    Container descriptor.
 * @param name          Container name.
 */
void DobbyInterface::onContainerStarted(int32_t descriptor, const std::string& name)
{
    JsonObject params;
    params["descriptor"] = std::to_string(descriptor);
    params["name"] = name;

    string containerId = GetContainerIdFromDescriptor(descriptor);
    params["containerId"] = (containerId.empty()) ? name : containerId;

    if (mEventHandler)
    {
        mEventHandler->onContainerStarted(params);
    }
}

/**
 * @brief Send an event notifying that a container has stopped.
 *
 * @param descriptor    Container descriptor.
 * @param name          Container name.
 */
void DobbyInterface::onContainerStopped(int32_t descriptor, const std::string& name)
{

    if (!mOmiProxy->umountCryptedBundle(name))
    {
        LOGERR("Failed to umount container %s - sync unmount request to omi failed.", name.c_str());
    }
    else
    {
        LOGINFO("Container %s properly unmounted.", name.c_str());
    }

    JsonObject params;
    params["descriptor"] = std::to_string(descriptor);
    params["name"] = name;
    string containerId = GetContainerIdFromDescriptor(descriptor);
    params["containerId"] = (containerId.empty()) ? name : containerId;

    if (mEventHandler)
    {
        mEventHandler->onContainerStopped(params);
    }
}

void DobbyInterface::onContainerStateChanged(int32_t descriptor, const std::string& name, IDobbyProxyEvents::ContainerState dobbyContainerState)
{
    Exchange::IOCIContainer::ContainerState state ;
    switch (dobbyContainerState)
    {
    case IDobbyProxyEvents::ContainerState::Starting:
        state = Exchange::IOCIContainer::ContainerState::STARTING;
        break;
    case IDobbyProxyEvents::ContainerState::Running:
        state = Exchange::IOCIContainer::ContainerState::RUNNING;
        break;
    case IDobbyProxyEvents::ContainerState::Stopping:
        state = Exchange::IOCIContainer::ContainerState::STOPPING;
        break;
    case IDobbyProxyEvents::ContainerState::Paused:
        state = Exchange::IOCIContainer::ContainerState::PAUSED;
        break;
    case IDobbyProxyEvents::ContainerState::Stopped:
        state = Exchange::IOCIContainer::ContainerState::STOPPED;
        break;
    case IDobbyProxyEvents::ContainerState::Hibernating:
        state = Exchange::IOCIContainer::ContainerState::HIBERNATING;
        break;
    case IDobbyProxyEvents::ContainerState::Hibernated:
        state = Exchange::IOCIContainer::ContainerState::HIBERNATED;
        break;
    case IDobbyProxyEvents::ContainerState::Awakening:
        state = Exchange::IOCIContainer::ContainerState::AWAKENING;
        break;
    default:
        state = Exchange::IOCIContainer::ContainerState::INVALID;
        break;
    }
    JsonObject params;
    params["state"] = static_cast<int>(state);
    string containerId = GetContainerIdFromDescriptor(descriptor);
    params["containerId"] = (containerId.empty()) ? name : containerId;

    LOGINFO("OnContainerStateChanged event state %u",static_cast<int>(state));

    if (mEventHandler)
    {
        mEventHandler->onContainerStateChange(params);
    }
}

// Begin Internal Methods

/**
 * @brief Converts the OCI container ID into the internal Dobby descriptor int
 *
 * Will only return a value if Dobby knows about the running container
 * (e.g. the container was started by Dobby, not manually using the OCI runtime).
 *
 * @param containerId The container ID used by the OCI runtime - not the Dobby descriptor
 *
 * @return Descriptor value
 */
const int DobbyInterface::GetContainerDescriptorFromId(const std::string& containerId)
{
    const std::list<std::pair<int32_t, std::string>> containers = mDobbyProxy->listContainers();

    for (const std::pair<int32_t, std::string>& container : containers)
    {
        char strDescriptor[32];
        snprintf(strDescriptor, sizeof(strDescriptor), "%d", container.first);

        if ((containerId == strDescriptor) || (containerId == container.second))
        {
            return container.first;
        }
    }

    LOGERR("Failed to find container %s", containerId.c_str());
    return -1;
}

/**
 * @brief Converts the dobbyDescriptor into the OCI Container ID
 *
 * Will only return a value if Dobby knows about the running container
 * (e.g. the container was started by Dobby, not manually using the OCI runtime).
 *
 * @param Descriptor value
 *
 * @return  containerId The container ID used by the OCI runtime
 */
const std::string DobbyInterface::GetContainerIdFromDescriptor(const int descriptor)
{
    /* Retrieve the list of containers */
    const std::list<std::pair<int32_t, std::string>> containers = mDobbyProxy->listContainers();

    /* Iterate over the containers to find the one with the matching descriptor */
    for (const std::pair<int32_t, std::string>& container : containers)
    {
        if (container.first == descriptor)
        {
            /* If the descriptor matches, return the container's ID */
            return container.second;
        }
    }

    /* If no container is found with the given descriptor, log an error */
    LOGERR("Failed to find container with descriptor %d", descriptor);
    return "";  /* Return an empty string to indicate failure */
}


/**
 * @brief Callback listener for state change events.
 *
 * @param descriptor Container descriptor
 * @param name       Container name
 * @param state      Container state
 * @param _this      Callback parameters, or in this case, the pointer to 'this'
 */
const void DobbyInterface::stateListener(int32_t descriptor, const std::string& name, IDobbyProxyEvents::ContainerState state, const void* _this)
{
    // Cast const void* back to DobbyInterface* type to get 'this'
    DobbyInterface* __this = const_cast<DobbyInterface*>(reinterpret_cast<const DobbyInterface*>(_this));

    __this->onContainerStateChanged(descriptor, name, state);

    if (state == IDobbyProxyEvents::ContainerState::Running)
    {
        __this->onContainerStarted(descriptor, name);
    }
    else if (state == IDobbyProxyEvents::ContainerState::Stopped)
    {
        __this->onContainerStopped(descriptor, name);
    }
    else
    {
        LOGINFO("Received an unknown state event for container '%s'.", name.c_str());
    }
}

/**
 * @brief Callback listener for OMI error events.
 *
 * @param id         Container id
 * @param err        Error type
 * @param _this      Callback parameters, or in this case, the pointer to 'this'
 */
const void DobbyInterface::omiErrorListener(const std::string& id, omi::IOmiProxy::ErrorType err, const void* _this)
{

    // Cast const void* back to DobbyInterface* type to get 'this'
    DobbyInterface* __this = const_cast<DobbyInterface*>(reinterpret_cast<const DobbyInterface*>(_this));

    if (__this != nullptr && err == omi::IOmiProxy::ErrorType::verityFailed)
    {
        __this->onVerityFailed(id);
    }
    else
    {
        LOGINFO("Received an unknown error type from OMI");
    }
}

/**
 * @brief Handle failure of verity check for container.
 *
 * @param name          Container name.
 */
void DobbyInterface::onVerityFailed(const std::string& name)
{
    LOGINFO("Handle onVerityFailed");
    int cd = GetContainerDescriptorFromId(name);
    if (cd < 0)
    {
        LOGERR("Failed to acquire container descriptor - cannot stop container due to verity failure");
        return;
    }

    JsonObject params;
    params["descriptor"] = cd;
    params["name"] = name;
    string containerId = GetContainerIdFromDescriptor(cd);
    params["containerId"] = (containerId.empty()) ? name : containerId;
    // Set error type to Verity Error (1)
    params["errorCode"] = 1;
    if (mEventHandler)
    {
        mEventHandler->onContainerFailed(params);
    }

    bool stoppedSuccessfully = mDobbyProxy->stopContainer(cd, true);

    if (!stoppedSuccessfully)
    {
        LOGERR("Failed to stop container - internal Dobby error.");
        return;
    }
}

// End Internal methods

} // namespace Plugin
} // namespace WPEFramework
