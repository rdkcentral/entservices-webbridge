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
#include <interfaces/IRDKWindowManager.h>
#include "tracing/Logging.h"
#include <vector>
#include <thread>
#include <fstream>
#include <com/com.h>
#include <core/core.h>
#include <rdkwindowmanager/include/rdkwindowmanagerevents.h>
#include <rdkwindowmanager/include/rdkwindowmanager.h>
#include <rdkwindowmanager/include/linuxkeys.h>

namespace WPEFramework {
namespace Plugin {

    struct CreateDisplayRequest
    {
        CreateDisplayRequest(std::string client, std::string displayName, uint32_t displayWidth=0, uint32_t displayHeight=0, bool virtualDisplayEnabled=false,
                             uint32_t virtualWidth=0, uint32_t virtualHeight=0, bool topmost = false, bool focus = false);

        ~CreateDisplayRequest();

        std::string mClient;
        std::string mDisplayName;
        uint32_t mDisplayWidth;
        uint32_t mDisplayHeight;
        bool mVirtualDisplayEnabled;
        uint32_t mVirtualWidth;
        uint32_t mVirtualHeight;
        bool mTopmost;
        bool mFocus;
        sem_t mSemaphore;
        bool mResult;
        bool mAutoDestroy;
    };

    class RDKWindowManagerImplementation : public Exchange::IRDKWindowManager{
    public:
        RDKWindowManagerImplementation();
        ~RDKWindowManagerImplementation() override;

        RDKWindowManagerImplementation(const RDKWindowManagerImplementation&) = delete;
        RDKWindowManagerImplementation& operator=(const RDKWindowManagerImplementation&) = delete;

        BEGIN_INTERFACE_MAP(RDKWindowManagerImplementation)
        INTERFACE_ENTRY(Exchange::IRDKWindowManager)
        END_INTERFACE_MAP

    public:
        enum Event {
                RDK_WINDOW_MANAGER_EVENT_UNKNOWN,
                RDK_WINDOW_MANAGER_EVENT_ON_USER_INACTIVITY
            };

        class EXTERNAL Job : public Core::IDispatch {
        protected:
             Job(RDKWindowManagerImplementation *rdkWindowManagerImplementation, Event event, JsonValue &params)
                : _rdkWindowManagerImplementation(rdkWindowManagerImplementation)
                , _event(event)
                , _params(params) {
                if (_rdkWindowManagerImplementation != nullptr) {
                    _rdkWindowManagerImplementation->AddRef();
                }
            }

       public:
            Job() = delete;
            Job(const Job&) = delete;
            Job& operator=(const Job&) = delete;
            ~Job() {
                if (_rdkWindowManagerImplementation != nullptr) {
                    _rdkWindowManagerImplementation->Release();
                }
            }

       public:
            static Core::ProxyType<Core::IDispatch> Create(RDKWindowManagerImplementation *rdkWindowManagerImplementation, Event event, JsonValue params) {
#ifndef USE_THUNDER_R4
                return (Core::proxy_cast<Core::IDispatch>(Core::ProxyType<Job>::Create(rdkWindowManagerImplementation, event, params)));
#else
                return (Core::ProxyType<Core::IDispatch>(Core::ProxyType<Job>::Create(rdkWindowManagerImplementation, event, params)));
#endif
            }

            virtual void Dispatch() {
                _rdkWindowManagerImplementation->Dispatch(_event, _params);
            }
        private:
            RDKWindowManagerImplementation *_rdkWindowManagerImplementation;
            const Event _event;
            const JsonValue _params;
        };

    public:
        Core::hresult Initialize(PluginHost::IShell* service) override;
        void Deinitialize(PluginHost::IShell* service) override;
        Core::hresult Register(INotification *notification) override;
        Core::hresult Unregister(INotification *notification) override;
        Core::hresult CreateDisplay(const string &displayParams) override;
        Core::hresult GetClients(string &clients) const override;
        Core::hresult AddKeyIntercept(const string &intercept) override;
        Core::hresult AddKeyIntercepts(const string &intercepts) override;
        Core::hresult RemoveKeyIntercept(const string &intercept) override;
        Core::hresult AddKeyListener(const string &keyListeners) override;
        Core::hresult RemoveKeyListener(const string &keyListeners) override;
        Core::hresult InjectKey(uint32_t keyCode, const string &modifiers) override;
        Core::hresult GenerateKey(const string& keys, const string& client) override;
        Core::hresult EnableInactivityReporting(const bool enable) override;;
        Core::hresult SetInactivityInterval(const uint32_t interval) override;
        Core::hresult ResetInactivityTime() override;
        Core::hresult EnableKeyRepeats(bool enable) override;
        Core::hresult GetKeyRepeatsEnabled(bool &keyRepeat) const override;
        Core::hresult IgnoreKeyInputs(bool ignore) override;
        Core::hresult EnableInputEvents(const string &clients, bool enable) override;
        Core::hresult KeyRepeatConfig(const string &input, const string &keyConfig) override;

    private: /*internal methods*/
        bool createDisplay(const string& client, const string& displayName, const uint32_t displayWidth = 0, const uint32_t displayHeight = 0,
                           const bool virtualDisplay = false, const uint32_t virtualWidth = 0, const uint32_t virtualHeight = 0,
                           const bool topmost = false, const bool focus = false);
        bool getClients(JsonArray& clients);
        bool addKeyIntercept(const uint32_t& keyCode, const JsonArray& modifiers, const string& client);
        bool addKeyIntercepts(const JsonArray& intercepts);
        bool removeKeyIntercept(const uint32_t& keyCode, const JsonArray& modifiers, const string& client);
        bool addKeyListeners(const string& client, const JsonArray& listeners);
        bool removeKeyListeners(const string& client, const JsonArray& listeners);
        bool injectKey(const uint32_t& keyCode, const JsonArray& modifiers);
        bool generateKey(const string& client, const JsonArray& keyInputs);
        bool enableInactivityReporting(const bool enable);
        bool setInactivityInterval(const uint32_t interval);
        bool resetInactivityTime();

        void dispatchEvent(Event event, const JsonValue &params);
        void Dispatch(Event event, const JsonValue params);

    private: /*internal methods*/
        class RdkWindowManagerListener : public RdkWindowManager::RdkWindowManagerEventListener {
          public:
            RdkWindowManagerListener(RDKWindowManagerImplementation* rdkWindowManagerImpl)
                : mRDKWindowManagerImpl(rdkWindowManagerImpl)
            {
            }

            ~RdkWindowManagerListener()
            {
            }

            /* Events listeners */
            virtual void onUserInactive(const double minutes);

          private:
              RDKWindowManagerImplementation *mRDKWindowManagerImpl;
        };

    public/*members*/:
        static RDKWindowManagerImplementation* _instance;

    private:
        mutable Core::CriticalSection mAdminLock;
        std::list<Exchange::IRDKWindowManager::INotification*> mRDKWindowManagerNotification;
        PluginHost::IShell* mService{};
        std::shared_ptr<RdkWindowManager::RdkWindowManagerEventListener> mEventListener;

        friend class Job;
    };
} /* namespace Plugin */
} /* namespace WPEFramework */
