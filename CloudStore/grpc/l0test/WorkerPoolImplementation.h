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

#include "../../Module.h"

class WorkerPoolImplementation
    : public WPEFramework::Core::WorkerPool,
      public WPEFramework::Core::ThreadPool::ICallback {
private:
    class Dispatcher : public WPEFramework::Core::ThreadPool::IDispatcher {
    public:
        Dispatcher(const Dispatcher&) = delete;
        Dispatcher& operator=(const Dispatcher&) = delete;
        Dispatcher() = default;
        ~Dispatcher() override = default;

    private:
        void Initialize() override
        {
        }
        void Deinitialize() override
        {
        }
        void Dispatch(WPEFramework::Core::IDispatch* job) override
        {
            job->Dispatch();
        }
    };

public:
    WorkerPoolImplementation() = delete;
    WorkerPoolImplementation(const WorkerPoolImplementation&) = delete;
    WorkerPoolImplementation& operator=(const WorkerPoolImplementation&) = delete;
    WorkerPoolImplementation(const uint32_t stackSize)
        : WPEFramework::Core::WorkerPool(4 /*threadCount*/, stackSize, 32 /*queueSize*/, &_dispatch, this)
        , _dispatch()
    {
        Run();
    }
    ~WorkerPoolImplementation() override
    {
        Stop();
    }
    void Idle() override
    {
    }

private:
    Dispatcher _dispatch;
};
