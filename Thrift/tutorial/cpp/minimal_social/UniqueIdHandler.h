#ifndef MINIMAL_SOCIAL_UNIQUE_ID_HANDLER_H
#define MINIMAL_SOCIAL_UNIQUE_ID_HANDLER_H

#include <atomic>
#include "gen-cpp/UniqueIdService.h"

namespace minimal_social {

class UniqueIdHandler : public UniqueIdServiceIf {
public:
    UniqueIdHandler() : _next_id(1) {}

    int64_t generateId(const int64_t req_id) override {
        return _next_id.fetch_add(1);
    }

private:
    std::atomic<int64_t> _next_id;
};

} // namespace minimal_social

#endif
