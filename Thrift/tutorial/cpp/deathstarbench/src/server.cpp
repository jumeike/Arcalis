#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/server/TThreadedServer.h>
#include <thrift/server/TSimpleServer.h>
#include <thrift/transport/TServerSocket.h>
#include <thrift/transport/TServerUDPSocket.h>
#include <thrift/transport/TBufferTransports.h>
#include <thrift/concurrency/Thread.h>
#include "../gen-cpp/ComposePostService.h"
#include <memory>
#include <map>
#include <set>
#include <vector>
#include <chrono>
#include <iostream>
#include <mutex>
#include <algorithm>
#include <unordered_map>

using namespace ::apache::thrift;
using namespace ::apache::thrift::protocol;
using namespace ::apache::thrift::transport;
using namespace ::apache::thrift::server;
using namespace ::apache::thrift::concurrency;
using namespace ::composepost;

class ComposePostHandler : virtual public ComposePostServiceIf {
private:
    std::mutex mutex_;
    std::map<int64_t, Post> posts_;
    std::map<std::string, std::vector<int64_t>> userPosts_;
    std::map<std::string, std::vector<int64_t>> hashtagPosts_;
    std::map<std::string, std::set<int64_t>> userSavedPosts_;
    std::unordered_map<std::string, int32_t> hashtagCounts_;

    bool isAuthorized(int64_t post_id, const std::string& username) {
        auto it = posts_.find(post_id);
        return it != posts_.end() && it->second.creator == username;
    }

public:
    ComposePostHandler() = default;

    void CreatePost(Post& _return, const std::string& creator, const std::string& text,
                   const std::vector<MediaMetadata>& media, const PostVisibility::type visibility,
                   const PostType::type post_type, const std::string& location,
                   const std::map<std::string, std::string>& metadata) override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        try {
            auto now = std::chrono::system_clock::now();
            auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()).count();
            
            _return.post_id = timestamp;
            _return.creator = creator;
            _return.text = text;
            _return.media = media;
            _return.timestamp = timestamp / 1000;
            _return.visibility = visibility;
            _return.post_type = post_type;
            _return.location = location;
            _return.metadata = metadata;
            
            // Initialize stats
            PostStats stats;
            stats.num_likes = 0;
            stats.num_comments = 0;
            stats.num_shares = 0;
            stats.num_saves = 0;
            _return.stats = stats;
            
            // Extract hashtags and mentions
            std::vector<Hashtag> hashtags;
            std::vector<UserMention> mentions;
            // Simple parsing (in practice, you'd want more sophisticated parsing)
            size_t pos = 0;
            while ((pos = text.find('#', pos)) != std::string::npos) {
                size_t end = text.find(' ', pos);
                if (end == std::string::npos) end = text.length();
                Hashtag tag;
                tag.tag = text.substr(pos + 1, end - pos - 1);
                tag.start_index = pos;
                tag.end_index = end;
                hashtags.push_back(tag);
                hashtagPosts_[tag.tag].push_back(_return.post_id);
                hashtagCounts_[tag.tag]++;
                pos = end;
            }
            _return.hashtags = hashtags;
            
