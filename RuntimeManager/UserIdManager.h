#ifndef USERIDMANAGER_H
#define USERIDMANAGER_H

#include <map>
#include <list>
#include <set>
#include <mutex>
#include <string>
#include <memory>

namespace WPEFramework {
namespace Plugin {

    class UserIdManager
    {
        public:
            UserIdManager();
            uid_t getUserId(const std::string& appId);
            void clearUserId(const std::string & appId);
            gid_t getAppsGid();
        
        private:
            std::pair<uid_t, uid_t> getAppsUidRange();
            mutable std::mutex mLock;
            std::map<std::string, uid_t> mUserIdMap;
            std::list<uid_t> mUserIdAvailablePool;
            const std::pair<uid_t, uid_t> mValidUidRange;
    };

} // namespace Plugin
} // namespace WPEFramework

#endif // !defined(USERIDMANAGER_H)
