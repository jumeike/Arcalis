namespace cpp composepost

enum PostVisibility {
    PUBLIC = 1,
    FRIENDS_ONLY = 2,
    PRIVATE = 3
}

enum PostType {
    TEXT = 1,
    IMAGE = 2,
    VIDEO = 3,
    LINK = 4
}

exception ServiceException {
    1: i32 error_code
    2: string error_message
}

struct MediaMetadata {
    1: string url
    2: string type
    3: string description
    4: i32 width
    5: i32 height
    6: i64 size_bytes
}

struct UserMention {
    1: string username
    2: i32 start_index
    3: i32 end_index
}

struct Hashtag {
    1: string tag
    2: i32 start_index
    3: i32 end_index
}

struct PostStats {
    1: i32 num_likes
    2: i32 num_comments
    3: i32 num_shares
    4: i32 num_saves
    5: list<string> liked_by
    6: list<string> shared_by
}

struct Post {
    1: i64 post_id
    2: string creator
    3: string text
    4: list<MediaMetadata> media
    5: i64 timestamp
    6: PostVisibility visibility
    7: PostType post_type
    8: list<UserMention> mentions
    9: list<Hashtag> hashtags
    10: PostStats stats
    11: optional string location
    12: optional map<string, string> metadata
}

struct PostFilter {
    1: optional list<PostType> post_types
    2: optional list<string> creators
    3: optional i64 start_time
    4: optional i64 end_time
    5: optional list<string> hashtags
}

service ComposePostService {
    // Core post operations
    Post CreatePost(
        1: string creator,
        2: string text,
        3: list<MediaMetadata> media,
        4: PostVisibility visibility,
        5: PostType post_type,
        6: optional string location,
        7: optional map<string, string> metadata
    ) throws (1: ServiceException se)

    void DeletePost(1: i64 post_id, 2: string creator) throws (1: ServiceException se)
    
    Post UpdatePost(
        1: i64 post_id,
        2: string creator,
        3: optional string text,
        4: optional list<MediaMetadata> media,
        5: optional PostVisibility visibility
    ) throws (1: ServiceException se)
    
    Post GetPost(1: i64 post_id) throws (1: ServiceException se)
    
    // Timeline operations
    list<Post> GetUserTimeline(
        1: string username,
        2: i32 start,
        3: i32 stop,
        4: optional PostFilter filter
    ) throws (1: ServiceException se)
    
    list<Post> GetHashtagTimeline(
        1: string hashtag,
        2: i32 start,
        3: i32 stop
    ) throws (1: ServiceException se)
    
    // Engagement operations
    void LikePost(1: i64 post_id, 2: string username) throws (1: ServiceException se)
    void UnlikePost(1: i64 post_id, 2: string username) throws (1: ServiceException se)
    void SharePost(1: i64 post_id, 2: string username) throws (1: ServiceException se)
    void SavePost(1: i64 post_id, 2: string username) throws (1: ServiceException se)
    void UnsavePost(1: i64 post_id, 2: string username) throws (1: ServiceException se)
    
    // Stats operations
    PostStats GetPostStats(1: i64 post_id) throws (1: ServiceException se)
    map<i64, PostStats> GetBulkPostStats(1: list<i64> post_ids) throws (1: ServiceException se)
    
    // Analysis operations
    list<Hashtag> GetTrendingHashtags(1: i32 limit) throws (1: ServiceException se)
    map<string, i32> GetUserEngagementStats(1: string username) throws (1: ServiceException se)
    
    // Search operations
    list<Post> SearchPosts(
        1: string query,
        2: PostFilter filter,
        3: i32 limit
    ) throws (1: ServiceException se)
}