            posts_[_return.post_id] = _return;
            userPosts_[creator].push_back(_return.post_id);
            
        } catch (const std::exception& e) {
            ServiceException se;
            se.error_code = 4;
            se.error_message = e.what();
            throw se;
        }
    }

    void GetPost(Post& _return, const int64_t post_id) override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = posts_.find(post_id);
        if (it == posts_.end()) {
            ServiceException se;
            se.error_code = 3;
            se.error_message = "Post not found";
            throw se;
        }
        
        _return = it->second;
    }

    void UpdatePost(Post& _return, const int64_t post_id, const std::string& creator,
                   const std::string& text, const std::vector<MediaMetadata>& media,
                   const PostVisibility::type visibility) override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (!isAuthorized(post_id, creator)) {
            ServiceException se;
            se.error_code = 2;
            //printf("UpdatePost\n");
            se.error_message = "Unauthorized";
            throw se;
        }
        
        auto it = posts_.find(post_id);
        if (it == posts_.end()) {
            ServiceException se;
            se.error_code = 3;
            se.error_message = "Post not found";
            throw se;
        }
        
        it->second.text = text;
        it->second.media = media;
        it->second.visibility = visibility;
        _return = it->second;
    }

    void DeletePost(const int64_t post_id, const std::string& creator) override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (!isAuthorized(post_id, creator)) {
            ServiceException se;
            se.error_code = 2;
            //printf("DeletePost\n");
            se.error_message = "Unauthorized";
            throw se;
        }
        
        auto post_it = posts_.find(post_id);
        if (post_it != posts_.end()) {
            // Remove from all indices
            for (const auto& hashtag : post_it->second.hashtags) {
                hashtagPosts_[hashtag.tag].erase(
                    std::remove(hashtagPosts_[hashtag.tag].begin(),
                              hashtagPosts_[hashtag.tag].end(),
                              post_id),
                    hashtagPosts_[hashtag.tag].end());
                hashtagCounts_[hashtag.tag]--;
            }
            
            auto& userPostList = userPosts_[creator];
            userPostList.erase(
                std::remove(userPostList.begin(), userPostList.end(), post_id),
                userPostList.end());
                
            posts_.erase(post_id);
        }
    }

    void GetUserTimeline(std::vector<Post>& _return, const std::string& username,
                        const int32_t start, const int32_t stop,
                        const PostFilter& filter) override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        try {
            auto it = userPosts_.find(username);
            if (it == userPosts_.end()) {
                return;
            }
            
            const auto& userPostIds = it->second;
            int32_t end = std::min(stop, static_cast<int32_t>(userPostIds.size()));
            
            for (int32_t i = start; i < end; ++i) {
                const auto& post = posts_[userPostIds[i]];
                
                // Apply filters
                bool include = true;
                if (!filter.post_types.empty() &&
                    std::find(filter.post_types.begin(),
                            filter.post_types.end(),
                            post.post_type) == filter.post_types.end()) {
                    include = false;
                }
                
                if (filter.start_time && post.timestamp < filter.start_time) {
                    include = false;
                }
                
                if (filter.end_time && post.timestamp > filter.end_time) {
                    include = false;
                }
                
                if (include) {
                    _return.push_back(post);
                }
            }
        } catch (const std::exception& e) {
            ServiceException se;
            se.error_code = 4;
            se.error_message = e.what();
            throw se;
        }
    }

    void GetHashtagTimeline(std::vector<Post>& _return, const std::string& hashtag,
                           const int32_t start, const int32_t stop) override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = hashtagPosts_.find(hashtag);
        if (it == hashtagPosts_.end()) {
            return;
        }
        
        const auto& postIds = it->second;
        int32_t end = std::min(stop, static_cast<int32_t>(postIds.size()));
        
        for (int32_t i = start; i < end; ++i) {
            _return.push_back(posts_[postIds[i]]);
        }
    }

    void GetTrendingHashtags(std::vector<Hashtag>& _return, const int32_t limit) override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<std::pair<std::string, int32_t>> hashtags;
        for (const auto& pair : hashtagCounts_) {
            hashtags.push_back(pair);
        }
        
        std::sort(hashtags.begin(), hashtags.end(),
                 [](const auto& a, const auto& b) {
                     return a.second > b.second;
                 });
        
        int32_t count = std::min(limit, static_cast<int32_t>(hashtags.size()));
        for (int32_t i = 0; i < count; ++i) {
            Hashtag tag;
            tag.tag = hashtags[i].first;
            _return.push_back(tag);
        }
    }

    void GetPostStats(PostStats& _return, const int64_t post_id) override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto post_it = posts_.find(post_id);
        if (post_it == posts_.end()) {
            ServiceException se;
            se.error_code = 3;
            se.error_message = "Post not found";
            throw se;
        }
        
        _return = post_it->second.stats;
    }

    void GetBulkPostStats(std::map<int64_t, PostStats>& _return,
                         const std::vector<int64_t>& post_ids) override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        for (const auto& post_id : post_ids) {
            auto post_it = posts_.find(post_id);
            if (post_it != posts_.end()) {
                _return[post_id] = post_it->second.stats;
            }
        }
    }

    void GetUserEngagementStats(std::map<std::string, int32_t>& _return,
                              const std::string& username) override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        _return["posts"] = userPosts_[username].size();
        _return["saves"] = userSavedPosts_[username].size();
        
        int32_t total_likes = 0;
        int32_t total_shares = 0;
        for (const auto& post_id : userPosts_[username]) {
            const auto& post = posts_[post_id];
            total_likes += post.stats.num_likes;
            total_shares += post.stats.num_shares;
        }
        
        _return["received_likes"] = total_likes;
        _return["received_shares"] = total_shares;
    }

    void SearchPosts(std::vector<Post>& _return, const std::string& query,
                    const PostFilter& filter, const int32_t limit) override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<Post> matching_posts;
        for (const auto& pair : posts_) {
            const auto& post = pair.second;
            
            if (post.text.find(query) != std::string::npos) {
                bool include = true;
                
                if (!filter.post_types.empty() &&
                    std::find(filter.post_types.begin(),
                            filter.post_types.end(),
                            post.post_type) == filter.post_types.end()) {
                    include = false;
                }
                
                if (filter.start_time && post.timestamp < filter.start_time) {
                    include = false;
                }
                
                if (filter.end_time && post.timestamp > filter.end_time) {
                    include = false;
                }
                
                if (!filter.hashtags.empty()) {
                    bool has_hashtag = false;
                    for (const auto& post_tag : post.hashtags) {
                        if (std::find(filter.hashtags.begin(),
                                    filter.hashtags.end(),
                                    post_tag.tag) != filter.hashtags.end()) {
                            has_hashtag = true;
                            break;
                        }
                    }
                    if (!has_hashtag) include = false;
                }
                
                if (include) {
                    matching_posts.push_back(post);
                }
            }
        }
        
        std::sort(matching_posts.begin(), matching_posts.end(),
                 [](const Post& a, const Post& b) {
                     return a.timestamp > b.timestamp;
                 });
        
        int32_t count = std::min(limit, static_cast<int32_t>(matching_posts.size()));
        _return.assign(matching_posts.begin(), matching_posts.begin() + count);
    }

    void LikePost(const int64_t post_id, const std::string& username) override {
        std::lock_guard<std::mutex> lock(mutex_);

        auto post_it = posts_.find(post_id);
        if (post_it == posts_.end()) {
            ServiceException se;
            se.error_code = 3;
            se.error_message = "Post not found";
            throw se;
        }

        auto& liked_by = post_it->second.stats.liked_by;
        if (std::find(liked_by.begin(), liked_by.end(), username) == liked_by.end()) {
            liked_by.push_back(username);
            post_it->second.stats.num_likes++;
        }
    }

    void UnlikePost(const int64_t post_id, const std::string& username) override {
        std::lock_guard<std::mutex> lock(mutex_);

        auto post_it = posts_.find(post_id);
        if (post_it == posts_.end()) {
            ServiceException se;
            se.error_code = 3;
            se.error_message = "Post not found";
            throw se;
        }

        auto& liked_by = post_it->second.stats.liked_by;
        auto it = std::find(liked_by.begin(), liked_by.end(), username);
        if (it != liked_by.end()) {
            liked_by.erase(it);
            post_it->second.stats.num_likes--;
        }
    }

    void SharePost(const int64_t post_id, const std::string& username) override {
        std::lock_guard<std::mutex> lock(mutex_);

        auto post_it = posts_.find(post_id);
        if (post_it == posts_.end()) {
            ServiceException se;
            se.error_code = 3;
            se.error_message = "Post not found";
            throw se;
        }

        auto& shared_by = post_it->second.stats.shared_by;
        if (std::find(shared_by.begin(), shared_by.end(), username) == shared_by.end()) {
            shared_by.push_back(username);
            post_it->second.stats.num_shares++;
        }
    }

    void SavePost(const int64_t post_id, const std::string& username) override {
        std::lock_guard<std::mutex> lock(mutex_);

        auto post_it = posts_.find(post_id);
        if (post_it == posts_.end()) {
            ServiceException se;
            se.error_code = 3;
            se.error_message = "Post not found";
            throw se;
        }

        if (userSavedPosts_[username].insert(post_id).second) {
            post_it->second.stats.num_saves++;
        }
    }

    void UnsavePost(const int64_t post_id, const std::string& username) override {
        std::lock_guard<std::mutex> lock(mutex_);

        auto post_it = posts_.find(post_id);
        if (post_it == posts_.end()) {
            ServiceException se;
            se.error_code = 3;
            se.error_message = "Post not found";
            throw se;
        }

        if (userSavedPosts_[username].erase(post_id) > 0) {
            post_it->second.stats.num_saves--;
        }
    }
};

int main(int argc, char** argv) {
    try {
        const int port = 9090;
        auto handler = std::make_shared<ComposePostHandler>();
        auto processor = std::make_shared<ComposePostServiceProcessor>(handler);
        //auto serverTransport = std::make_shared<TServerSocket>("192.168.1.1", port);
        auto serverTransport = std::make_shared<TServerUDPSocket>(port);
        auto transportFactory = std::make_shared<TBufferedTransportFactory>();
        auto protocolFactory = std::make_shared<TBinaryProtocolFactory>();

        //TThreadedServer server(processor, serverTransport, transportFactory, protocolFactory);
        TSimpleServer server(processor, serverTransport, transportFactory, protocolFactory);

        std::cout << "Starting ComposePost server on port " << port << "..." << std::endl;
        server.serve();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
