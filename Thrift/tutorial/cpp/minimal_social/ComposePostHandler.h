#ifndef MINIMAL_SOCIAL_COMPOSE_POST_HANDLER_H
#define MINIMAL_SOCIAL_COMPOSE_POST_HANDLER_H

#include <chrono>
#include <memory>
#include <string>
#include <map>
#include "gen-cpp/ComposePostService.h"
#include "gen-cpp/UserService.h"
#include "gen-cpp/UniqueIdService.h"
#include "ClientPool.h"

namespace minimal_social {

class ComposePostHandler : public ComposePostServiceIf {
public:
    ComposePostHandler(
        std::shared_ptr<ClientPool<UserServiceClient>> user_client_pool,
        std::shared_ptr<ClientPool<UniqueIdServiceClient>> unique_id_client_pool
    ) : _user_client_pool(user_client_pool),
        _unique_id_client_pool(unique_id_client_pool) {}

    void ComposePost(
        const int64_t req_id,
        const std::string& username,
        const std::string& text
    ) override {
        // Get user info
        User user;
        auto user_client = _user_client_pool->getClient();
        try {
            user_client->getUser(user, req_id, username);
        } catch (ServiceException& se) {
            throw se;
        }

        // Get unique post ID
        int64_t post_id;
        auto unique_id_client = _unique_id_client_pool->getClient();
        try {
            post_id = unique_id_client->generateId(req_id);
        } catch (ServiceException& se) {
            throw se;
        }

        // Create post
        Post post;
        post.post_id = post_id;
        post.user_id = user.user_id;
        post.username = username;
        post.text = text;
        post.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();

        // Store post (in memory for this example)
        _posts[post_id] = post;
    }

private:
    std::shared_ptr<ClientPool<UserServiceClient>> _user_client_pool;
    std::shared_ptr<ClientPool<UniqueIdServiceClient>> _unique_id_client_pool;
    std::map<int64_t, Post> _posts;
};

} // namespace minimal_social

#endif
