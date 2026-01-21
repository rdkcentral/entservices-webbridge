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

#include "DobbySpecGenerator.h"
#include "ApplicationConfiguration.h"
#include "UtilsLogging.h"
#include <sys/mount.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <bitset>
#include <set>
#include <sys/sysinfo.h>
//TODO SUPPORT THIS
//#include <IPackage.h>
#include <curl/curl.h>

namespace WPEFramework
{
namespace Plugin
{

namespace 
{
    #define XDG_RUNTIME_DIR "/tmp"
}

DobbySpecGenerator::DobbySpecGenerator(): mIonMemoryPluginData(Json::objectValue), mPackageMountPoint("/package"), mRuntimeMountPoint("/runtime"), mGstRegistrySourcePath(""), mGstRegistryDestinationPath("/tmp/gstreamer-cached-registry.bin")
{
    LOGINFO("DobbySpecGenerator()");
    mAIConfiguration = new AIConfiguration();
    mAIConfiguration->initialize();
    initialiseIonHeapsJson();
//TODO SUPPORT THIS
/*
    if (mAIConfiguration->getGstreamerRegistryEnabled())
    {
	GStreamerRegistry gstRegistry;
        if (gstRegistry.generate())
        {
            mGstRegistrySourcePath = gstRegistry.path();
	}
    }
*/
}

DobbySpecGenerator::~DobbySpecGenerator()
{
    LOGINFO("~DobbySpecGenerator()");
    if (nullptr != mAIConfiguration)
    {
        delete mAIConfiguration;
    }
}

Json::Value DobbySpecGenerator::getWorkingDir(const ApplicationConfiguration& config, const WPEFramework::Exchange::RuntimeConfig& runtimeConfig) const
{
    // default to the package directory
    std::string workingDir(mPackageMountPoint);

    // TODO SUPPORT execFilePath in runtime config - appPath
    /*
    char *execFilePathCopy = strdup(appPackage->execFilePath().c_str()); //Get it from metadata path
    const char* execFileDir = dirname(execFilePathCopy);
    if (execFileDir && (strcmp(execFileDir, ".") != 0))
        workingDir += execFileDir;

    free(execFilePathCopy);
    */
    return Json::Value(workingDir);
}

bool DobbySpecGenerator::generate(const ApplicationConfiguration& config, const WPEFramework::Exchange::RuntimeConfig& runtimeConfig, string& resultSpec)
{
    LOGINFO("DobbySpecGenerator::generate()");
    resultSpec = "";

    std::ifstream inFile("/tmp/specchange");
    if (inFile.good())
    {
        inFile.open("/tmp/specchange"); //open the input file
        std::stringstream strStream;
        strStream << inFile.rdbuf(); //read the file
        resultSpec = strStream.str(); //str holds the content of the file
        std::cout << resultSpec << "\n"; //you can do anything with the string!!!
        inFile.close();
        JsonObject parameters;
        parameters.FromString(resultSpec.c_str());
        return true;
    }
   
    ssize_t memLimit = getSysMemoryLimit(config, runtimeConfig);
    /*mandatory parameter check*/
    if((memLimit <= 0) || runtimeConfig.command.empty() || (config.mUserId ==0))
    {
        LOGERR("Invalid mandatory parameter: Mem Limit=%d, Command=%s UserId %d", memLimit, runtimeConfig.command.c_str(), config.mUserId);
        return false;
    }

    Json::Value spec;
    spec["version"] = "1.1";
    spec["memLimit"] = std::move(memLimit);

    Json::Value args(Json::arrayValue);
    std::string command ="";
    if(!runtimeConfig.runtimePath.empty())
    {
        command = "/runtime/";
    }
    else
    {
        command = "/package/";
    }
    command.append(runtimeConfig.command);
    args.append(command);
    spec["args"] = std::move(args);
    spec["cwd"] = getWorkingDir(config, runtimeConfig);
    if (shouldEnableGpu(config))
    {
        Json::Value gpuObj(Json::objectValue);
        gpuObj["enable"] = true;
        gpuObj["memLimit"] = getGPUMemoryLimit(config, runtimeConfig);
        spec["gpu"] = std::move(gpuObj);
    }
    spec["restartOnCrash"] = false;

    Json::Value vpuObj;
    vpuObj["enable"] = getVpuEnabled(config, runtimeConfig);
    spec["vpu"] = std::move(vpuObj);
    
    //TODO CHECK FOR OPTIMIZATION
    std::list<std::string> dbusRequiringApps = mAIConfiguration->getAppsRequiringDBus(); 
    for (auto it = dbusRequiringApps.begin(); it != dbusRequiringApps.end(); ++it)
    {
        std::string appId = *it;
        if (appId.compare(config.mAppId) == 0)
	{
            Json::Value dbusObj(Json::objectValue);
            dbusObj["system"] = "system";
            spec["dbus"] = std::move(dbusObj);
	}
    }
    Json::Value cpuObj;
    cpuObj["cores"] = getCpuCores();
    spec["cpu"] = std::move(cpuObj);

#if (AI_BUILD_TYPE == AI_DEBUG)
    Json::Value consoleObj;
    consoleObj["limit"] = mAIConfiguration->getContainerConsoleLogCap();
    //TODO: SUPPORT Read console path from runtime config: RDKEMW-4432
    consoleObj["path"] = "/tmp/container.log";
    spec["console"] = std::move(consoleObj);
#endif // (AI_BUILD_TYPE == AI_DEBUG)

    Json::Value etcObj;
    Json::Value hostsArray(Json::arrayValue);
    Json::Value servicesArray(Json::arrayValue);
    // always add a local host entry
    hostsArray.append("127.0.0.1\tlocalhost");

    // add some common services for the app
    servicesArray.append("ftp\t\t21/tcp");
    servicesArray.append("domain\t\t53/tcp");
    servicesArray.append("domain\t\t53/udp");
    servicesArray.append("http\t\t80/tcp\t\twww");
    servicesArray.append("http\t\t80/udp");
    servicesArray.append("ntp\t\t123/udp");
    servicesArray.append("https\t\t443/tcp");
    servicesArray.append("https\t\t443/udp");

    //TODO SUPPORT mapi in runtime config: RDKEMW-4432
    //if (runtimeConfig.mapi)
    if (true)
    {
        for (int port : mAIConfiguration->getMapiPorts())
        {
            servicesArray.append("mapi\t\t" + std::to_string(port) + "/tcp");
        }
    }

    Json::Value preloadsArray(Json::arrayValue);
    //TODO CHECK FOR OPTIMIZATION
    std::list<std::string> preloads = mAIConfiguration->getPreloads();
    for (auto it = preloads.begin(); it != preloads.end(); ++it)
    {
        preloadsArray.append(*it);
    }

    etcObj["hosts"] = hostsArray;
    etcObj["services"] = servicesArray;
    etcObj["ld-preload"] = preloadsArray;
    spec["etc"] = std::move(etcObj);

    if (runtimeConfig.wanLanAccess)
    {
        spec["network"] = "nat";
    }
    else
    {
        spec["network"] = "private";
    }
    Json::Value userObj;
    userObj["uid"] = config.mUserId;
    userObj["gid"] = config.mGroupId;
    spec["user"] = std::move(userObj);

    //spec["plugins"] = populateClassicPlugins(config, runtimeConfig);
    populateClassicPlugins(config, runtimeConfig, spec);
    spec["rdkPlugins"] = createRdkPlugins(config, runtimeConfig);
    spec["mounts"] = createMounts(config, runtimeConfig);
    spec["env"] = createEnvVars(config, runtimeConfig);

#ifdef RDK_APPMANAGERS_DEBUG
    addHolePunchPortToSpec(spec, 22222);
#endif

    Json::FastWriter writer;
    resultSpec = writer.write(spec);
    LOGINFO("spec: '%s'\n", resultSpec.c_str());

    std::ofstream generatedSpecFile;
    generatedSpecFile.open("/tmp/generatedSpec.json");
    generatedSpecFile << resultSpec.c_str();
    generatedSpecFile.close();

    return true;
}

Json::Value DobbySpecGenerator::createEnvVars(const ApplicationConfiguration& config, const WPEFramework::Exchange::RuntimeConfig& runtimeConfig) const
{
    Json::Value env(Json::arrayValue);
    env.append(std::string("APPLICATION_NAME=") + config.mAppId);
    
     //TODO YET TO ANALYZE SUPPORT APPLICATION_LAUNCH_PARAMETERS
     //TODO YET TO ANALYZE SUPPORT APPLICATION_LAUNCH_METHOD
     //TODO YET TO ANALYZE SUPPORT APPLICATION_TOKEN

   JsonArray envInputArray;
   envInputArray.FromString(runtimeConfig.envVariables);
   for (unsigned int i = 0; i < envInputArray.Length(); ++i)
   {
       std::string envInputItem = envInputArray[i].String();
       env.append(envInputArray[i].String());
   }

   std::list<std::string> configEnvs = mAIConfiguration->getEnvs();
   for (auto it = configEnvs.begin(); it != configEnvs.end(); ++it)
   {
       env.append(*it);
   }

   if (!config.mWesterosSocketPath.empty())
   {
       env.append("XDG_RUNTIME_DIR=/tmp");
       env.append("WAYLAND_DISPLAY=westeros");
       env.append("WESTEROS_SINK_VIRTUAL_WIDTH=1920");
       env.append("WESTEROS_SINK_VIRTUAL_HEIGHT=1080");
       env.append("QT_WAYLAND_CLIENT_BUFFER_INTEGRATION=wayland-egl");
       env.append("QT_WAYLAND_SHELL_INTEGRATION=wl-simple-shell");
       env.append("QT_WAYLAND_INPUTDEVICE_INTEGRATION=skyq-input");
       env.append("QT_QPA_PLATFORM=wayland-sky-rdk");
   }
   if (mAIConfiguration->getResourceManagerClientEnabled())
   {
       env.append("ESSRMGR_APPID=" + config.mAppId);
       env.append("CLIENT_IDENTIFIER=" + config.mAppId);
       if (!config.mWesterosSocketPath.empty())
       {
           env.append("WESTEROS_SINK_USE_ESSRMGR=1");
       }
   }	     

   if (runtimeConfig.dial)
   {
       std::string dialId = runtimeConfig.dialId;
       env.append(std::string("APPLICATION_DIAL_NAME=") + dialId);
       std::ostringstream dataUrlStream;
       dataUrlStream << "http://127.0.0.1:"
                     << mAIConfiguration->getDialServerPort() << '/'
                     << mAIConfiguration->getDialServerPathPrefix() << '/'
                     << dialId << '/'
                     << "dial_data";

       std::string dataUrl = encodeURL(dataUrlStream.str());
       env.append(std::string("ADDITIONAL_DATA_URL=") + dataUrl);
       env.append(std::string("DIAL_USN=") + mAIConfiguration->getDialUsn());
   }

   //TODO SUPPORT RIALTO
   //TODO SUPPORT rialto in runtime config
   //if (rialtoSMClient && appPackage->hasCapability(IPackage::Capability::RequiresRialto))
   if (false)
   {
       //const std::string rialtoSocketPath = rialtoSMClient->getSocketPath();
       //if (!rialtoSocketPath.empty())
       //{
       //    // Pass Rialto socket name used by RialtoClient to communicate with RialtoSessionServer
       //    env.append(std::string("RIALTO_SOCKET_PATH=") + rialtoSocketPath);
       //}
   }
   else if (!mGstRegistrySourcePath.empty())
   {
       env.append("GST_REGISTRY=" + mGstRegistryDestinationPath);
       env.append("GST_REGISTRY_UPDATE=no");
   }

   //TODO SUPPORT WATCHDOG
   //TODO SUPPORT runtime parameter in runtime config: RDKEMW-4432
   //TODO SUPPORT Add only for web runtime
   env.append("WEBKIT_LEGACY_INSPECTOR_SERVER=0.0.0.0:22222");

   return env;
}

Json::Value DobbySpecGenerator::createMounts(const ApplicationConfiguration& config, const WPEFramework::Exchange::RuntimeConfig& runtimeConfig) const
{
    Json::Value mounts(Json::arrayValue);

    if (!runtimeConfig.appPath.empty())
    {
        mounts.append(createBindMount(runtimeConfig.appPath, mPackageMountPoint, MS_BIND | MS_RDONLY | MS_NOSUID | MS_NODEV));
    }

    if (!runtimeConfig.runtimePath.empty())
    {
        mounts.append(createBindMount(runtimeConfig.runtimePath, mRuntimeMountPoint, MS_BIND | MS_RDONLY | MS_NOSUID | MS_NODEV));
    }

    mounts.append(createBindMount("/etc/ssl/certs", "/etc/ssl/certs",
                               (MS_BIND | MS_RDONLY | MS_NOSUID | MS_NODEV)));

    mounts.append(createPrivateDataMount(runtimeConfig));
    
    createFkpsMounts(config, runtimeConfig, mounts);

    if (!config.mWesterosSocketPath.empty())
    {
        if (mAIConfiguration->getResourceManagerClientEnabled())
        {
            // bind mount the resource manager socket into the container
            Json::Value resmgrMount = createResourceManagerMount(config);
            if (!resmgrMount.isNull())
                mounts.append(std::move(resmgrMount));
        }
    }

    //TODO SUPPORT Handle rialto
    //TODO SUPPORT Netflix specific mounts
    //TODO SUPPORT SVP file mounts
    //TODO SUPPORT Platform specific mounts
    //TODO SUPPORT Airplay specific mounts
    //TODO SUPPORT TSB Storage
    //TODO SUPPORT EPG specific migration data store mount
    //TODO SUPPORT USB Mass storage
    //TODO SUPPORT PerfettoSocketPath not mounted
    /*
    if (usingRialto)
    {
        Json::Value rialtoMount = createRialtoMount(appPackage, rialtoSMClient);
        if (!rialtoMount.isNull())
            mountsArray.append(std::move(rialtoMount));
    }
    */
    if (!mGstRegistrySourcePath.empty())
    {
        mounts.append(createBindMount(mGstRegistrySourcePath,
                                           mGstRegistryDestinationPath,
                                           (MS_BIND | MS_NOSUID | MS_NODEV | MS_NOEXEC | MS_RDONLY)));
    }


    return mounts;
}

Json::Value DobbySpecGenerator::createBindMount(const std::string& source,
                                                const std::string& destination,
                                                unsigned long mountFlags) const
{
    Json::Value mount(Json::objectValue);

    mount["source"] = source;
    mount["destination"] = destination;
    mount["type"] = "bind";

    Json::Value mountOptions(Json::arrayValue);;

    static const std::vector<std::pair<unsigned long, std::string>> mountFlagsNames =
    {
        {   MS_BIND | MS_REC,   "rbind"         },
        {   MS_BIND,            "bind"          },
        {   MS_SILENT,          "silent"        },
        {   MS_RDONLY,          "ro"            },
        {   MS_SYNCHRONOUS,     "sync"          },
        {   MS_NOSUID,          "nosuid"        },
        {   MS_DIRSYNC,         "dirsync"       },
        {   MS_NODIRATIME,      "nodiratime"    },
        {   MS_RELATIME,        "relatime"      },
        {   MS_NOEXEC,          "noexec"        },
        {   MS_NODEV,           "nodev"         },
        {   MS_NOATIME,         "noatime"       },
        {   MS_STRICTATIME,     "strictatime"   },
    };

    // convert the mount flags to their string equivalents
    for (const auto &entry : mountFlagsNames)
    {
        const unsigned long mountFlag = entry.first;
        if ((mountFlag & mountFlags) == mountFlag)
        {
            mountOptions.append(entry.second);

            // clear the mount flags bit such that we can display a warning
            // if the caller supplies a flag we don't support
            mountFlags &= ~mountFlag;
        }
    }

    // if there was a mount flag we didn't support display a warning
    if (mountFlags != 0)
    {
        LOGWARN("unsupported mount flag(s) 0x%04lx", mountFlags);
    }

    mount["options"] = std::move(mountOptions);
    
    return mount;
}

bool DobbySpecGenerator::shouldEnableGpu(const ApplicationConfiguration& config) const
{
    return !config.mWesterosSocketPath.empty();
}

ssize_t DobbySpecGenerator::getSysMemoryLimit(const ApplicationConfiguration& config, const WPEFramework::Exchange::RuntimeConfig& runtimeConfig) const
{
    ssize_t memoryLimit = runtimeConfig.systemMemoryLimit;
    if (memoryLimit <= 0)
    {
        if (runtimeConfig.appType.compare("INTERACTIVE") == 0)
	{
            memoryLimit = mAIConfiguration->getNonHomeAppMemoryLimit();
        }
        //TODO SUPPORT Add other application types
    }
    return memoryLimit;
}

ssize_t DobbySpecGenerator::getGPUMemoryLimit(const ApplicationConfiguration& config, const WPEFramework::Exchange::RuntimeConfig& runtimeConfig) const
{
    ssize_t gpuMemoryLimit = runtimeConfig.gpuMemoryLimit;
    if (gpuMemoryLimit <= 0)
    {
        if (runtimeConfig.appType.compare("INTERACTIVE") == 0)
	{
            gpuMemoryLimit = mAIConfiguration->getNonHomeAppGpuLimit();
        }
        //TODO SUPPORT Add other application types
    }
    return gpuMemoryLimit;
}

bool DobbySpecGenerator::getVpuEnabled(const ApplicationConfiguration& config, const WPEFramework::Exchange::RuntimeConfig& runtimeConfig) const
{
    // TODO SUPPORT RIALTO
    /*
    if (rialToEnabled)
    {
        return false;
    }
    */
    if (runtimeConfig.appType.compare("SYSTEM") == 0)
    {
        return false;
    }
    std::list<std::string> vpuAccessBlackList = mAIConfiguration->getVpuAccessBlacklist();
    bool isBlackListed = false;
    for (auto it = vpuAccessBlackList.begin(); it != vpuAccessBlackList.end(); ++it)
    {
        if ((*it).compare(config.mAppId) == 0)
	{
            isBlackListed = true;
            break;	    
	}
    }
    return !isBlackListed;
}

std::string DobbySpecGenerator::getCpuCores()
{
    const int nCores = std::min(32, get_nprocs());
    std::bitset<32> cpuSetBitmask = mAIConfiguration->getAppsCpuSet();
    cpuSetBitmask &= ((0x1U << nCores) - 1);
    // check the bitmask so that we have at least one core enabled
    if (cpuSetBitmask.none())
    {
        cpuSetBitmask = ((0x1U << nCores) - 1);
    }

    // create the json string for the cores to enable
    std::ostringstream coresStream;
    for (int core = 0; core < nCores; core++)
    {
        if (cpuSetBitmask.test(core))
        {
            coresStream << core << ",";
        }
    }

    // get the string and remove trailing ','
    std::string coresStr = coresStream.str();
    if (!coresStr.empty())
        coresStr.pop_back();

    return coresStr;
}

Json::Value DobbySpecGenerator::createRdkPlugins(const ApplicationConfiguration& config, const WPEFramework::Exchange::RuntimeConfig& runtimeConfig) const
{
    Json::Value rdkPluginsObj(Json::objectValue);
    if (!config.mPorts.empty())
    {
        rdkPluginsObj["appservicesrdk"] = createAppServiceSDKPlugin(config, runtimeConfig);
    }
    rdkPluginsObj["ionmemory"] = createIonMemoryPlugin();



    rdkPluginsObj["networking"] = createNetworkPlugin(config, runtimeConfig);
    if (runtimeConfig.thunder)
    {
        rdkPluginsObj["thunder"] = createThunderPlugin(config);
    }

    //WORK: runtime config need to have credmgr certificate
    /*
    std::optional<IPackage::Certificate> credmgrCert = appPackage->credmgrCertificate();
    if (credmgrCert)
    {
        rdkPluginsObj["credentialsmanager"]["data"] =
            createCredentialsManagerData(credmgrCert->x509Certificate, credmgrCert->rsaKey);
    }
    */
    rdkPluginsObj["minidump"] = createMinidumpPlugin();

    //WORK: httpproxy only for debug builds
    /*
    if (!mHttpProxyHostname.empty())
    {
        rdkPluginsObj["httpproxy"] = createHttpProxyPlugin(false,
                                                           mHttpProxyHostname,
                                                           mHttpProxyPort,
                                                           mHttpProxyCACert);
    }
    */
    //WORK: seccomp
    return rdkPluginsObj;
}

Json::Value DobbySpecGenerator::createMinidumpPlugin() const
{
    Json::Value pluginObj(Json::objectValue);
    static const Json::StaticString minidumpPath("/opt/minidumps");
    static const Json::StaticString minidumpSecurePath("/opt/secure/minidumps");

    pluginObj["required"] = false;
    if (access("/tmp/.SecureDumpDisable", R_OK) == 0)
    {
        pluginObj["data"]["destinationPath"] = minidumpPath;
    }
    else
    {
        pluginObj["data"]["destinationPath"] = minidumpSecurePath;
    }
    return pluginObj;
}

//TODO SUPPORT localservices in appsservice plugin
//TODO SUPPORT airplay2 ports in appsservice plugin
Json::Value DobbySpecGenerator::createAppServiceSDKPlugin(const ApplicationConfiguration& config, const WPEFramework::Exchange::RuntimeConfig& runtimeConfig) const
{
    Json::Value pluginObj(Json::objectValue);

    pluginObj["required"] = false;

    Json::Value dependencies(Json::arrayValue);
    dependencies.append("networking");

    pluginObj["dependsOn"] = std::move(dependencies);

    Json::Value ports(Json::arrayValue);
    if (runtimeConfig.dial)
    {
        ports.append(mAIConfiguration->getDialServerPort());
    }
    for (auto port : config.mPorts)
        ports.append(port);
    for (int port : mAIConfiguration->getMapiPorts())
    {
        ports.append(port);
    }
    pluginObj["data"]["additionalPorts"] = std::move(ports);

    return pluginObj;
}

Json::Value DobbySpecGenerator::createNetworkPlugin(const ApplicationConfiguration& config, const WPEFramework::Exchange::RuntimeConfig& runtimeConfig) const
{
    Json::Value pluginObj(Json::objectValue);

    pluginObj["required"] = true;

    Json::Value dataObj(Json::objectValue);
    if (runtimeConfig.wanLanAccess)
    {
        dataObj["type"] = "nat";
        dataObj["dnsmasq"] = true;
    }
    else
    {
        dataObj["type"] = "none";
        dataObj["dnsmasq"] = false;
    }

    dataObj["ipv4"] = true;
    dataObj["ipv6"] = mAIConfiguration->getIPv6Enabled();

    //TODO SUPPORT Nat holepunch
    /*
    // add NAT holepunch support if requested (only for non-html apps)
    if (appPackage->hasCapability(IPackage::Capability::NatHolePunch) &&
        isAllowedHolePunch(appPackage))
    {
        data["portForwarding"]["hostToContainer"] = createHolePunchArray(appPackage);
    }

    //TODO SUPPORT Multicast forwarding
    // add multicast forwarding - primarily used for netflix MDX
    if (appPackage->hasCapability(IPackage::Capability::MulticastForward))
    {
        data["multicastForwarding"] = createMulticastGroupsArray(appPackage);
    }

    // TODO SUPPORT inter-container communication
    if (appPackage->hasCapability(IPackage::Capability::LocalSocketServer) ||
        appPackage->hasCapability(IPackage::Capability::LocalSocketClient))
    {
        Json::Value interContainerArray = createInterContainerArray(appPackage);
        if (!interContainerArray.empty())
        {
            data[interContainer] = std::move(interContainerArray);
        }
    }
    */
    pluginObj["data"] = std::move(dataObj);

    return pluginObj;
}

void DobbySpecGenerator::populateClassicPlugins(const ApplicationConfiguration& config, const WPEFramework::Exchange::RuntimeConfig& runtimeConfig, Json::Value& spec)
{
    Json::Value pluginsArray(Json::arrayValue);
    // enable the logging plugin
    pluginsArray.append(createEthanLogPlugin(config, runtimeConfig));
    
    //TODO SUPPORT Runtime config need to have requiresDrm parameter
    /*
    if (!usingRialto)
    {
        if ((appPackage->hasCapability(IPackage::Capability::RequiresDrm)) ||
            (appPackage->runtime() == "application/html"))
        {
            pluginsArray.append(createOpenCDMPlugin(config, runtimeConfig));
        }
    }
    */    
    pluginsArray.append(createOpenCDMPlugin(config, runtimeConfig));

    //TODO SUPPORT Runtime config need to have multicastSocket, multicastForward,NatHolePunch capability parameter
    /*
    if (appPackage->hasCapability(IPackage::Capability::MulticastSocket))
    if (false)
    {
        pluginsArray.append(createMulticastSocketPlugin(config, runtimeConfig));
    }
    }

    // add multicast forwarding - primarily used for netflix MDX
    if (appPackage->hasCapability(IPackage::Capability::MulticastForward))
    {
        pluginsArray.append(createMulticastForwarderPlugin(appPackage));
    }

    // add NAT holepunch support if requested (only for non-html apps)
    if (appPackage->hasCapability(IPackage::Capability::NatHolePunch) &&
        isAllowedHolePunch(appPackage)) //(package->runtime() == "application/system")
    {
        pluginsArray.append(createHolePuncherPlugin(appPackage));
    }
    */
    spec["plugins"] = std::move(pluginsArray);
}

Json::Value DobbySpecGenerator::createEthanLogPlugin(const ApplicationConfiguration& config, const WPEFramework::Exchange::RuntimeConfig& runtimeConfig) const
{
    Json::Value plugin(Json::objectValue);
    static const Json::StaticString name("name");
    static const Json::StaticString data("data");
    static const Json::StaticString pluginName("EthanLog");

    static const Json::StaticString loglevels("loglevels");
    static const Json::StaticString fatal("fatal");
    static const Json::StaticString error("error");
    static const Json::StaticString warning("warning");
    static const Json::StaticString milestone("milestone");
    static const Json::StaticString info("info");
    static const Json::StaticString debug("debug");

    Json::Value levels(Json::arrayValue);
    //TODO SUPPORT logging mask in runtime config: RDKEMW-4432
    /*
    unsigned logMask = package->loggingMask();
    if (logMask & unsigned(IPackage::LogLevel::Default))
        logMask |= mDefaultLoggingMask;

    if (logMask & unsigned(IPackage::LogLevel::Fatal))
        levels.append(fatal);
    if (logMask & unsigned(IPackage::LogLevel::Error))
        levels.append(error);
    if (logMask & unsigned(IPackage::LogLevel::Warning))
        levels.append(warning);
    if (logMask & unsigned(IPackage::LogLevel::Info))
        levels.append(info);
    if (logMask & unsigned(IPackage::LogLevel::Debug))
        levels.append(debug);
    if (logMask & unsigned(IPackage::LogLevel::Milestone))
        levels.append(milestone);
    */
    levels.append(fatal);
    levels.append(error);
    levels.append(warning);
    levels.append(info);
    levels.append(debug);
    levels.append(milestone);

    plugin[name] = pluginName;
    plugin[data][loglevels] = levels;

    return plugin;
}


Json::Value DobbySpecGenerator::createMulticastSocketPlugin(const ApplicationConfiguration& config, const WPEFramework::Exchange::RuntimeConfig& runtimeConfig) const
{
    //TODO SUPPORT multicast socket plugin
    /*
    static const Json::StaticString name("name");
    static const Json::StaticString data("data");
    static const Json::StaticString pluginName("MulticastSockets");

    static const Json::StaticString serverSockets("serverSockets");
    static const Json::StaticString clientSockets("clientSockets");

    Json::Value serverSocketsArray = createMulticastServerSocketArray(package);
    Json::Value clientSocketsArray = createMulticastClientSocketArray(package);

    if (serverSocketsArray.empty() && clientSocketsArray.empty())
    {
        return Json::Value::null;
    }

    Json::Value plugin(Json::objectValue);
    plugin[name] = pluginName;
    if (!serverSocketsArray.empty())
        plugin[data][serverSockets] = std::move(serverSocketsArray);
    if (!clientSocketsArray.empty())
        plugin[data][clientSockets] = std::move(clientSocketsArray);

    return plugin;
    */
    return Json::Value::null;
}

Json::Value DobbySpecGenerator::createIonMemoryPlugin() const
{
    Json::Value plugin(Json::objectValue);
    static const Json::StaticString data("data");
    plugin[data] = mIonMemoryPluginData;
    return plugin;
}

Json::Value DobbySpecGenerator::createThunderPlugin(const ApplicationConfiguration& config) const
{
    static const Json::StaticString dependsOn("dependsOn");
    static const Json::StaticString bearerUrl("bearerUrl");
    static const Json::StaticString data("data");
    static const Json::StaticString localServices1("http://local-services-1.sky.com");
    static const Json::StaticString localServices2("http://local-services-2.sky.com");
    static const Json::StaticString localServices3("http://local-services-3.sky.com");
    static const Json::StaticString localServices4("http://local-services-4.sky.com");
    static const Json::StaticString localServices5("http://local-services-5.sky.com");

    Json::Value plugin(Json::objectValue);
    Json::Value dependencies(Json::arrayValue);
    dependencies.append("networking");
    plugin[dependsOn] = std::move(dependencies);

    //TODO SUPPORT Runtime config to check for localservices1,localservices2,localservices3,localservices4
    /*
    if (package->hasCapability(IPackage::Capability::LocalServices1))
        plugin[data][bearerUrl] = localServices1;
    else if (package->hasCapability(IPackage::Capability::LocalServices2))
        plugin[data][bearerUrl] = localServices2;
    else if (package->hasCapability(IPackage::Capability::LocalServices3))
        plugin[data][bearerUrl] = localServices3;
    else if (package->hasCapability(IPackage::Capability::LocalServices4))
        plugin[data][bearerUrl] = localServices4;
    else
        plugin[data][bearerUrl] = localServices5;
    */
    plugin[data][bearerUrl] = localServices2;

    return plugin;
}

Json::Value DobbySpecGenerator::createOpenCDMPlugin(const ApplicationConfiguration& config, const WPEFramework::Exchange::RuntimeConfig& runtimeConfig) const
{
    static const Json::StaticString name("name");
    static const Json::StaticString data("data");
    static const Json::StaticString pluginName("OpenCDM");

    Json::Value plugin(Json::objectValue);
    plugin[name] = pluginName;
    plugin[data] = Json::Value::null;

    return plugin;
}

void DobbySpecGenerator::initialiseIonHeapsJson()
{
    Json::Value heapsArray(Json::arrayValue);
    for (const auto &heapQuota : mAIConfiguration->getIonHeapQuotas())
    {
        Json::Value heapObject(Json::objectValue);
        heapObject["name"] = heapQuota.first;
        heapObject["limit"] = heapQuota.second;

        heapsArray.append(std::move(heapObject));
    }

    mIonMemoryPluginData["defaultLimit"] = mAIConfiguration->getIonHeapDefaultQuota();
    mIonMemoryPluginData["heaps"] = std::move(heapsArray);
}

Json::Value DobbySpecGenerator::createPrivateDataMount(const WPEFramework::Exchange::RuntimeConfig& runtimeConfig) const
{
    static const Json::StaticString source("source");
    static const Json::StaticString destination("destination");
    static const Json::StaticString type("type");
    static const Json::StaticString loop("loop");
    static const Json::StaticString fstype("fstype");
    static const Json::StaticString ext4("ext4");
    static const Json::StaticString options("options");

    static const Json::StaticString nosuid("nosuid");
    static const Json::StaticString nodev("nodev");
    static const Json::StaticString noexec("noexec");

    Json::Value mount(Json::objectValue);
    std::string sourcePath(runtimeConfig.unpackedPath);
    if (sourcePath.empty())
    {
        return Json::nullValue;
    }

    mount[source] = sourcePath;
    mount[destination] = "/home/private";
    mount[type] = loop;
    mount[fstype] = ext4;

    Json::Value optionsArray(Json::arrayValue);
    optionsArray.append(nosuid);
    optionsArray.append(nodev);
    optionsArray.append(noexec);
    mount[options] = optionsArray;

    return mount;
}

void DobbySpecGenerator::createFkpsMounts(const ApplicationConfiguration& config, const WPEFramework::Exchange::RuntimeConfig& runtimeConfig, Json::Value& spec) const
{
    std::list<std::string> fkpsFiles;
    JsonArray fkpsFilesArray;
    fkpsFilesArray.FromString(runtimeConfig.fkpsFiles);
    for (unsigned int i = 0; i < fkpsFilesArray.Length(); ++i)
    {
        fkpsFiles.push_back(fkpsFilesArray[i].String());
    }
    if (fkpsFiles.empty())
        return;

    // iterate through the files and make sure they're accessible
    const std::string fkpsPathPrefix("/opt/drm/");
    for (std::list<std::string>::iterator it=fkpsFiles.begin(); it!=fkpsFiles.end(); ++it)
    {
	std::string fkpsFile = *it;      
        const std::string fkpsFilePath = fkpsPathPrefix + fkpsFile;

        // check if the file exists
        struct stat details;
        if (stat(fkpsFilePath.c_str(), &details) != 0)
        {
            if (errno == ENOENT)
            {
                printf("missing FKPS file '%s', won't map into container",
                            fkpsFilePath.c_str());
            }
            else
            {
                printf("failed to set FKPS file '%s' %d",
                                 fkpsFilePath.c_str(), errno);
            }

            continue;
        }

        // check the group owner matches the app
        if ((details.st_gid != config.mGroupId) &&
            (chown(fkpsFilePath.c_str(), -1, config.mGroupId) != 0))
        {
            printf("failed to change group owner of '%s' %d",
                             fkpsFilePath.c_str(), errno);
        }

        // and that the group perms are set to r--
        if (((details.st_mode & S_IRGRP) != S_IRGRP) &&
            (chmod(fkpsFilePath.c_str(), ((details.st_mode | S_IRGRP) & ALLPERMS)) != 0))
        {
            printf("failed to set file permissions to 0%03o for '%s' %d",
                             ((details.st_mode & ~S_IRWXG) | S_IRGRP), fkpsFilePath.c_str(), errno);
        }

        // finally add a bind mount for them
        spec.append(createBindMount(fkpsFilePath, fkpsFilePath,
                                          (MS_BIND | MS_RDONLY | MS_NOSUID | MS_NODEV | MS_NOEXEC)));
    }

    // additional tmpfs mount apparently required for YT certification
    spec.append(createTmpfsMount("/opt/drm/vault",
                                       (MS_NOSUID | MS_NODEV | MS_NOEXEC)));

}

Json::Value DobbySpecGenerator::createTmpfsMount(const std::string &mntDestination,
                                                 unsigned long mntOptions) const
{
    static const Json::StaticString source("source");
    static const Json::StaticString destination("destination");
    static const Json::StaticString type("type");
    static const Json::StaticString tmpfs("tmpfs");
    static const Json::StaticString options("options");

    static const Json::StaticString nosuid("nosuid");
    static const Json::StaticString nodev("nodev");
    static const Json::StaticString noexec("noexec");
    static const Json::StaticString noatime("noatime");
    static const Json::StaticString relatime("relatime");
    static const Json::StaticString strictatime("strictatime");
    static const Json::StaticString sync("sync");
    static const Json::StaticString nodiratime("nodiratime");
    static const Json::StaticString size("size=65536k");
    static const Json::StaticString indoes("nr_inodes=8k");
    Json::Value mount(Json::objectValue);

    mount[type] = tmpfs;
    mount[source] = tmpfs;
    mount[destination] = mntDestination;

    Json::Value optionsArray(Json::arrayValue);

    if (mntOptions & MS_NOSUID)
        optionsArray.append(nosuid);
    if (mntOptions & MS_NODEV)
        optionsArray.append(nodev);
    if (mntOptions & MS_NOEXEC)
        optionsArray.append(noexec);

    if (mntOptions & MS_SYNCHRONOUS)
        optionsArray.append(sync);

    if (mntOptions & MS_NODIRATIME)
        optionsArray.append(nodiratime);

    if (mntOptions & MS_NOATIME)
        optionsArray.append(noatime);
    if (mntOptions & MS_RELATIME)
        optionsArray.append(relatime);
    if (mntOptions & MS_STRICTATIME)
        optionsArray.append(strictatime);

    optionsArray.append(size);
    optionsArray.append(indoes);

    mount[options] = std::move(optionsArray);

    return mount;
}

Json::Value DobbySpecGenerator::createResourceManagerMount(const ApplicationConfiguration& config) const
{
    constexpr unsigned long mntOptions = (MS_BIND | MS_NOSUID | MS_NODEV | MS_NOEXEC);
    static const std::string resmgrMountSource = XDG_RUNTIME_DIR "/resource";
    static const std::string resmgrMountPoint = XDG_RUNTIME_DIR "/resource";

    struct stat details;
    Json::Value resmgrMount;
    if (stat(resmgrMountSource.c_str(), &details) == 0)
    {
        resmgrMount = createBindMount(resmgrMountSource, resmgrMountPoint, mntOptions);

        // check the group owner matches the app
        if ((details.st_gid != config.mGroupId) &&
            (chown(resmgrMountSource.c_str(), -1, config.mGroupId) != 0))
        {
            printf("failed to change group owner of '%s'", resmgrMountSource.c_str());
        }

        // and that the group perms are set to 0770
        if (chmod(resmgrMountSource.c_str(), 0770) != 0)
        {
            printf("failed to set file permissions to 0770 for '%s'", resmgrMountSource.c_str());
        }
    }

    return resmgrMount;
}

std::string DobbySpecGenerator::encodeURL(std::string url) const
{
    std::string encodedUrl;

    CURL *curl = curl_easy_init();
    if (curl)
    {
      char *escapedUrl = curl_easy_escape(curl, url.c_str(), (int)url.length());
      if (escapedUrl)
      {
          encodedUrl = std::string(escapedUrl);
          curl_free(escapedUrl);
      }
      else
      {
          printf("curl_easy_escape() failed");
          fflush(stdout);
      }

      curl_easy_cleanup(curl);
    }
    else
    {
        printf("curl_easy_init() failed");
        fflush(stdout);
    }

    return encodedUrl;
}

void DobbySpecGenerator::addHolePunchPortToSpec(Json::Value &spec, in_port_t port) const
{
printf("Adding hole punching port %hu for app", port);
    fflush(stdout);

    static Json::Value nullValue(Json::nullValue);

    Json::Value &specRef = spec;

    // create a new entry
    Json::Value entry(Json::objectValue);
    entry["port"] = port;
    entry["protocol"] = "tcp";

    // check which type of spec we have
    if (specRef.isMember("rdkPlugins"))
    {
        // dobby++ spec format
        Json::Value &rdkPlugins = specRef["rdkPlugins"];

        const Json::Value &hostToContainer =
            Json::Path(".networking.data.portForwarding.hostToContainer")
                .resolve(rdkPlugins);
        if (!hostToContainer.isArray())
        {
            // no hole punches defined so add Networking rdkPlugin's
            // hostToContainer field
            rdkPlugins["networking"]["data"]["portForwarding"]["hostToContainer"] = Json::arrayValue;
        }

        // append to the array hole punches array
        rdkPlugins["networking"]["data"]["portForwarding"]["hostToContainer"].append(std::move(entry));
    }
    else
    {
        // classic dobby format

        // add / modify the hole punch code for gdbserver
        Json::Value &plugins = specRef["plugins"];
        for (Json::Value &plugin : plugins)
        {
            if (!plugin.isObject())
                break;

            const Json::Value &name = plugin["name"];
            if (!name.isString() || (strcmp(name.asCString(), "HolePuncher") != 0))
                continue;

            Json::Value &holes = plugin["data"]["holes"];
            if (!holes.isArray())
                LOGINFO("odd, invalid HolePuncher plugin json object");
            else
                holes.append(std::move(entry));

            return;
        }

        // no hole punch plugin defined so add one now
        Json::Value holes(Json::arrayValue);
        holes.append(std::move(entry));

        Json::Value plugin(Json::objectValue);
        plugin["name"] = "HolePuncher";
        plugin["data"]["holes"] = std::move(holes);

        plugins.append(std::move(plugin));
    }
}
} // namespace Plugin
} // namespace WPEFramework
