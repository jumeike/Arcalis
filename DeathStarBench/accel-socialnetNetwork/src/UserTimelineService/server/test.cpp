#include <sw/redis++/redis++.h>
#include <iostream>

using namespace sw::redis;

int main() {
    try {
        ConnectionOptions opts;
        opts.host = "127.0.0.1";  // or "localhost"
        opts.port = 6379;
        opts.connect_timeout = std::chrono::milliseconds(3000);
        Redis redis(opts);

        std::cout << "PING result: " << redis.ping() << std::endl;
    } catch (const Error &err) {
        std::cerr << "Redis error: " << err.what() << std::endl;
    }
}

