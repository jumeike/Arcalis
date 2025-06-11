#ifndef MINIMAL_SOCIAL_USER_HANDLER_H
#define MINIMAL_SOCIAL_USER_HANDLER_H

#include <iostream>
#include <map>
#include "gen-cpp/UserService.h"

namespace minimal_social {

class UserHandler : public UserServiceIf {
public:
    UserHandler() {
        // Pre-populate some users for testing
        User user1;
        user1.user_id = 1;
        user1.username = "user1";
        
        User user2;
        user2.user_id = 2;
        user2.username = "user2";
        
        _users["user1"] = user1;
        _users["user2"] = user2;
    }

    void getUser(User& _return, const int64_t req_id, const std::string& username) override {
        auto it = _users.find(username);
        if (it == _users.end()) {
            ServiceException se;
            se.errorCode = ErrorCode::SE_UNAUTHORIZED;
            se.message = "User not found";
            throw se;
        }
        _return = it->second;
    }

private:
    std::map<std::string, User> _users;
};

} // namespace minimal_social

#endif
