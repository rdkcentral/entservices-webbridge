#ifndef STATE_TRANSITION_REQUEST_H
#define STATE_TRANSITION_REQUEST_H
#include "ApplicationContext.h"
#include <thread>
#include <semaphore>
#include <vector>

namespace WPEFramework
{
    namespace Plugin
    {
        struct StateTransitionRequest
        {
            StateTransitionRequest(ApplicationContext* context, Exchange::ILifecycleManager::LifecycleState state): mContext(context), mTargetState(state), mStatePath()
            {
            }
            ApplicationContext* mContext;
            Exchange::ILifecycleManager::LifecycleState mTargetState;
            std::vector<Exchange::ILifecycleManager::LifecycleState> mStatePath;
        };
    }
}
#endif
