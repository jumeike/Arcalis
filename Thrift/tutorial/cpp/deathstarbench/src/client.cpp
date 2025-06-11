#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/transport/TSocket.h>
#include <thrift/transport/TTransportUtils.h>
#include "../gen-cpp/ComposePostService.h"
#include <iostream>
#include <memory>
#include <chrono>
#include <thread>
#include <random>

using namespace apache::thrift;
using namespace apache::thrift::protocol;
using namespace apache::thrift::transport;
using namespace composepost;

void printPost(const Post& post) {
    std::cout << "Post ID: " << post.post_id << std::endl
              << "Creator: " << post.creator << std::endl
              << "Text: " << post.text << std::endl
              << "Type: " << static_cast<int>(post.post_type) << std::endl
              << "Visibility: " << static_cast<int>(post.visibility) << std::endl
              << "Stats:" << std::endl
              << "  Likes: " << post.stats.num_likes << std::endl
              << "  Comments: " << post.stats.num_comments << std::endl
              << "  Shares: " << post.stats.num_shares << std::endl
              << "  Saves: " << post.stats.num_saves << std::endl;
              
    std::cout << "Media:" << std::endl;
    for (const auto& media : post.media) {
        std::cout << "  - " << media.url << " (" << media.type << ")" << std::endl;
    }
    
    std::cout << "Hashtags:" << std::endl;
    for (const auto& tag : post.hashtags) {
        std::cout << "  - " << tag.tag << std::endl;
    }
    
    if (!post.location.empty()) {
        std::cout << "Location: " << post.location << std::endl;
    }
    
    std::cout << "-------------------" << std::endl;
}

int main(int argc, char** argv) {
    try {
        auto socket = std::make_shared<TSocket>("localhost", 9090);
        auto transport = std::make_shared<TBufferedTransport>(socket);
        auto protocol = std::make_shared<TBinaryProtocol>(transport);
        ComposePostServiceClient client(protocol);
        
        transport->open();
        
        // Create some sample media metadata
        std::vector<MediaMetadata> media;
        MediaMetadata image1;
        image1.url = "http://example.com/image1.jpg";
        image1.type = "image/jpeg";
        image1.width = 1920;
        image1.height = 1080;
        image1.size_bytes = 2048576;
        media.push_back(image1);
        
        // Create a post
        Post post;
        std::map<std::string, std::string> metadata{{"filter", "valencia"}, {"device", "iPhone"}};
        client.CreatePost(post, "user123", "Hello from C++ client! #test #cpp",
                        media, PostVisibility::PUBLIC, PostType::IMAGE,
                        "San Francisco, CA", metadata);
        
        std::cout << "Created new post:" << std::endl;
        printPost(post);
        
        // Like and share the post with different users
        std::vector<std::string> test_users = {"user456", "user789", "user101"};
        for (const auto& user : test_users) {
            client.LikePost(post.post_id, user);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            if (std::rand() % 2 == 0) {
                client.SharePost(post.post_id, user);
            }
        }
        
        // Get updated post stats
        PostStats stats;
        client.GetPostStats(stats, post.post_id);
        std::cout << "Updated post stats:" << std::endl
                  << "Likes: " << stats.num_likes << std::endl
                  << "Shares: " << stats.num_shares << std::endl;
        
        // Search for posts with hashtag
        PostFilter filter;
        filter.hashtags = {"test", "cpp"};
        filter.post_types = {PostType::IMAGE};
        std::vector<Post> search_results;
        client.SearchPosts(search_results, "Hello", filter, 10);
        
        std::cout << "\nSearch results:" << std::endl;
        for (const auto& result : search_results) {
            printPost(result);
        }
        
        // Get trending hashtags
        std::vector<Hashtag> trending;
        client.GetTrendingHashtags(trending, 5);
        
        std::cout << "\nTrending hashtags:" << std::endl;
        for (const auto& tag : trending) {
            std::cout << "  #" << tag.tag << std::endl;
        }
        
        // Get user engagement stats
        std::map<std::string, int32_t> engagement_stats;
        client.GetUserEngagementStats(engagement_stats, "user123");
        
        std::cout << "\nUser engagement stats:" << std::endl;
        for (const auto& pair : engagement_stats) {
            std::cout << "  " << pair.first << ": " << pair.second << std::endl;
        }
        
        transport->close();
    } catch (const ServiceException& se) {
        std::cerr << "Service Exception: " << se.error_message << std::endl;
        return 1;
    } catch (const TException& tx) {
        std::cerr << "Transport Exception: " << tx.what() << std::endl;
        return 1;
    }
    
    return 0;
}
