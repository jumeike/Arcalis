namespace cpp minimal_social
namespace py minimal_social
namespace lua minimal_social

enum ErrorCode {
    SE_THRIFT_CONN_ERROR,
    SE_UNAUTHORIZED,
    SE_THRIFT_HANDLER_ERROR
}

exception ServiceException {
    1: ErrorCode errorCode;
    2: string message;
}

struct User {
    1: i64 user_id;
    2: string username;
}

struct Post {
    1: i64 post_id;
    2: i64 user_id;
    3: string username;
    4: string text;
    5: i64 timestamp;
}

service UserService {
    User getUser(
        1: i64 req_id,
        2: string username
    ) throws (1: ServiceException se)
}

service UniqueIdService {
    i64 generateId(
        1: i64 req_id
    ) throws (1: ServiceException se)
}

service ComposePostService {
    void ComposePost(
        1: i64 req_id,
        2: string username,
        3: string text
    ) throws (1: ServiceException se)
}
