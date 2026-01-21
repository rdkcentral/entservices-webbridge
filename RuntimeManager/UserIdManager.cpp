#include "UserIdManager.h"

namespace WPEFramework {
namespace Plugin {

UserIdManager::UserIdManager()
    :mValidUidRange(getAppsUidRange())
{
    unsigned int count = 0u;
    for (uid_t uid = mValidUidRange.first; uid <= mValidUidRange.second; uid++)
    {
        if (count++ > 1000u)
            break;

        mUserIdAvailablePool.push_back(uid);
    }

    if (mUserIdAvailablePool.empty())
    {
        printf("Depleted user id pool, none available for new apps");
    }

}

uid_t UserIdManager::getUserId(const std::string& appId)
{
    std::lock_guard<std::mutex> lock(mLock);

    auto found = mUserIdMap.find(appId);
    if (found != mUserIdMap.end())
    {
        return found->second;
    }

    if (mUserIdAvailablePool.size() < 5u)
    {
        printf("Pool of available uids has been depleted, refusing to give"
                     " app '%s' a uid", appId.c_str());
        return 0;
    }

    uid_t uid = mUserIdAvailablePool.front();
    mUserIdAvailablePool.pop_front();

    mUserIdMap[appId] = uid;
    return uid;
}

void UserIdManager::clearUserId(const std::string & appId)
{
    std::lock_guard<std::mutex> lock(mLock);

    std::map<std::string, uid_t>::iterator found = mUserIdMap.find(appId);
    if (found != mUserIdMap.end())
    {
        mUserIdAvailablePool.push_back(found->second);
        mUserIdMap.erase(found);
    }
}

std::pair<uid_t, uid_t> UserIdManager::getAppsUidRange()
{
    return std::make_pair(30001, 31000);
}

gid_t UserIdManager::getAppsGid()
{
    return 30000;
}
} // namespace Plugin
} // namespace WPEFramework
