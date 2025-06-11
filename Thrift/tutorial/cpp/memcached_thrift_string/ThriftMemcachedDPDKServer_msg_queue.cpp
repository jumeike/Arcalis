// System headers
#include <memory>
#include <string> 
#include <iostream>
#include <functional>
#include <stdexcept>
#include <stdint.h>
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <algorithm>
#include <cassert>
#include <assert.h>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <sstream> 
#include <exception>
#include <sys/types.h>
#include <map>
#include <vector>
#include <climits>
#include <boost/numeric/conversion/cast.hpp>
#include <queue>
#include <future>

// Network headers
#include <netinet/in.h>
#include <arpa/inet.h>
// #ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
// #endif
// #ifdef HAVE_SYS_RESOURCE_H 
#include <sys/resource.h>
// #endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif

// Thrift headers
#include <thrift/TConfiguration.h>
#include <thrift/Thrift.h>
#include <thrift/TUuid.h>
#include <thrift/thrift-config.h>
#include <thrift/transport/PlatformSocket.h> 
#include <thrift/async/TConcurrentClientSyncInfo.h>
#include "gen-cpp/memcached_benchmark_types.h"

// DPDK headers
//#include <rte_eal.h>
//#include <rte_ethdev.h>
//#include <rte_mbuf.h>
//#include <rte_mempool.h>
//#include <rte_udp.h>
//#include <rte_ip.h>
//#include <rte_ether.h>

#include "DPDKHandler.h"

// External dependencies
#include <libmemcached/memcached.h>
// #include "gen-cpp/MemcachedService.h"

// #include <thrift/protocol/TBinaryProtocol.h>
// #include <thrift/server/TSimpleServer.h> //not need as TSimpleServer.h is now in the same file
// #include <thrift/transport/TServerUDPSocket.h>
// #include <thrift/transport/TBufferTransports.h>
// #include "gen-cpp/MemcachedService.h"
// #include <libmemcached/memcached.h>
// #include <iostream>
// #include <memory>

//includes from TServerFramework.h
// #include <memory>
// #include <stdint.h>
// #include <thrift/TProcessor.h>
// #include <thrift/concurrency/Monitor.h>
// #include <thrift/server/TConnectedClient.h>
// #include <thrift/server/TServer.h>
// #include <thrift/transport/TServerTransport.h>
// #include <thrift/transport/TTransport.h>

//includes from TServerFramework.cpp
// #include <algorithm>
// #include <functional>
// #include <stdexcept>
// #include <stdint.h>

//includes from TSimpleServer.h
// #include <thrift/server/TServerFramework.h> //not need as TServerFramework.h is now in the same file

//includes from TSimpleServer.cpp
// #include <thrift/server/TSimpleServer.h> //not need as TSimpleServer.h is now in the same file

//includes from TProcessor.h
// #include <string>
// #include <thrift/protocol/TProtocol.h>

//includes from TDispatchProcessor.h
// #include <thrift/TProcessor.h>

// includes from TConnectedClient.h
// #include <memory>
// #include <thrift/TProcessor.h>
// #include <thrift/protocol/TProtocol.h>
// #include <thrift/server/TServer.h>
// #include <thrift/transport/TTransport.h>

// includes from Thread.h
// #include <memory>
// #include <thread>
// #include <thrift/concurrency/Monitor.h>

// includes from Monitor.h
// #include <chrono>
// #include <thrift/concurrency/Exception.h>
// #include <thrift/concurrency/Mutex.h>
// #include <thrift/TNonCopyable.h>

// includes from Monitor.cpp
// #include <thrift/thrift-config.h>
// #include <thrift/concurrency/Monitor.h>
// #include <thrift/concurrency/Exception.h>
// #include <thrift/transport/PlatformSocket.h> //might not include in this file
// #include <assert.h>
// #include <condition_variable>
// #include <chrono>
// #include <thread>
// #include <mutex>

// includes from Mutex.h
// #include <memory>
// #include <thrift/TNonCopyable.h>

// includes from Mutex.cpp
// #include <thrift/concurrency/Mutex.h>
// #include <chrono>
// #include <mutex>

// includes from TNonCopyable.h
// Nothing

// includes from Exception.h
// #include <exception>
// #include <thrift/Thrift.h>

// includes from TServer.cpp
// #include <thrift/thrift-config.h>

// #ifdef HAVE_SYS_TIME_H
// #include <sys/time.h>
// #endif
// #ifdef HAVE_SYS_RESOURCE_H
// #include <sys/resource.h>
// #endif

// #ifdef HAVE_UNISTD_H
// #include <unistd.h>
// #endif

// includes from TServer.h
// #include <thrift/TProcessor.h>
// #include <thrift/transport/TServerTransport.h>
// #include <thrift/protocol/TBinaryProtocol.h>
// #include <thrift/concurrency/Thread.h>
// #include <memory>

// includes from TServerTransport.h
// #include <thrift/transport/TTransport.h>
// #include <thrift/transport/TTransportException.h>

// includes from TTransport.h
// #include <thrift/Thrift.h>
// #include <thrift/TConfiguration.h>
// #include <thrift/transport/TTransportException.h>
// #include <memory>
// #include <string>

// includes from TTransportException.h
// #include <boost/numeric/conversion/cast.hpp>
// #include <string>
// #include <thrift/Thrift.h>

// includes from TTransportException.cpp
// #include <thrift/transport/TTransportException.h>
// #include <cstring>
// #include <thrift/thrift-config.h>

// includes from TBinaryProtocol.h
// #include <thrift/protocol/TProtocol.h>
// #include <thrift/protocol/TVirtualProtocol.h>
// #include <memory>

// includes from TBinaryProtocol.tcc
// #include <thrift/protocol/TBinaryProtocol.h>
// #include <thrift/transport/TTransportException.h>
// #include <limits>

// includes from TProtocol.h
#ifdef _WIN32
// Including Winsock2.h adds problematic macros like min() and max().
// Try to work around:
#ifndef NOMINMAX
#define NOMINMAX
#define _THRIFT_UNDEF_NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#define _THRIFT_UNDEF_WIN32_LEAN_AND_MEAN
#endif
// Need to come before any Windows.h includes
#include <winsock2.h>
#ifdef _THRIFT_UNDEF_NOMINMAX
#undef NOMINMAX
#undef _THRIFT_UNDEF_NOMINMAX
#endif
#ifdef _THRIFT_UNDEF_WIN32_LEAN_AND_MEAN
#undef WIN32_LEAN_AND_MEAN
#undef _THRIFT_UNDEF_WIN32_LEAN_AND_MEAN
#endif
#endif

// #include <thrift/transport/TTransport.h>
// #include <thrift/protocol/TProtocolException.h>
// #include <thrift/protocol/TEnum.h>
// #include <thrift/protocol/TList.h>
// #include <thrift/protocol/TSet.h>
// #include <thrift/protocol/TMap.h>
// #include <thrift/TUuid.h>

// #include <memory>

// #ifdef HAVE_NETINET_IN_H
// #include <netinet/in.h>
// #endif
// #include <sys/types.h>
// #include <string>
// #include <map>
// #include <vector>
// #include <climits>

// Use this to get around strict aliasing rules.
// For example, uint64_t i = bitwise_cast<uint64_t>(returns_double());
// The most obvious implementation is to just cast a pointer,
// but that doesn't work.
// For a pretty in-depth explanation of the problem, see
// http://cellperformance.beyond3d.com/articles/2006/06/understanding-strict-aliasing.html

namespace shared {
    struct MemcachedMessage {
        enum Type { GET, SET, GET_RESPONSE, SET_RESPONSE };
        Type type;
        std::string key;
        std::string value;
        bool success;
        std::promise<void> completed;
    };

    // Message queue for communication
    class MessageQueue {
    public:
        static MessageQueue& getInstance() {
            static MessageQueue instance;
            return instance;
        }

        void pushRequest(std::shared_ptr<MemcachedMessage> msg) {
            std::lock_guard<std::mutex> lock(mutex_);
            requestQueue_.push(msg);
            cv_.notify_one();
        }

        std::shared_ptr<MemcachedMessage> getRequest() {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this] { return !requestQueue_.empty(); });
            auto msg = requestQueue_.front();
            requestQueue_.pop();
            return msg;
        }

    private:
        std::queue<std::shared_ptr<MemcachedMessage>> requestQueue_;
        std::mutex mutex_;
        std::condition_variable cv_;
    };
}

namespace my_thrift {
template <typename To, typename From>
static inline To bitwise_cast(From from) {
  static_assert(sizeof(From) == sizeof(To), "sizeof(From) == sizeof(To)");

  // BAD!!!  These are all broken with -O2.
  //return *reinterpret_cast<To*>(&from);  // BAD!!!
  //return *static_cast<To*>(static_cast<void*>(&from));  // BAD!!!
  //return *(To*)(void*)&from;  // BAD!!!

  // Super clean and paritally blessed by section 3.9 of the standard.
  //unsigned char c[sizeof(from)];
  //memcpy(c, &from, sizeof(from));
  //To to;
  //memcpy(&to, c, sizeof(c));
  //return to;

  // Slightly more questionable.
  // Same code emitted by GCC.
  //To to;
  //memcpy(&to, &from, sizeof(from));
  //return to;

  // Technically undefined, but almost universally supported,
  // and the most efficient implementation.
  union {
    From f;
    To t;
  } u;
  u.f = from;
  return u.t;
}


// #ifdef HAVE_SYS_PARAM_H
// #include <sys/param.h>
// #endif

#ifdef __ZEPHYR__
#  include <zephyr/sys/byteorder.h>

#  define __THRIFT_BYTE_ORDER __BYTE_ORDER__
#  define __THRIFT_LITTLE_ENDIAN __ORDER_LITTLE_ENDIAN__
#  define __THRIFT_BIG_ENDIAN __ORDER_BIG_ENDIAN__

#  if __THRIFT_BYTE_ORDER == __THRIFT_BIG_ENDIAN
#    undef bswap_64
#    undef bswap_32
#    undef bswap_16
#  endif
#endif

#ifndef __THRIFT_BYTE_ORDER
# if defined(BYTE_ORDER) && defined(LITTLE_ENDIAN) && defined(BIG_ENDIAN)
#  define __THRIFT_BYTE_ORDER BYTE_ORDER
#  define __THRIFT_LITTLE_ENDIAN LITTLE_ENDIAN
#  define __THRIFT_BIG_ENDIAN BIG_ENDIAN
# else
#  include <boost/predef/other/endian.h>
#  if BOOST_ENDIAN_BIG_BYTE
#    define __THRIFT_BYTE_ORDER 4321
#    define __THRIFT_LITTLE_ENDIAN 0
#    define __THRIFT_BIG_ENDIAN __THRIFT_BYTE_ORDER
#  elif BOOST_ENDIAN_LITTLE_BYTE
#    define __THRIFT_BYTE_ORDER 1234
#    define __THRIFT_LITTLE_ENDIAN __THRIFT_BYTE_ORDER
#    define __THRIFT_BIG_ENDIAN 0
#  endif
#  ifdef BOOST_LITTLE_ENDIAN
#  else
#  endif
# endif
#endif

#if __THRIFT_BYTE_ORDER == __THRIFT_BIG_ENDIAN
# if !defined(THRIFT_ntohll)
#  define THRIFT_ntohll(n) (n)
#  define THRIFT_htonll(n) (n)
# endif
# if defined(__GNUC__) && defined(__GLIBC__)
#  include <byteswap.h>
#  define THRIFT_htolell(n) bswap_64(n)
#  define THRIFT_letohll(n) bswap_64(n)
#  define THRIFT_htolel(n) bswap_32(n)
#  define THRIFT_letohl(n) bswap_32(n)
#  define THRIFT_htoles(n) bswap_16(n)
#  define THRIFT_letohs(n) bswap_16(n)
# else /* GNUC & GLIBC */
#  define bswap_64(n) \
      ( (((n) & 0xff00000000000000ull) >> 56) \
      | (((n) & 0x00ff000000000000ull) >> 40) \
      | (((n) & 0x0000ff0000000000ull) >> 24) \
      | (((n) & 0x000000ff00000000ull) >> 8)  \
      | (((n) & 0x00000000ff000000ull) << 8)  \
      | (((n) & 0x0000000000ff0000ull) << 24) \
      | (((n) & 0x000000000000ff00ull) << 40) \
      | (((n) & 0x00000000000000ffull) << 56) )
#  define bswap_32(n) \
      ( (((n) & 0xff000000ul) >> 24) \
      | (((n) & 0x00ff0000ul) >> 8)  \
      | (((n) & 0x0000ff00ul) << 8)  \
      | (((n) & 0x000000fful) << 24) )
#  define bswap_16(n) \
      ( (((n) & ((unsigned short)0xff00ul)) >> 8)  \
      | (((n) & ((unsigned short)0x00fful)) << 8)  )
#  define THRIFT_htolell(n) bswap_64(n)
#  define THRIFT_letohll(n) bswap_64(n)
#  define THRIFT_htolel(n) bswap_32(n)
#  define THRIFT_letohl(n) bswap_32(n)
#  define THRIFT_htoles(n) bswap_16(n)
#  define THRIFT_letohs(n) bswap_16(n)
# endif /* GNUC & GLIBC */
#elif __THRIFT_BYTE_ORDER == __THRIFT_LITTLE_ENDIAN
#  define THRIFT_htolell(n) (n)
#  define THRIFT_letohll(n) (n)
#  define THRIFT_htolel(n) (n)
#  define THRIFT_letohl(n) (n)
#  define THRIFT_htoles(n) (n)
#  define THRIFT_letohs(n) (n)
# if defined(__GNUC__) && defined(__GLIBC__)
#  include <byteswap.h>
#  define THRIFT_ntohll(n) bswap_64(n)
#  define THRIFT_htonll(n) bswap_64(n)
# elif defined(_MSC_VER) /* Microsoft Visual C++ */
#  define THRIFT_ntohll(n) ( _byteswap_uint64((uint64_t)n) )
#  define THRIFT_htonll(n) ( _byteswap_uint64((uint64_t)n) )
# elif !defined(THRIFT_ntohll) /* Not GNUC/GLIBC or MSVC */
#  define THRIFT_ntohll(n) ( (((uint64_t)ntohl((uint32_t)n)) << 32) + ntohl((uint32_t)(n >> 32)) )
#  define THRIFT_htonll(n) ( (((uint64_t)htonl((uint32_t)n)) << 32) + htonl((uint32_t)(n >> 32)) )
# endif /* GNUC/GLIBC or MSVC or something else */
#else /* __THRIFT_BYTE_ORDER */
# error "Can't define THRIFT_htonll or THRIFT_ntohll!"
#endif

// includes from TProtocol.cpp
// #include <thrift/protocol/TProtocol.h>

// includes from TVirtualProtocol.h
// #include <thrift/protocol/TProtocol.h>

// includes from TEnum.h
// Nothing

// includes from TList.h
// #include <thrift/protocol/TEnum.h>

// includes from TSet.h
// #include <thrift/protocol/TEnum.h>
// #include <thrift/protocol/TList.h>

// includes from TMap.h
// #include <thrift/protocol/TEnum.h>

// includes from TProtocolException.h
// #include <string>

// includes from TServerUDPSocket.h
// #include <functional>
// #include <memory>
// #include <rte_eal.h>
// #include <rte_ethdev.h>
// #include <rte_mbuf.h>
// #include <rte_mempool.h>
// #include "DPDKResources.h"
// #include <thrift/transport/TServerTransport.h>
// #include <thrift/concurrency/Mutex.h>

// includes from TServerUDPSocket.cpp
// #include <thrift/transport/TServerUDPSocket.h>
// #include <thrift/transport/TUDPSocket.h>
// #include <thrift/transport/TTransportException.h>

// includes from TUDPSocket.h
// #include <string>
// #include <sstream> 
// #include <netinet/in.h>  
// #include <arpa/inet.h>  
// #include <rte_eal.h>
// #include <rte_ethdev.h>
// #include <rte_mbuf.h>
// #include <rte_mempool.h>
// #include "DPDKResources.h"
// #include <thrift/transport/TTransport.h>
// #include <thrift/transport/TVirtualTransport.h>

// includes from TUDPSocket.cpp
// #include <thrift/transport/TUDPSocket.h>
// #include <thrift/transport/TTransportException.h>
// #include <rte_udp.h>
// #include <rte_ip.h>
// #include <rte_ether.h>

// includes from DPDKResources.h
// #include <rte_ethdev.h>
// #include <rte_mempool.h>

// includes from TBufferTransport.h
// #include <cstdlib>
// #include <cstring>
// #include <limits>
// #include <thrift/transport/TTransport.h>
// #include <thrift/transport/TVirtualTransport.h>

#ifdef __GNUC__
#define TDB_LIKELY(val) (__builtin_expect((val), 1))
#define TDB_UNLIKELY(val) (__builtin_expect((val), 0))
#else
#define TDB_LIKELY(val) (val)
#define TDB_UNLIKELY(val) (val)
#endif

// includes from TBufferTransport.cpp
// #include <algorithm>
// #include <cassert>
// #include <cmath>
// #include <thrift/transport/TBufferTransports.h>
// using std::string;

// includes from TVirtualTransport.h
// #include <thrift/transport/TTransport.h>

// includes from TBase.h
// #include <thrift/Thrift.h>
// #include <thrift/protocol/TProtocol.h>

// includes from MemcachedService.h
// #include <thrift/TDispatchProcessor.h>
// #include <thrift/async/TConcurrentClientSyncInfo.h>
// #include <memory>
// #include "memcached_benchmark_types.h"

// includes from TApplicationException.h
// #include <thrift/Thrift.h>

// includes from TApplicationException.cpp
// #include <thrift/TApplicationException.h>
// #include <thrift/protocol/TProtocol.h>

// includes from TConcurrentClientSyncInfo.h
// #include <thrift/protocol/TProtocol.h>
// #include <thrift/concurrency/Mutex.h>
// #include <thrift/concurrency/Monitor.h>
// #include <memory>
// #include <vector>
// #include <string>
// #include <map>

// includes from TConcurrentClientSyncInfo.cpp
// #include <limits>
// #include <memory>
// #include <thrift/TApplicationException.h>
// #include <thrift/async/TConcurrentClientSyncInfo.h>
// #include <thrift/transport/TTransportException.h>

// includes from TThreadedServer.h
// #include <map>
// #include <thrift/concurrency/Monitor.h>
// #include <thrift/concurrency/ThreadFactory.h>
// #include <thrift/concurrency/Thread.h>
// #include <thrift/server/TServerFramework.h>

// includes from TThreadedServer.h
// #include <string>
// #include <memory>
// #include <thrift/concurrency/ThreadFactory.h>
// #include <thrift/server/TThreadedServer.h>

// includes from ThreadFactory.h
// #include <thrift/concurrency/Thread.h>
// #include <memory>

// includes from ThreadFactory.cpp
// #include <thrift/thrift-config.h>
// #include <thrift/concurrency/ThreadFactory.h>
// #include <memory>

namespace apache {
namespace thrift {
  using ::apache::thrift::GlobalOutput;
  using ::apache::thrift::TOutput;
  // ***TNoncopyable.h***
  /**
   * @brief A simple non-copyable base class pattern. Derive from TNonCopyable to
   * make a class non-copyable and prohibit assignment and copy-construction.
   */
  class TNonCopyable {
  protected:
    TNonCopyable() = default;
    virtual ~TNonCopyable() = default;

    TNonCopyable(const TNonCopyable&) = delete;
    TNonCopyable& operator=(const TNonCopyable&) = delete;
  };

  namespace concurrency {
  
  // ***Exception.h***
  class NoSuchTaskException : public ::apache::thrift::TException {};

  class UncancellableTaskException : public ::apache::thrift::TException {};

  class InvalidArgumentException : public ::apache::thrift::TException {};

  class IllegalStateException : public ::apache::thrift::TException {
  public:
    IllegalStateException() = default;
    IllegalStateException(const std::string& message) : ::apache::thrift::TException(message) {}
  };

  class TimedOutException : public ::apache::thrift::TException {
  public:
    TimedOutException() : ::apache::thrift::TException("TimedOutException"){};
    TimedOutException(const std::string& message) : ::apache::thrift::TException(message) {}
  };

  class TooManyPendingTasksException : public ::apache::thrift::TException {
  public:
    TooManyPendingTasksException() : ::apache::thrift::TException("TooManyPendingTasksException"){};
    TooManyPendingTasksException(const std::string& message) : ::apache::thrift::TException(message) {}
  };

  class SystemResourceException : public ::apache::thrift::TException {
  public:
    SystemResourceException() = default;

    SystemResourceException(const std::string& message) : ::apache::thrift::TException(message) {}
  };
  
  // ***Mutex.h***
  /**
   * NOTE: All mutex implementations throw an exception on failure.  See each
   *       specific implementation to understand the exception type(s) used.
   */

  /**
   * A simple mutex class
   *
   * @version $Id:$
   */
  class Mutex {
  public:
    Mutex();
    virtual ~Mutex() = default;

    virtual void lock() const;
    virtual bool trylock() const;
    virtual bool timedlock(int64_t milliseconds) const;
    virtual void unlock() const;

    void* getUnderlyingImpl() const;

  private:
    class impl;
    std::shared_ptr<impl> impl_;
  };


  class Guard : my_thrift::apache::thrift::TNonCopyable {
  public:
    Guard(const Mutex& value, int64_t timeout = 0) : mutex_(&value) {
      if (timeout == 0) {
        value.lock();
      } else if (timeout < 0) {
        if (!value.trylock()) {
          mutex_ = nullptr;
        }
      } else {
        if (!value.timedlock(timeout)) {
          mutex_ = nullptr;
        }
      }
    }
    ~Guard() {
      if (mutex_) {
        mutex_->unlock();
      }
    }

    operator bool() const { return (mutex_ != nullptr); }

  private:
    const Mutex* mutex_;
  };

  // ***Mutex.cpp***
  /**
   * Implementation of Mutex class using C++11 std::timed_mutex
   *
   * Methods throw std::system_error on error.
   *
   * @version $Id:$
   */
  class Mutex::impl : public std::timed_mutex {};

  Mutex::Mutex() : impl_(new Mutex::impl()) {
  }

  void* Mutex::getUnderlyingImpl() const {
    return impl_.get();
  }

  void Mutex::lock() const {
    impl_->lock();
  }

  bool Mutex::trylock() const {
    return impl_->try_lock();
  }

  bool Mutex::timedlock(int64_t ms) const {
    return impl_->try_lock_for(std::chrono::milliseconds(ms));
  }

  void Mutex::unlock() const {
    impl_->unlock();
  }
  // Monitor.h
  /**
   * A monitor is a combination mutex and condition-event.  Waiting and
   * notifying condition events requires that the caller own the mutex.  Mutex
   * lock and unlock operations can be performed independently of condition
   * events.  This is more or less analogous to java.lang.Object multi-thread
   * operations.
   *
   * Note the Monitor can create a new, internal mutex; alternatively, a
   * separate Mutex can be passed in and the Monitor will re-use it without
   * taking ownership.  It's the user's responsibility to make sure that the
   * Mutex is not deallocated before the Monitor.
   *
   * Note that all methods are const.  Monitors implement logical constness, not
   * bit constness.  This allows const methods to call monitor methods without
   * needing to cast away constness or change to non-const signatures.
   *
   * @version $Id:$
   */
  class Monitor : my_thrift::apache::thrift::TNonCopyable {
  public:
    /** Creates a new mutex, and takes ownership of it. */
    Monitor();

    /** Uses the provided mutex without taking ownership. */
    explicit Monitor(Mutex* mutex);

    /** Uses the mutex inside the provided Monitor without taking ownership. */
    explicit Monitor(Monitor* monitor);

    /** Deallocates the mutex only if we own it. */
    virtual ~Monitor();

    Mutex& mutex() const;

    virtual void lock() const;

    virtual void unlock() const;

    /**
     * Waits a maximum of the specified timeout in milliseconds for the condition
     * to occur, or waits forever if timeout is zero.
     *
     * Returns 0 if condition occurs, THRIFT_ETIMEDOUT on timeout, or an error code.
     */
    int waitForTimeRelative(const std::chrono::milliseconds &timeout) const;

    int waitForTimeRelative(uint64_t timeout_ms) const { return waitForTimeRelative(std::chrono::milliseconds(timeout_ms)); }

    /**
     * Waits until the absolute time specified by abstime.
     * Returns 0 if condition occurs, THRIFT_ETIMEDOUT on timeout, or an error code.
     */
    int waitForTime(const std::chrono::time_point<std::chrono::steady_clock>& abstime) const;

    /**
     * Waits forever until the condition occurs.
     * Returns 0 if condition occurs, or an error code otherwise.
     */
    int waitForever() const;

    /**
     * Exception-throwing version of waitForTimeRelative(), called simply
     * wait(std::chrono::milliseconds) for historical reasons.  Timeout is in milliseconds.
     *
     * If the condition occurs, this function returns cleanly; on timeout or
     * error an exception is thrown.
     */
    void wait(const std::chrono::milliseconds &timeout) const;

    void wait(uint64_t timeout_ms = 0ULL) const { this->wait(std::chrono::milliseconds(timeout_ms)); }

    /** Wakes up one thread waiting on this monitor. */
    virtual void notify() const;

    /** Wakes up all waiting threads on this monitor. */
    virtual void notifyAll() const;

  private:
    class Impl;

    Impl* impl_;
  };

  class Synchronized {
  public:
    Synchronized(const Monitor* monitor) : g(monitor->mutex()) {}
    Synchronized(const Monitor& monitor) : g(monitor.mutex()) {}

  private:
    Guard g;
  };
  // ***Monitor.cpp***
  /**
   * Monitor implementation using the std thread library
   *
   * @version $Id:$
   */
  class Monitor::Impl {

  public:
    Impl() : ownedMutex_(new Mutex()), conditionVariable_(), mutex_(nullptr) { init(ownedMutex_.get()); }

    Impl(Mutex* mutex) : ownedMutex_(), conditionVariable_(), mutex_(nullptr) { init(mutex); }

    Impl(Monitor* monitor) : ownedMutex_(), conditionVariable_(), mutex_(nullptr) {
      init(&(monitor->mutex()));
    }

    Mutex& mutex() { return *mutex_; }
    void lock() { mutex_->lock(); }
    void unlock() { mutex_->unlock(); }

    /**
     * Exception-throwing version of waitForTimeRelative(), called simply
     * wait(int64) for historical reasons.  Timeout is in milliseconds.
     *
     * If the condition occurs,  this function returns cleanly; on timeout or
     * error an exception is thrown.
     */
    void wait(const std::chrono::milliseconds &timeout) {
      int result = waitForTimeRelative(timeout);
      if (result == THRIFT_ETIMEDOUT) {
        throw TimedOutException();
      } else if (result != 0) {
        throw ::apache::thrift::TException("Monitor::wait() failed");
      }
    }

    /**
     * Waits until the specified timeout in milliseconds for the condition to
     * occur, or waits forever if timeout is zero.
     *
     * Returns 0 if condition occurs, THRIFT_ETIMEDOUT on timeout, or an error code.
     */
    int waitForTimeRelative(const std::chrono::milliseconds &timeout) {
      if (timeout.count() == 0) {
        return waitForever();
      }

      assert(mutex_);
      auto* mutexImpl = static_cast<std::timed_mutex*>(mutex_->getUnderlyingImpl());
      assert(mutexImpl);

      std::unique_lock<std::timed_mutex> lock(*mutexImpl, std::adopt_lock);
      bool timedout = (conditionVariable_.wait_for(lock, timeout)
                      == std::cv_status::timeout);
      lock.release();
      return (timedout ? THRIFT_ETIMEDOUT : 0);
    }

    /**
     * Waits until the absolute time specified by abstime.
     * Returns 0 if condition occurs, THRIFT_ETIMEDOUT on timeout, or an error code.
     */
    int waitForTime(const std::chrono::time_point<std::chrono::steady_clock>& abstime) {
      assert(mutex_);
      auto* mutexImpl = static_cast<std::timed_mutex*>(mutex_->getUnderlyingImpl());
      assert(mutexImpl);

      std::unique_lock<std::timed_mutex> lock(*mutexImpl, std::adopt_lock);
      bool timedout = (conditionVariable_.wait_until(lock, abstime)
                      == std::cv_status::timeout);
      lock.release();
      return (timedout ? THRIFT_ETIMEDOUT : 0);
    }

    /**
     * Waits forever until the condition occurs.
     * Returns 0 if condition occurs, or an error code otherwise.
     */
    int waitForever() {
      assert(mutex_);
      auto* mutexImpl = static_cast<std::timed_mutex*>(mutex_->getUnderlyingImpl());
      assert(mutexImpl);

      std::unique_lock<std::timed_mutex> lock(*mutexImpl, std::adopt_lock);
      conditionVariable_.wait(lock);
      lock.release();
      return 0;
    }

    void notify() { conditionVariable_.notify_one(); }

    void notifyAll() { conditionVariable_.notify_all(); }

  private:
    void init(Mutex* mutex) { mutex_ = mutex; }

    const std::unique_ptr<Mutex> ownedMutex_;
    std::condition_variable_any conditionVariable_;
    Mutex* mutex_;
  };

  Monitor::Monitor() : impl_(new Monitor::Impl()) {
  }
  Monitor::Monitor(Mutex* mutex) : impl_(new Monitor::Impl(mutex)) {
  }
  Monitor::Monitor(Monitor* monitor) : impl_(new Monitor::Impl(monitor)) {
  }

  Monitor::~Monitor() {
    delete impl_;
  }

  Mutex& Monitor::mutex() const {
    return const_cast<Monitor::Impl*>(impl_)->mutex();
  }

  void Monitor::lock() const {
    const_cast<Monitor::Impl*>(impl_)->lock();
  }

  void Monitor::unlock() const {
    const_cast<Monitor::Impl*>(impl_)->unlock();
  }

  void Monitor::wait(const std::chrono::milliseconds &timeout) const {
    const_cast<Monitor::Impl*>(impl_)->wait(timeout);
  }

  int Monitor::waitForTime(const std::chrono::time_point<std::chrono::steady_clock>& abstime) const {
    return const_cast<Monitor::Impl*>(impl_)->waitForTime(abstime);
  }

  int Monitor::waitForTimeRelative(const std::chrono::milliseconds &timeout) const {
    return const_cast<Monitor::Impl*>(impl_)->waitForTimeRelative(timeout);
  }

  int Monitor::waitForever() const {
    return const_cast<Monitor::Impl*>(impl_)->waitForever();
  }

  void Monitor::notify() const {
    const_cast<Monitor::Impl*>(impl_)->notify();
  }

  void Monitor::notifyAll() const {
    const_cast<Monitor::Impl*>(impl_)->notifyAll();
  }
  // ***Thread.h***
  class Thread;

  /**
   * Minimal runnable class.  More or less analogous to java.lang.Runnable.
   *
   * @version $Id:$
   */
  class Runnable {

  public:
    virtual ~Runnable() = default;
    virtual void run() = 0;

    /**
     * Gets the thread object that is hosting this runnable object  - can return
     * an empty boost::shared pointer if no references remain on that thread object
     */
    virtual std::shared_ptr<Thread> thread() { return thread_.lock(); }

    /**
     * Sets the thread that is executing this object.  This is only meant for
     * use by concrete implementations of Thread.
     */
    virtual void thread(std::shared_ptr<Thread> value) { thread_ = value; }

  private:
    std::weak_ptr<Thread> thread_;
  };

  /**
   * Minimal thread class. Returned by thread factory bound to a Runnable object
   * and ready to start execution.  More or less analogous to java.lang.Thread
   * (minus all the thread group, priority, mode and other baggage, since that
   * is difficult to abstract across platforms and is left for platform-specific
   * ThreadFactory implementations to deal with
   *
   * @see apache::thrift::concurrency::ThreadFactory)
   */
  class Thread : public std::enable_shared_from_this<Thread> {

  public:
    typedef std::thread::id id_t;
    typedef void (*thread_funct_t)(std::shared_ptr<Thread> );

    enum STATE { uninitialized, starting, started, stopping, stopped };

    static void threadMain(std::shared_ptr<Thread> thread);

    static inline bool is_current(id_t t) { return t == std::this_thread::get_id(); }
    static inline id_t get_current() { return std::this_thread::get_id(); }

    Thread(bool detached, std::shared_ptr<Runnable> runnable)
      : state_(uninitialized), detached_(detached) {
      this->_runnable = runnable;
    }

    virtual ~Thread() {
      if (!detached_ && thread_->joinable()) {
        try {
          join();
        } catch (...) {
          // We're really hosed.
        }
      }
    }

    STATE getState() const
    {
      Synchronized sync(monitor_);
      return state_;
    }

    void setState(STATE newState)
    {
      Synchronized sync(monitor_);
      state_ = newState;

      // unblock start() with the knowledge that the thread has actually
      // started running, which avoids a race in detached threads.
      if (newState == started) {
      monitor_.notify();
      }
    }

    /**
     * Starts the thread. Does platform specific thread creation and
     * configuration then invokes the run method of the Runnable object bound
     * to this thread.
     */
    virtual void start() {
      if (getState() != uninitialized) {
        return;
      }

      std::shared_ptr<Thread> selfRef = shared_from_this();
      setState(starting);

      Synchronized sync(monitor_);
      thread_ = std::unique_ptr<std::thread>(new std::thread(getThreadFunc(), selfRef));

      if (detached_)
        thread_->detach();
      
      // Wait for the thread to start and get far enough to grab everything
      // that it needs from the calling context, thus absolving the caller
      // from being required to hold on to runnable indefinitely.
      monitor_.wait();
    }

    /**
     * Join this thread. If this thread is joinable, the calling thread blocks
     * until this thread completes.  If the target thread is not joinable, then
     * nothing happens.
     */
    virtual void join() {
      if (!detached_ && state_ != uninitialized) {
        thread_->join();
      }
    }

    /**
     * Gets the thread's platform-specific ID
     */
    Thread::id_t getId() const { return thread_.get() ? thread_->get_id() : std::thread::id(); }

    /**
     * Gets the runnable object this thread is hosting
     */
    std::shared_ptr<Runnable> runnable() const { return _runnable; }

  protected:

    virtual thread_funct_t getThreadFunc() const {
        return threadMain;
    } 

  private:
    std::shared_ptr<Runnable> _runnable;
    std::unique_ptr<std::thread> thread_;
    Monitor monitor_;
    STATE state_;
    bool detached_;
  };
  // ***Thread.cpp***
  void Thread::threadMain(std::shared_ptr<Thread> thread) {
    thread->setState(started);
    thread->runnable()->run();

    if (thread->getState() != stopping && thread->getState() != stopped) {
      thread->setState(stopping);
    }
  }

  // ***ThreadFactory.h***
  /**
   * Factory to create thread object and bind them to Runnable
   * object for execution
   */
  class ThreadFactory {
  public:
    /**
     * All threads created by a factory are reference-counted
     * via std::shared_ptr.  The factory guarantees that threads and the Runnable tasks
     * they host will be properly cleaned up once the last strong reference
     * to both is given up.
     *
     * By default threads are not joinable.
     */
    ThreadFactory(bool detached = true) : detached_(detached) { }

    virtual ~ThreadFactory() = default;

    /**
     * Gets current detached mode
     */
    bool isDetached() const { return detached_; }

    /**
     * Sets the detached disposition of newly created threads.
     */
    void setDetached(bool detached) { detached_ = detached; }

    /**
     * Create a new thread.
     */
    virtual std::shared_ptr<Thread> newThread(std::shared_ptr<Runnable> runnable) const;

    /**
     * Gets the current thread id or unknown_thread_id if the current thread is not a thrift thread
     */
    Thread::id_t getCurrentThreadId() const;

  private:
    bool detached_;
  };
  // ***ThreadFactory.cpp***
  std::shared_ptr<Thread> ThreadFactory::newThread(std::shared_ptr<Runnable> runnable) const {
    std::shared_ptr<Thread> result = std::make_shared<Thread>(isDetached(), runnable);
    runnable->thread(result);
    return result;
  }

  Thread::id_t ThreadFactory::getCurrentThreadId() const {
    return std::this_thread::get_id();
  }

  } // my_thrift::apache::thrift::concurrency

  namespace transport {
  
  // ***DPDKResources.h***
  struct DPDKResources {
      uint16_t portId;
      struct rte_mempool* mbufPool;
      struct rte_eth_dev_info devInfo;
      struct rte_eth_conf portConf;
      bool isInitialized;
      
      DPDKResources() 
          : portId(0)
          , mbufPool(nullptr)
          , isInitialized(true) {
          memset(&devInfo, 0, sizeof(devInfo));
          memset(&portConf, 0, sizeof(portConf));
      }
  };
  
  // ***TTransportException.h***
  /**
   * Class to encapsulate all the possible types of transport errors that may
   * occur in various transport systems. This provides a sort of generic
   * wrapper around the vague UNIX E_ error codes that lets a common code
   * base of error handling to be used for various types of transports, i.e.
   * pipes etc.
   *
   */
  class TTransportException : public ::apache::thrift::TException {
  public:
    /**
     * Error codes for the various types of exceptions.
     */
    enum TTransportExceptionType {
      UNKNOWN = 0,
      NOT_OPEN = 1,
      TIMED_OUT = 2,
      END_OF_FILE = 3,
      INTERRUPTED = 4,
      BAD_ARGS = 5,
      CORRUPTED_DATA = 6,
      INTERNAL_ERROR = 7,
      CLIENT_DISCONNECT = 8
    };

    TTransportException() : ::apache::thrift::TException(), type_(UNKNOWN) {}

    TTransportException(TTransportExceptionType type) : ::apache::thrift::TException(), type_(type) {}

    TTransportException(const std::string& message)
      : ::apache::thrift::TException(message), type_(UNKNOWN) {}

    TTransportException(TTransportExceptionType type, const std::string& message)
      : ::apache::thrift::TException(message), type_(type) {}

    TTransportException(TTransportExceptionType type, const std::string& message, int errno_copy)
      : ::apache::thrift::TException(message + ": " + TOutput::strerror_s(errno_copy)), type_(type) {}

    ~TTransportException() noexcept override = default;

    /**
     * Returns an error code that provides information about the type of error
     * that has occurred.
     *
     * @return Error code
     */
    TTransportExceptionType getType() const noexcept { return type_; }

    const char* what() const noexcept override;

  protected:
    /** Just like strerror_r but returns a C++ string object. */
    std::string strerror_s(int errno_copy);

    /** Error code */
    TTransportExceptionType type_;
  };

  /**
   * Legacy code in transport implementations have overflow issues
   * that need to be enforced.
   */
  template <typename To, typename From> To safe_numeric_cast(From i) {
    try {
      return boost::numeric_cast<To>(i);
    }
    catch (const std::bad_cast& bc) {
      throw TTransportException(TTransportException::CORRUPTED_DATA,
                                bc.what());
    }
  }
  // ***TTransportException.cpp***
  const char* TTransportException::what() const noexcept {
    if (message_.empty()) {
      switch (type_) {
      case UNKNOWN:
        return "TTransportException: Unknown transport exception";
      case NOT_OPEN:
        return "TTransportException: Transport not open";
      case TIMED_OUT:
        return "TTransportException: Timed out";
      case END_OF_FILE:
        return "TTransportException: End of file";
      case INTERRUPTED:
        return "TTransportException: Interrupted";
      case BAD_ARGS:
        return "TTransportException: Invalid arguments";
      case CORRUPTED_DATA:
        return "TTransportException: Corrupted Data";
      case INTERNAL_ERROR:
        return "TTransportException: Internal error";
      default:
        return "TTransportException: (Invalid exception type)";
      }
    } else {
      return message_.c_str();
    }
  }
  // ***TTransport.h***
  /**
   * Helper template to hoist readAll implementation out of TTransport
   */
  // Johnson
  // class TBufferBase;
  template <class Transport_>
  uint32_t readAll(Transport_& trans, uint8_t* buf, uint32_t len) { //links from TBufferTransports.h:84
    uint32_t have = 0;
    uint32_t get = 0;

    while (have < len) {
      get = trans.read(buf + have, len - have); // links to TBufferTransports.h:63 
      if (get <= 0) {
        throw TTransportException(TTransportException::END_OF_FILE, "No more data to read.");
      }
      have += get;
    }

    return have;
  }

  /**
   * Generic interface for a method of transporting data. A TTransport may be
   * capable of either reading or writing, but not necessarily both.
   *
   */
  class TTransport {
  public:
    TTransport(std::shared_ptr<::apache::thrift::TConfiguration> config = nullptr) { 
      if(config == nullptr) {
        configuration_ = std::shared_ptr<::apache::thrift::TConfiguration> (new ::apache::thrift::TConfiguration());
      } else {
        configuration_ = config;
      }
      resetConsumedMessageSize(); 
    }

    /**
     * Virtual deconstructor.
     */
    virtual ~TTransport() = default;

    /**
     * Whether this transport is open.
     */
    virtual bool isOpen() const { return false; }

    /**
     * Tests whether there is more data to read or if the remote side is
     * still open. By default this is true whenever the transport is open,
     * but implementations should add logic to test for this condition where
     * possible (i.e. on a socket).
     * This is used by a server to check if it should listen for another
     * request.
     */
    virtual bool peek() { return isOpen(); }

    /**
     * Opens the transport for communications.
     *
     * @return bool Whether the transport was successfully opened
     * @throws TTransportException if opening failed
     */
    virtual void open() {
      throw TTransportException(TTransportException::NOT_OPEN, "Cannot open base TTransport.");
    }

    /**
     * Closes the transport.
     */
    virtual void close() {
      throw TTransportException(TTransportException::NOT_OPEN, "Cannot close base TTransport.");
    }

    /**
     * Attempt to read up to the specified number of bytes into the string.
     *
     * @param buf  Reference to the location to write the data
     * @param len  How many bytes to read
     * @return How many bytes were actually read
     * @throws TTransportException If an error occurs
     */
    uint32_t read(uint8_t* buf, uint32_t len) { // links from TBufferedTransport.cpp:54 (readslow)
      T_VIRTUAL_CALL();
      return read_virt(buf, len); //links to TVirtualTransport.h:87 (read_virt)
    }
    virtual uint32_t read_virt(uint8_t* /* buf */, uint32_t /* len */) {
      throw TTransportException(TTransportException::NOT_OPEN, "Base TTransport cannot read.");
    }

    /**
     * Reads the given amount of data in its entirety no matter what.
     *
     * @param s     Reference to location for read data
     * @param len   How many bytes to read
     * @return How many bytes read, which must be equal to size
     * @throws TTransportException If insufficient data was read
     */
    uint32_t readAll(uint8_t* buf, uint32_t len) { // links from TBinaryProtocol.tcc:393
      T_VIRTUAL_CALL();
      return readAll_virt(buf, len); // links to TVirtualTransport.h:91 (readAll_virt) 
    }
    virtual uint32_t readAll_virt(uint8_t* buf, uint32_t len) {
      return my_thrift::apache::thrift::transport::readAll(*this, buf, len);
    }

    /**
     * Called when read is completed.
     * This can be over-ridden to perform a transport-specific action
     * e.g. logging the request to a file
     *
     * @return number of bytes read if available, 0 otherwise.
     */
    virtual uint32_t readEnd() {
      // default behaviour is to do nothing
      return 0;
    }

    /**
     * Writes the string in its entirety to the buffer.
     *
     * Note: You must call flush() to ensure the data is actually written,
     * and available to be read back in the future.  Destroying a TTransport
     * object does not automatically flush pending data--if you destroy a
     * TTransport object with written but unflushed data, that data may be
     * discarded.
     *
     * @param buf  The data to write out
     * @throws TTransportException if an error occurs
     */
    void write(const uint8_t* buf, uint32_t len) {
      T_VIRTUAL_CALL();
      write_virt(buf, len);
    }
    virtual void write_virt(const uint8_t* /* buf */, uint32_t /* len */) {
      throw TTransportException(TTransportException::NOT_OPEN, "Base TTransport cannot write.");
    }

    /**
     * Called when write is completed.
     * This can be over-ridden to perform a transport-specific action
     * at the end of a request.
     *
     * @return number of bytes written if available, 0 otherwise
     */
    virtual uint32_t writeEnd() {
      // default behaviour is to do nothing
      return 0;
    }

    /**
     * Flushes any pending data to be written. Typically used with buffered
     * transport mechanisms.
     *
     * @throws TTransportException if an error occurs
     */
    virtual void flush() {
      // default behaviour is to do nothing
    }

    /**
     * Attempts to return a pointer to \c len bytes, possibly copied into \c buf.
     * Does not consume the bytes read (i.e.: a later read will return the same
     * data).  This method is meant to support protocols that need to read
     * variable-length fields.  They can attempt to borrow the maximum amount of
     * data that they will need, then consume (see next method) what they
     * actually use.  Some transports will not support this method and others
     * will fail occasionally, so protocols must be prepared to use read if
     * borrow fails.
     *
     * @oaram buf  A buffer where the data can be stored if needed.
     *             If borrow doesn't return buf, then the contents of
     *             buf after the call are undefined.  This parameter may be
     *             nullptr to indicate that the caller is not supplying storage,
     *             but would like a pointer into an internal buffer, if
     *             available.
     * @param len  *len should initially contain the number of bytes to borrow.
     *             If borrow succeeds, *len will contain the number of bytes
     *             available in the returned pointer.  This will be at least
     *             what was requested, but may be more if borrow returns
     *             a pointer to an internal buffer, rather than buf.
     *             If borrow fails, the contents of *len are undefined.
     * @return If the borrow succeeds, return a pointer to the borrowed data.
     *         This might be equal to \c buf, or it might be a pointer into
     *         the transport's internal buffers.
     * @throws TTransportException if an error occurs
     */
    const uint8_t* borrow(uint8_t* buf, uint32_t* len) {
      T_VIRTUAL_CALL();
      return borrow_virt(buf, len);
    }
    virtual const uint8_t* borrow_virt(uint8_t* /* buf */, uint32_t* /* len */) { return nullptr; }

    /**
     * Remove len bytes from the transport.  This should always follow a borrow
     * of at least len bytes, and should always succeed.
     * TODO(dreiss): Is there any transport that could borrow but fail to
     * consume, or that would require a buffer to dump the consumed data?
     *
     * @param len  How many bytes to consume
     * @throws TTransportException If an error occurs
     */
    void consume(uint32_t len) {
      T_VIRTUAL_CALL();
      consume_virt(len);
    }
    virtual void consume_virt(uint32_t /* len */) {
      throw TTransportException(TTransportException::NOT_OPEN, "Base TTransport cannot consume.");
    }

    /**
     * Returns the origin of the transports call. The value depends on the
     * transport used. An IP based transport for example will return the
     * IP address of the client making the request.
     * If the transport doesn't know the origin Unknown is returned.
     *
     * The returned value can be used in a log message for example
     */
    virtual const std::string getOrigin() const { return "Unknown"; }

    std::shared_ptr<::apache::thrift::TConfiguration> getConfiguration() { return configuration_; }

    void setConfiguration(std::shared_ptr<::apache::thrift::TConfiguration> config) { 
      if (config != nullptr) configuration_ = config; 
    }

    /**
     * Updates RemainingMessageSize to reflect then known real message size (e.g. framed transport).
     * Will throw if we already consumed too many bytes or if the new size is larger than allowed.
     *
     * @param size  real message size
     */
    void updateKnownMessageSize(long int size)
    {
      long int consumed = knownMessageSize_ - remainingMessageSize_;
      resetConsumedMessageSize(size);
      countConsumedMessageBytes(consumed);
    }

    /**
     * Throws if there are not enough bytes in the input stream to satisfy a read of numBytes bytes of data
     *
     * @param numBytes  numBytes bytes of data
     */
    void checkReadBytesAvailable(long int numBytes)
    {
      if (remainingMessageSize_ < numBytes)
        throw TTransportException(TTransportException::END_OF_FILE, "MaxMessageSize reached");
    }

  protected:
    std::shared_ptr<::apache::thrift::TConfiguration> configuration_;
    long int remainingMessageSize_;
    long int knownMessageSize_;

    inline long int getRemainingMessageSize() { return remainingMessageSize_; }
    inline void setRemainingMessageSize(long int remainingMessageSize) { remainingMessageSize_ = remainingMessageSize; }
    inline int getMaxMessageSize() { return configuration_->getMaxMessageSize(); }
    inline long int getKnownMessageSize() { return knownMessageSize_; }
    void setKnownMessageSize(long int knownMessageSize) { knownMessageSize_ = knownMessageSize; }

    /**  
     * Resets RemainingMessageSize to the configured maximum
     * 
     *  @param newSize  configured size
     */
    void resetConsumedMessageSize(long newSize = -1)
    {
      // full reset 
      if (newSize < 0)
      {
          knownMessageSize_ = getMaxMessageSize();
          remainingMessageSize_ = getMaxMessageSize();
          return;
      }

      // update only: message size can shrink, but not grow
      if (newSize > knownMessageSize_)
          throw TTransportException(TTransportException::END_OF_FILE, "MaxMessageSize reached");

      knownMessageSize_ = newSize;
      remainingMessageSize_ = newSize;
    }

    /**
     * Consumes numBytes from the RemainingMessageSize.
     * 
     *  @param numBytes  Consumes numBytes
     */
    void countConsumedMessageBytes(long int numBytes)
    {
      if (remainingMessageSize_ >= numBytes)
      {
        remainingMessageSize_ -= numBytes;
      }
      else
      {
        remainingMessageSize_ = 0;
        throw TTransportException(TTransportException::END_OF_FILE, "MaxMessageSize reached");
      }
    }
  };

  /**
   * Generic factory class to make an input and output transport out of a
   * source transport. Commonly used inside servers to make input and output
   * streams out of raw clients.
   *
   */
  class TTransportFactory {
  public:
    TTransportFactory() = default;

    virtual ~TTransportFactory() = default;

    /**
     * Default implementation does nothing, just returns the transport given.
     */
    virtual std::shared_ptr<TTransport> getTransport(std::shared_ptr<TTransport> trans) {
      return trans;
    }
  };

  // ***TVirtualTransport.h***
  /**
   * Helper class that provides default implementations of TTransport methods.
   *
   * This class provides default implementations of read(), readAll(), write(),
   * borrow() and consume().
   *
   * In the TTransport base class, each of these methods simply invokes its
   * virtual counterpart.  This class overrides them to always perform the
   * default behavior, without a virtual function call.
   *
   * The primary purpose of this class is to serve as a base class for
   * TVirtualTransport, and prevent infinite recursion if one of its subclasses
   * does not override the TTransport implementation of these methods.  (Since
   * TVirtualTransport::read_virt() calls read(), and TTransport::read() calls
   * read_virt().)
   */
  class TTransportDefaults : public TTransport {
  public:
    /*
    * TTransport *_virt() methods provide reasonable default implementations.
    * Invoke them non-virtually.
    */
    uint32_t read(uint8_t* buf, uint32_t len) { return this->TTransport::read_virt(buf, len); }
    uint32_t readAll(uint8_t* buf, uint32_t len) { return this->TTransport::readAll_virt(buf, len); }
    void write(const uint8_t* buf, uint32_t len) { this->TTransport::write_virt(buf, len); }
    const uint8_t* borrow(uint8_t* buf, uint32_t* len) {
      return this->TTransport::borrow_virt(buf, len);
    }
    void consume(uint32_t len) { this->TTransport::consume_virt(len); }

  protected:
    TTransportDefaults(std::shared_ptr<::apache::thrift::TConfiguration> config = nullptr) : TTransport(config) {}
  };

  /**
   * Helper class to provide polymorphism for subclasses of TTransport.
   *
   * This class implements *_virt() methods of TTransport, to call the
   * non-virtual versions of these functions in the proper subclass.
   *
   * To define your own transport class using TVirtualTransport:
   * 1) Derive your subclass from TVirtualTransport<your class>
   *    e.g:  class MyTransport : public TVirtualTransport<MyTransport> {
   * 2) Provide your own implementations of read(), readAll(), etc.
   *    These methods should be non-virtual.
   *
   * Transport implementations that need to use virtual inheritance when
   * inheriting from TTransport cannot use TVirtualTransport.
   *
   * @author Chad Walters <chad@powerset.com>
   */
  template <class Transport_, class Super_ = TTransportDefaults>
  class TVirtualTransport : public Super_ {
  public:
    /*
    * Implementations of the *_virt() functions, to call the subclass's
    * non-virtual implementation function.
    */
    uint32_t read_virt(uint8_t* buf, uint32_t len) override { // links from TTransport.h:115
      return static_cast<Transport_*>(this)->read(buf, len); // links to TUDPSocket.cpp:516 
    }

    uint32_t readAll_virt(uint8_t* buf, uint32_t len) override { // links from TTransport.h:131
      return static_cast<Transport_*>(this)->readAll(buf, len); // links to TBufferTransports.h:270
    }

    void write_virt(const uint8_t* buf, uint32_t len) override {
      static_cast<Transport_*>(this)->write(buf, len);
    }

    const uint8_t* borrow_virt(uint8_t* buf, uint32_t* len) override {
      return static_cast<Transport_*>(this)->borrow(buf, len);
    }

    void consume_virt(uint32_t len) override { static_cast<Transport_*>(this)->consume(len); }

    /*
    * Provide a default readAll() implementation that invokes
    * read() non-virtually.
    *
    * Note: subclasses that use TVirtualTransport to derive from another
    * transport implementation (i.e., not TTransportDefaults) should beware that
    * this may override any non-default readAll() implementation provided by
    * the parent transport class.  They may need to redefine readAll() to call
    * the correct parent implementation, if desired.
    */
    uint32_t readAll(uint8_t* buf, uint32_t len) {
      auto* trans = static_cast<Transport_*>(this);
      return ::my_thrift::apache::thrift::transport::readAll(*trans, buf, len);
    }

  protected:
    TVirtualTransport() : Super_() {}

    /*
    * Templatized constructors, to allow arguments to be passed to the Super_
    * constructor.  Currently we only support 0, 1, or 2 arguments, but
    * additional versions can be added as needed.
    */
    template <typename Arg_>
    TVirtualTransport(Arg_ const& arg)
      : Super_(arg) {}

    template <typename Arg1_, typename Arg2_>
    TVirtualTransport(Arg1_ const& a1, Arg2_ const& a2)
      : Super_(a1, a2) {}
  };

  // ***TBufferTransports.h***
  /**
   * Base class for all transports that use read/write buffers for performance.
   *
   * TBufferBase is designed to implement the fast-path "memcpy" style
   * operations that work in the common case.  It does so with small and
   * (eventually) nonvirtual, inlinable methods.  TBufferBase is an abstract
   * class.  Subclasses are expected to define the "slow path" operations
   * that have to be done when the buffers are full or empty.
   *
   */
  class TBufferBase : public TVirtualTransport<TBufferBase> {

  public:
    /**
     * Fast-path read.
     *
     * When we have enough data buffered to fulfill the read, we can satisfy it
     * with a single memcpy, then adjust our internal pointers.  If the buffer
     * is empty, we call out to our slow path, implemented by a subclass.
     * This method is meant to eventually be nonvirtual and inlinable.
     */
    uint32_t read(uint8_t* buf, uint32_t len) { //links from TTransport.h:42
      checkReadBytesAvailable(len);
      uint8_t* new_rBase = rBase_ + len;
      if (TDB_LIKELY(new_rBase <= rBound_)) {
        std::memcpy(buf, rBase_, len);
        rBase_ = new_rBase;
        return len;
      }
      return readSlow(buf, len); //links to TBufferTransports.cpp:32
    }

    /**
     * Shortcutted version of readAll.
     */
    uint32_t readAll(uint8_t* buf, uint32_t len) { //links from TBufferTransports.h:270
      uint8_t* new_rBase = rBase_ + len;
      if (TDB_LIKELY(new_rBase <= rBound_)) {
        std::memcpy(buf, rBase_, len);
        rBase_ = new_rBase;
        return len;
      }
      return my_thrift::apache::thrift::transport::readAll(*this, buf, len); //links to TTransport.h:37  
    }

    /**
     * Fast-path write.
     *
     * When we have enough empty space in our buffer to accommodate the write, we
     * can satisfy it with a single memcpy, then adjust our internal pointers.
     * If the buffer is full, we call out to our slow path, implemented by a
     * subclass.  This method is meant to eventually be nonvirtual and
     * inlinable.
     */
    void write(const uint8_t* buf, uint32_t len) {
      uint8_t* new_wBase = wBase_ + len;
      if (TDB_LIKELY(new_wBase <= wBound_)) {
        std::memcpy(wBase_, buf, len);
        wBase_ = new_wBase;
        return;
      }
      writeSlow(buf, len);
    }

    /**
     * Fast-path borrow.  A lot like the fast-path read.
     */
    const uint8_t* borrow(uint8_t* buf, uint32_t* len) {
      if (TDB_LIKELY(static_cast<ptrdiff_t>(*len) <= rBound_ - rBase_)) {
        // With strict aliasing, writing to len shouldn't force us to
        // refetch rBase_ from memory.  TODO(dreiss): Verify this.
        *len = static_cast<uint32_t>(rBound_ - rBase_);
        return rBase_;
      }
      return borrowSlow(buf, len);
    }

    /**
     * Consume doesn't require a slow path.
     */
    void consume(uint32_t len) {
      countConsumedMessageBytes(len);
      if (TDB_LIKELY(static_cast<ptrdiff_t>(len) <= rBound_ - rBase_)) {
        rBase_ += len;
      } else {
        throw TTransportException(TTransportException::BAD_ARGS, "consume did not follow a borrow.");
      }
    }

  protected:
    /// Slow path read.
    virtual uint32_t readSlow(uint8_t* buf, uint32_t len) = 0;

    /// Slow path write.
    virtual void writeSlow(const uint8_t* buf, uint32_t len) = 0;

    /**
     * Slow path borrow.
     *
     * POSTCONDITION: return == nullptr || rBound_ - rBase_ >= *len
     */
    virtual const uint8_t* borrowSlow(uint8_t* buf, uint32_t* len) = 0;

    /**
     * Trivial constructor.
     *
     * Initialize pointers safely.  Constructing is not a very
     * performance-sensitive operation, so it is okay to just leave it to
     * the concrete class to set up pointers correctly.
     */
    TBufferBase(std::shared_ptr<::apache::thrift::TConfiguration> config = nullptr)
      : TVirtualTransport(config), rBase_(nullptr), rBound_(nullptr), wBase_(nullptr), wBound_(nullptr) {}

    /// Convenience mutator for setting the read buffer.
    void setReadBuffer(uint8_t* buf, uint32_t len) {
      rBase_ = buf;
      rBound_ = buf + len;
    }

    /// Convenience mutator for setting the write buffer.
    void setWriteBuffer(uint8_t* buf, uint32_t len) {
      wBase_ = buf;
      wBound_ = buf + len;
    }

    ~TBufferBase() override = default;

    /// Reads begin here.
    uint8_t* rBase_;
    /// Reads may extend to just before here.
    uint8_t* rBound_;

    /// Writes begin here.
    uint8_t* wBase_;
    /// Writes may extend to just before here.
    uint8_t* wBound_;
  };

  /**
   * Buffered transport. For reads it will read more data than is requested
   * and will serve future data out of a local buffer. For writes, data is
   * stored to an in memory buffer before being written out.
   *
   */
  class TBufferedTransport : public TVirtualTransport<TBufferedTransport, TBufferBase> {
  public:
    static const int DEFAULT_BUFFER_SIZE = 512;

    /// Use default buffer sizes.
    TBufferedTransport(std::shared_ptr<TTransport> transport, std::shared_ptr<::apache::thrift::TConfiguration> config = nullptr)
      : TVirtualTransport(config),
        transport_(transport),
        rBufSize_(DEFAULT_BUFFER_SIZE),
        wBufSize_(DEFAULT_BUFFER_SIZE),
        rBuf_(new uint8_t[rBufSize_]),
        wBuf_(new uint8_t[wBufSize_]) {
      initPointers();
    }

    /// Use specified buffer sizes.
    TBufferedTransport(std::shared_ptr<TTransport> transport, uint32_t sz, std::shared_ptr<::apache::thrift::TConfiguration> config = nullptr)
      : TVirtualTransport(config),
        transport_(transport),
        rBufSize_(sz),
        wBufSize_(sz),
        rBuf_(new uint8_t[rBufSize_]),
        wBuf_(new uint8_t[wBufSize_]) {
      initPointers();
    }

    /// Use specified read and write buffer sizes.
    TBufferedTransport(std::shared_ptr<TTransport> transport, uint32_t rsz, uint32_t wsz,
                      std::shared_ptr<::apache::thrift::TConfiguration> config = nullptr)
      : TVirtualTransport(config),
        transport_(transport),
        rBufSize_(rsz),
        wBufSize_(wsz),
        rBuf_(new uint8_t[rBufSize_]),
        wBuf_(new uint8_t[wBufSize_]) {
      initPointers();
    }

    void open() override { transport_->open(); }

    bool isOpen() const override { return transport_->isOpen(); }

    bool peek() override {
      if (rBase_ == rBound_) {
        setReadBuffer(rBuf_.get(), transport_->read(rBuf_.get(), rBufSize_));
      }
      return (rBound_ > rBase_);
    }

    void close() override {
      flush();
      transport_->close();
    }

    uint32_t readSlow(uint8_t* buf, uint32_t len) override;

    void writeSlow(const uint8_t* buf, uint32_t len) override;

    void flush() override;

    /**
     * Returns the origin of the underlying transport
     */
    const std::string getOrigin() const override { return transport_->getOrigin(); }

    /**
     * The following behavior is currently implemented by TBufferedTransport,
     * but that may change in a future version:
     * 1/ If len is at most rBufSize_, borrow will never return nullptr.
     *    Depending on the underlying transport, it could throw an exception
     *    or hang forever.
     * 2/ Some borrow requests may copy bytes internally.  However,
     *    if len is at most rBufSize_/2, none of the copied bytes
     *    will ever have to be copied again.  For optimial performance,
     *    stay under this limit.
     */
    const uint8_t* borrowSlow(uint8_t* buf, uint32_t* len) override;

    std::shared_ptr<TTransport> getUnderlyingTransport() { return transport_; }

    /*
    * TVirtualTransport provides a default implementation of readAll().
    * We want to use the TBufferBase version instead.
    */
    uint32_t readAll(uint8_t* buf, uint32_t len) { return TBufferBase::readAll(buf, len); } // links to TBufferTransports.h:78

    uint32_t readEnd() override {
      resetConsumedMessageSize();
      return 0;
    }

  protected:
    void initPointers() {
      setReadBuffer(rBuf_.get(), 0);
      setWriteBuffer(wBuf_.get(), wBufSize_);
      // Write size never changes.
    }

    std::shared_ptr<TTransport> transport_;

    uint32_t rBufSize_;
    uint32_t wBufSize_;
    std::unique_ptr<uint8_t[]> rBuf_;
    std::unique_ptr<uint8_t[]> wBuf_;
  };

  /**
   * Wraps a transport into a buffered one.
   *
   */
  class TBufferedTransportFactory : public TTransportFactory {
  public:
    TBufferedTransportFactory() = default;

    ~TBufferedTransportFactory() override = default;

    /**
     * Wraps the transport into a buffered one.
     */
    std::shared_ptr<TTransport> getTransport(std::shared_ptr<TTransport> trans) override {
      return std::shared_ptr<TTransport>(new TBufferedTransport(trans));
    }
  };

  /**
   * Framed transport. All writes go into an in-memory buffer until flush is
   * called, at which point the transport writes the length of the entire
   * binary chunk followed by the data payload. This allows the receiver on the
   * other end to always do fixed-length reads.
   *
   */
  class TFramedTransport : public TVirtualTransport<TFramedTransport, TBufferBase> {
  public:
    static const int DEFAULT_BUFFER_SIZE = 512;
    static const int DEFAULT_MAX_FRAME_SIZE = 256 * 1024 * 1024;

    /// Use default buffer sizes.
    TFramedTransport(std::shared_ptr<::apache::thrift::TConfiguration> config = nullptr)
      : TVirtualTransport(config),
        transport_(),
        rBufSize_(0),
        wBufSize_(DEFAULT_BUFFER_SIZE),
        rBuf_(),
        wBuf_(new uint8_t[wBufSize_]),
        bufReclaimThresh_((std::numeric_limits<uint32_t>::max)()) {
      initPointers();
    }

    TFramedTransport(std::shared_ptr<TTransport> transport, std::shared_ptr<::apache::thrift::TConfiguration> config = nullptr)
      : TVirtualTransport(config),
        transport_(transport),
        rBufSize_(0),
        wBufSize_(DEFAULT_BUFFER_SIZE),
        rBuf_(),
        wBuf_(new uint8_t[wBufSize_]),
        bufReclaimThresh_((std::numeric_limits<uint32_t>::max)()),
        maxFrameSize_(configuration_->getMaxFrameSize()) {
      initPointers();
    }

    TFramedTransport(std::shared_ptr<TTransport> transport,
                    uint32_t sz,
                    uint32_t bufReclaimThresh = (std::numeric_limits<uint32_t>::max)(),
                    std::shared_ptr<::apache::thrift::TConfiguration> config = nullptr)
      : TVirtualTransport(config),
        transport_(transport),
        rBufSize_(0),
        wBufSize_(sz),
        rBuf_(),
        wBuf_(new uint8_t[wBufSize_]),
        bufReclaimThresh_(bufReclaimThresh),
        maxFrameSize_(configuration_->getMaxFrameSize()) {
      initPointers();
    }

    void open() override { transport_->open(); }

    bool isOpen() const override { return transport_->isOpen(); }

    bool peek() override { return (rBase_ < rBound_) || transport_->peek(); }

    void close() override {
      flush();
      transport_->close();
    }

    uint32_t readSlow(uint8_t* buf, uint32_t len) override;

    void writeSlow(const uint8_t* buf, uint32_t len) override;

    void flush() override;

    uint32_t readEnd() override;

    uint32_t writeEnd() override;

    const uint8_t* borrowSlow(uint8_t* buf, uint32_t* len) override;

    std::shared_ptr<TTransport> getUnderlyingTransport() { return transport_; }

    /*
    * TVirtualTransport provides a default implementation of readAll().
    * We want to use the TBufferBase version instead.
    */
    using TBufferBase::readAll;

    /**
     * Returns the origin of the underlying transport
     */
    const std::string getOrigin() const override { return transport_->getOrigin(); }

    /**
     * Set the maximum size of the frame at read
     */
    void setMaxFrameSize(uint32_t maxFrameSize) { maxFrameSize_ = maxFrameSize; }

    /**
     * Get the maximum size of the frame at read
     */
    uint32_t getMaxFrameSize() { return maxFrameSize_; }

  protected:
    /**
     * Reads a frame of input from the underlying stream.
     *
     * Returns true if a frame was read successfully, or false on EOF.
     * (Raises a TTransportException if EOF occurs after a partial frame.)
     */
    virtual bool readFrame();

    void initPointers() {
      setReadBuffer(nullptr, 0);
      setWriteBuffer(wBuf_.get(), wBufSize_);

      // Pad the buffer so we can insert the size later.
      int32_t pad = 0;
      this->write((uint8_t*)&pad, sizeof(pad));
    }

    std::shared_ptr<TTransport> transport_;

    uint32_t rBufSize_;
    uint32_t wBufSize_;
    std::unique_ptr<uint8_t[]> rBuf_;
    std::unique_ptr<uint8_t[]> wBuf_;
    uint32_t bufReclaimThresh_;
    uint32_t maxFrameSize_;
  };

  /**
   * Wraps a transport into a framed one.
   *
   */
  class TFramedTransportFactory : public TTransportFactory {
  public:
    TFramedTransportFactory() = default;

    ~TFramedTransportFactory() override = default;

    /**
     * Wraps the transport into a framed one.
     */
    std::shared_ptr<TTransport> getTransport(std::shared_ptr<TTransport> trans) override {
      return std::shared_ptr<TTransport>(new TFramedTransport(trans));
    }
  };

  /**
   * A memory buffer is a tranpsort that simply reads from and writes to an
   * in memory buffer. Anytime you call write on it, the data is simply placed
   * into a buffer, and anytime you call read, data is read from that buffer.
   *
   * The buffers are allocated using C constructs malloc,realloc, and the size
   * doubles as necessary.  We've considered using scoped
   *
   */
  class TMemoryBuffer : public TVirtualTransport<TMemoryBuffer, TBufferBase> {
  private:
    // Common initialization done by all constructors.
    void initCommon(uint8_t* buf, uint32_t size, bool owner, uint32_t wPos) {

      maxBufferSize_ = (std::numeric_limits<uint32_t>::max)();

      if (buf == nullptr && size != 0) {
        assert(owner);
        buf = (uint8_t*)std::malloc(size);
        if (buf == nullptr) {
    throw std::bad_alloc();
        }
      }

      buffer_ = buf;
      bufferSize_ = size;

      rBase_ = buffer_;
      rBound_ = buffer_ + wPos;
      // TODO(dreiss): Investigate NULL-ing this if !owner.
      wBase_ = buffer_ + wPos;
      wBound_ = buffer_ + bufferSize_;

      owner_ = owner;

      // rBound_ is really an artifact.  In principle, it should always be
      // equal to wBase_.  We update it in a few places (computeRead, etc.).
    }

  public:
    static const uint32_t defaultSize = 1024;

    /**
     * This enum specifies how a TMemoryBuffer should treat
     * memory passed to it via constructors or resetBuffer.
     *
     * OBSERVE:
     *   TMemoryBuffer will simply store a pointer to the memory.
     *   It is the callers responsibility to ensure that the pointer
     *   remains valid for the lifetime of the TMemoryBuffer,
     *   and that it is properly cleaned up.
     *   Note that no data can be written to observed buffers.
     *
     * COPY:
     *   TMemoryBuffer will make an internal copy of the buffer.
     *   The caller has no responsibilities.
     *
     * TAKE_OWNERSHIP:
     *   TMemoryBuffer will become the "owner" of the buffer,
     *   and will be responsible for freeing it.
     *   The memory must have been allocated with malloc.
     */
    enum MemoryPolicy { OBSERVE = 1, COPY = 2, TAKE_OWNERSHIP = 3 };

    /**
     * Construct a TMemoryBuffer with a default-sized buffer,
     * owned by the TMemoryBuffer object.
     */
    TMemoryBuffer(std::shared_ptr<::apache::thrift::TConfiguration> config = nullptr)
      : TVirtualTransport(config) {
      initCommon(nullptr, defaultSize, true, 0);
    }

    /**
     * Construct a TMemoryBuffer with a buffer of a specified size,
     * owned by the TMemoryBuffer object.
     *
     * @param sz  The initial size of the buffer.
     */
    TMemoryBuffer(uint32_t sz, std::shared_ptr<::apache::thrift::TConfiguration> config = nullptr)
      : TVirtualTransport(config) {
      initCommon(nullptr, sz, true, 0);
    }

    /**
     * Construct a TMemoryBuffer with buf as its initial contents.
     *
     * @param buf    The initial contents of the buffer.
     *               Note that, while buf is a non-const pointer,
     *               TMemoryBuffer will not write to it if policy == OBSERVE,
     *               so it is safe to const_cast<uint8_t*>(whatever).
     * @param sz     The size of @c buf.
     * @param policy See @link MemoryPolicy @endlink .
     */
    TMemoryBuffer(uint8_t* buf, uint32_t sz, MemoryPolicy policy = OBSERVE, std::shared_ptr<::apache::thrift::TConfiguration> config = nullptr)
      : TVirtualTransport(config) {
      if (buf == nullptr && sz != 0) {
        throw TTransportException(TTransportException::BAD_ARGS,
                                  "TMemoryBuffer given null buffer with non-zero size.");
      }

      switch (policy) {
      case OBSERVE:
      case TAKE_OWNERSHIP:
        initCommon(buf, sz, policy == TAKE_OWNERSHIP, sz);
        break;
      case COPY:
        initCommon(nullptr, sz, true, 0);
        this->write(buf, sz);
        break;
      default:
        throw TTransportException(TTransportException::BAD_ARGS,
                                  "Invalid MemoryPolicy for TMemoryBuffer");
      }
    }

    ~TMemoryBuffer() override {
      if (owner_) {
        std::free(buffer_);
      }
    }

    bool isOpen() const override { return true; }

    bool peek() override { return (rBase_ < wBase_); }

    void open() override {}

    void close() override {}

    // TODO(dreiss): Make bufPtr const.
    void getBuffer(uint8_t** bufPtr, uint32_t* sz) {
      *bufPtr = rBase_;
      *sz = static_cast<uint32_t>(wBase_ - rBase_);
    }

    std::string getBufferAsString() {
      if (buffer_ == nullptr) {
        return "";
      }
      uint8_t* buf;
      uint32_t sz;
      getBuffer(&buf, &sz);
      return std::string((char*)buf, (std::string::size_type)sz);
    }

    void appendBufferToString(std::string& str) {
      if (buffer_ == nullptr) {
        return;
      }
      uint8_t* buf;
      uint32_t sz;
      getBuffer(&buf, &sz);
      str.append((char*)buf, sz);
    }

    void resetBuffer() {
      rBase_ = buffer_;
      rBound_ = buffer_;
      wBase_ = buffer_;
      // It isn't safe to write into a buffer we don't own.
      if (!owner_) {
        wBound_ = wBase_;
        bufferSize_ = 0;
      }
    }

    /// See constructor documentation.
    void resetBuffer(uint8_t* buf, uint32_t sz, MemoryPolicy policy = OBSERVE) {
      // Use a variant of the copy-and-swap trick for assignment operators.
      // This is sub-optimal in terms of performance for two reasons:
      //   1/ The constructing and swapping of the (small) values
      //      in the temporary object takes some time, and is not necessary.
      //   2/ If policy == COPY, we allocate the new buffer before
      //      freeing the old one, precluding the possibility of
      //      reusing that memory.
      // I doubt that either of these problems could be optimized away,
      // but the second is probably no a common case, and the first is minor.
      // I don't expect resetBuffer to be a common operation, so I'm willing to
      // bite the performance bullet to make the method this simple.

      // Construct the new buffer.
      TMemoryBuffer new_buffer(buf, sz, policy);
      // Move it into ourself.
      this->swap(new_buffer);
      // Our old self gets destroyed.
    }

    /// See constructor documentation.
    void resetBuffer(uint32_t sz) {
      // Construct the new buffer.
      TMemoryBuffer new_buffer(sz);
      // Move it into ourself.
      this->swap(new_buffer);
      // Our old self gets destroyed.
    }

    std::string readAsString(uint32_t len) {
      std::string str;
      (void)readAppendToString(str, len);
      return str;
    }

    uint32_t readAppendToString(std::string& str, uint32_t len);

    // return number of bytes read
    uint32_t readEnd() override {
      // This cast should be safe, because buffer_'s size is a uint32_t
      auto bytes = static_cast<uint32_t>(rBase_ - buffer_);
      if (rBase_ == wBase_) {
        resetBuffer();
      }
      resetConsumedMessageSize();
      return bytes;
    }

    // Return number of bytes written
    uint32_t writeEnd() override {
      // This cast should be safe, because buffer_'s size is a uint32_t
      return static_cast<uint32_t>(wBase_ - buffer_);
    }

    uint32_t available_read() const {
      // Remember, wBase_ is the real rBound_.
      return static_cast<uint32_t>(wBase_ - rBase_);
    }

    uint32_t available_write() const { return static_cast<uint32_t>(wBound_ - wBase_); }

    // Returns a pointer to where the client can write data to append to
    // the TMemoryBuffer, and ensures the buffer is big enough to accommodate a
    // write of the provided length.  The returned pointer is very convenient for
    // passing to read(), recv(), or similar. You must call wroteBytes() as soon
    // as data is written or the buffer will not be aware that data has changed.
    uint8_t* getWritePtr(uint32_t len) {
      ensureCanWrite(len);
      return wBase_;
    }

    // Informs the buffer that the client has written 'len' bytes into storage
    // that had been provided by getWritePtr().
    void wroteBytes(uint32_t len);

    /*
    * TVirtualTransport provides a default implementation of readAll().
    * We want to use the TBufferBase version instead.
    */
    uint32_t readAll(uint8_t* buf, uint32_t len) { return TBufferBase::readAll(buf, len); }

    //! \brief Get the current buffer size
    //! \returns the current buffer size
    uint32_t getBufferSize() const {
      return bufferSize_;
    }

    //! \brief Get the current maximum buffer size
    //! \returns the current maximum buffer size
    uint32_t getMaxBufferSize() const {
      return maxBufferSize_;
    }

    //! \brief Change the maximum buffer size
    //! \param[in]  maxSize  the new maximum buffer size allowed to grow to
    //! \throws  TTransportException(BAD_ARGS) if maxSize is less than the current buffer size
    void setMaxBufferSize(uint32_t maxSize) {
      if (maxSize < bufferSize_) {
        throw TTransportException(TTransportException::BAD_ARGS,
                                  "Maximum buffer size would be less than current buffer size");
      }
      maxBufferSize_ = maxSize;
    }

  protected:
    void swap(TMemoryBuffer& that) {
      using std::swap;
      swap(buffer_, that.buffer_);
      swap(bufferSize_, that.bufferSize_);

      swap(rBase_, that.rBase_);
      swap(rBound_, that.rBound_);
      swap(wBase_, that.wBase_);
      swap(wBound_, that.wBound_);

      swap(owner_, that.owner_);
    }

    // Make sure there's at least 'len' bytes available for writing.
    void ensureCanWrite(uint32_t len);

    // Compute the position and available data for reading.
    void computeRead(uint32_t len, uint8_t** out_start, uint32_t* out_give);

    uint32_t readSlow(uint8_t* buf, uint32_t len) override;

    void writeSlow(const uint8_t* buf, uint32_t len) override;

    const uint8_t* borrowSlow(uint8_t* buf, uint32_t* len) override;

    // Data buffer
    uint8_t* buffer_;

    // Allocated buffer size
    uint32_t bufferSize_;

    // Maximum allowed size
    uint32_t maxBufferSize_;

    // Is this object the owner of the buffer?
    bool owner_;

    // Don't forget to update constrctors, initCommon, and swap if
    // you add new members.
  };

  // ***TBufferTransport.cpp***
  uint32_t TBufferedTransport::readSlow(uint8_t* buf, uint32_t len) { //links from TBufferTransports.h:71
    auto have = static_cast<uint32_t>(rBound_ - rBase_);

    // We should only take the slow path if we can't satisfy the read
    // with the data already in the buffer.
    assert(have < len);

    // If we have some data in the buffer, copy it out and return it.
    // We have to return it without attempting to read more, since we aren't
    // guaranteed that the underlying transport actually has more data, so
    // attempting to read from it could block.
    if (have > 0) {
      memcpy(buf, rBase_, have);
      setReadBuffer(rBuf_.get(), 0);
      return have;
    }

    // No data is available in our buffer.
    // Get more from underlying transport up to buffer size.
    // Note that this makes a lot of sense if len < rBufSize_
    // and almost no sense otherwise.  TODO(dreiss): Fix that
    // case (possibly including some readv hotness).
    setReadBuffer(rBuf_.get(), transport_->read(rBuf_.get(), rBufSize_)); //links to TTransport.h:115  

    // Hand over whatever we have.
    uint32_t give = (std::min)(len, static_cast<uint32_t>(rBound_ - rBase_));
    memcpy(buf, rBase_, give);
    rBase_ += give;

    return give;
  }

  void TBufferedTransport::writeSlow(const uint8_t* buf, uint32_t len) {
    auto have_bytes = static_cast<uint32_t>(wBase_ - wBuf_.get());
    auto space = static_cast<uint32_t>(wBound_ - wBase_);
    // We should only take the slow path if we can't accommodate the write
    // with the free space already in the buffer.
    assert(wBound_ - wBase_ < static_cast<ptrdiff_t>(len));

    // Now here's the tricky question: should we copy data from buf into our
    // internal buffer and write it from there, or should we just write out
    // the current internal buffer in one syscall and write out buf in another.
    // If our currently buffered data plus buf is at least double our buffer
    // size, we will have to do two syscalls no matter what (except in the
    // degenerate case when our buffer is empty), so there is no use copying.
    // Otherwise, there is sort of a sliding scale.  If we have N-1 bytes
    // buffered and need to write 2, it would be crazy to do two syscalls.
    // On the other hand, if we have 2 bytes buffered and are writing 2N-3,
    // we can save a syscall in the short term by loading up our buffer, writing
    // it out, and copying the rest of the bytes into our buffer.  Of course,
    // if we get another 2-byte write, we haven't saved any syscalls at all,
    // and have just copied nearly 2N bytes for nothing.  Finding a perfect
    // policy would require predicting the size of future writes, so we're just
    // going to always eschew syscalls if we have less than 2N bytes to write.

    // The case where we have to do two syscalls.
    // This case also covers the case where the buffer is empty,
    // but it is clearer (I think) to think of it as two separate cases.
    if ((have_bytes + len >= 2 * wBufSize_) || (have_bytes == 0)) {
      // TODO(dreiss): writev
      if (have_bytes > 0) {
        transport_->write(wBuf_.get(), have_bytes);
      }
      transport_->write(buf, len);
      wBase_ = wBuf_.get();
      return;
    }

    // Fill up our internal buffer for a write.
    memcpy(wBase_, buf, space);
    buf += space;
    len -= space;
    transport_->write(wBuf_.get(), wBufSize_);

    // Copy the rest into our buffer.
    assert(len < wBufSize_);
    memcpy(wBuf_.get(), buf, len);
    wBase_ = wBuf_.get() + len;
    return;
  }

  const uint8_t* TBufferedTransport::borrowSlow(uint8_t* buf, uint32_t* len) {
    (void)buf;
    (void)len;
    // Simply return nullptr.  We don't know if there is actually data available on
    // the underlying transport, so calling read() might block.
    return nullptr;
  }

  void TBufferedTransport::flush() { // links from Calculator.cpp:896 
    resetConsumedMessageSize();
    // Write out any data waiting in the write buffer.
    auto have_bytes = static_cast<uint32_t>(wBase_ - wBuf_.get());
    if (have_bytes > 0) {
      // Note that we reset wBase_ prior to the underlying write
      // to ensure we're in a sane state (i.e. internal buffer cleaned)
      // if the underlying write throws up an exception
      wBase_ = wBuf_.get();
      transport_->write(wBuf_.get(), have_bytes);
    }

    // Flush the underlying transport.
    transport_->flush();
  }

  uint32_t TFramedTransport::readSlow(uint8_t* buf, uint32_t len) {
    uint32_t want = len;
    auto have = static_cast<uint32_t>(rBound_ - rBase_);

    // We should only take the slow path if we can't satisfy the read
    // with the data already in the buffer.
    assert(have < want);

    // If we have some data in the buffer, copy it out and return it.
    // We have to return it without attempting to read more, since we aren't
    // guaranteed that the underlying transport actually has more data, so
    // attempting to read from it could block.
    if (have > 0) {
      memcpy(buf, rBase_, have);
      setReadBuffer(rBuf_.get(), 0);
      return have;
    }

    // Read another frame.
    if (!readFrame()) {
      // EOF.  No frame available.
      return 0;
    }

    // TODO(dreiss): Should we warn when reads cross frames?

    // Hand over whatever we have.
    uint32_t give = (std::min)(want, static_cast<uint32_t>(rBound_ - rBase_));
    memcpy(buf, rBase_, give);
    rBase_ += give;
    want -= give;

    return (len - want);
  }

  bool TFramedTransport::readFrame() {
    // TODO(dreiss): Think about using readv here, even though it would
    // result in (gasp) read-ahead.

    // Read the size of the next frame.
    // We can't use readAll(&sz, sizeof(sz)), since that always throws an
    // exception on EOF.  We want to throw an exception only if EOF occurs after
    // partial size data.
    int32_t sz = -1;
    uint32_t size_bytes_read = 0;
    while (size_bytes_read < sizeof(sz)) {
      uint8_t* szp = reinterpret_cast<uint8_t*>(&sz) + size_bytes_read;
      uint32_t bytes_read
          = transport_->read(szp, static_cast<uint32_t>(sizeof(sz)) - size_bytes_read);
      if (bytes_read == 0) {
        if (size_bytes_read == 0) {
          // EOF before any data was read.
          return false;
        } else {
          // EOF after a partial frame header.  Raise an exception.
          throw TTransportException(TTransportException::END_OF_FILE,
                                    "No more data to read after "
                                    "partial frame header.");
        }
      }
      size_bytes_read += bytes_read;
    }

    sz = ntohl(sz);

    if (sz < 0) {
      throw TTransportException("Frame size has negative value");
    }

    // Check for oversized frame
    if (sz > static_cast<int32_t>(maxFrameSize_))
      throw TTransportException(TTransportException::CORRUPTED_DATA, "Received an oversized frame");

    // Read the frame payload, and reset markers.
    if (sz > static_cast<int32_t>(rBufSize_)) {
      rBuf_.reset(new uint8_t[sz]);
      rBufSize_ = sz;
    }
    transport_->readAll(rBuf_.get(), sz);
    setReadBuffer(rBuf_.get(), sz);
    return true;
  }

  void TFramedTransport::writeSlow(const uint8_t* buf, uint32_t len) {
    // Double buffer size until sufficient.
    auto have = static_cast<uint32_t>(wBase_ - wBuf_.get());
    uint32_t new_size = wBufSize_;
    if (len + have < have /* overflow */ || len + have > 0x7fffffff) {
      throw TTransportException(TTransportException::BAD_ARGS,
                                "Attempted to write over 2 GB to TFramedTransport.");
    }
    while (new_size < len + have) {
      new_size = new_size > 0 ? new_size * 2 : 1;
    }

    // TODO(dreiss): Consider modifying this class to use malloc/free
    // so we can use realloc here.

    // Allocate new buffer.
    auto* new_buf = new uint8_t[new_size];

    // Copy the old buffer to the new one.
    memcpy(new_buf, wBuf_.get(), have);

    // Now point buf to the new one.
    wBuf_.reset(new_buf);
    wBufSize_ = new_size;
    wBase_ = wBuf_.get() + have;
    wBound_ = wBuf_.get() + wBufSize_;

    // Copy the data into the new buffer.
    memcpy(wBase_, buf, len);
    wBase_ += len;
  }

  void TFramedTransport::flush() {
    resetConsumedMessageSize();
    int32_t sz_hbo, sz_nbo;
    assert(wBufSize_ > sizeof(sz_nbo));

    // Slip the frame size into the start of the buffer.
    sz_hbo = static_cast<uint32_t>(wBase_ - (wBuf_.get() + sizeof(sz_nbo)));
    sz_nbo = (int32_t)htonl((uint32_t)(sz_hbo));
    memcpy(wBuf_.get(), (uint8_t*)&sz_nbo, sizeof(sz_nbo));

    if (sz_hbo > 0) {
      // Note that we reset wBase_ (with a pad for the frame size)
      // prior to the underlying write to ensure we're in a sane state
      // (i.e. internal buffer cleaned) if the underlying write throws
      // up an exception
      wBase_ = wBuf_.get() + sizeof(sz_nbo);

      // Write size and frame body.
      transport_->write(wBuf_.get(), static_cast<uint32_t>(sizeof(sz_nbo)) + sz_hbo);
    }

    // Flush the underlying transport.
    transport_->flush();

    // reclaim write buffer
    if (wBufSize_ > bufReclaimThresh_) {
      wBufSize_ = DEFAULT_BUFFER_SIZE;
      wBuf_.reset(new uint8_t[wBufSize_]);
      setWriteBuffer(wBuf_.get(), wBufSize_);

      // reset wBase_ with a pad for the frame size
      int32_t pad = 0;
      wBase_ = wBuf_.get() + sizeof(pad);
    }
  }

  uint32_t TFramedTransport::writeEnd() {
    return static_cast<uint32_t>(wBase_ - wBuf_.get());
  }

  const uint8_t* TFramedTransport::borrowSlow(uint8_t* buf, uint32_t* len) {
    (void)buf;
    (void)len;
    // Don't try to be clever with shifting buffers.
    // If the fast path failed let the protocol use its slow path.
    // Besides, who is going to try to borrow across messages?
    return nullptr;
  }

  uint32_t TFramedTransport::readEnd() {
    // include framing bytes
    auto bytes_read = static_cast<uint32_t>(rBound_ - rBuf_.get() + sizeof(uint32_t));

    if (rBufSize_ > bufReclaimThresh_) {
      rBufSize_ = 0;
      rBuf_.reset();
      setReadBuffer(rBuf_.get(), rBufSize_);
    }

    return bytes_read;
  }

  void TMemoryBuffer::computeRead(uint32_t len, uint8_t** out_start, uint32_t* out_give) {
    // Correct rBound_ so we can use the fast path in the future.
    rBound_ = wBase_;

    // Decide how much to give.
    uint32_t give = (std::min)(len, available_read());

    *out_start = rBase_;
    *out_give = give;

    // Preincrement rBase_ so the caller doesn't have to.
    rBase_ += give;
  }

  uint32_t TMemoryBuffer::readSlow(uint8_t* buf, uint32_t len) {
    uint8_t* start;
    uint32_t give;
    computeRead(len, &start, &give);

    // Copy into the provided buffer.
    memcpy(buf, start, give);

    return give;
  }

  uint32_t TMemoryBuffer::readAppendToString(std::string& str, uint32_t len) {
    // Don't get some stupid assertion failure.
    if (buffer_ == nullptr) {
      return 0;
    }

    uint8_t* start;
    uint32_t give;
    computeRead(len, &start, &give);

    // Append to the provided string.
    str.append((char*)start, give);

    return give;
  }

  void TMemoryBuffer::ensureCanWrite(uint32_t len) {
    // Check available space
    uint32_t avail = available_write();
    if (len <= avail) {
      return;
    }

    if (!owner_) {
      throw TTransportException("Insufficient space in external MemoryBuffer");
    }

    // Grow the buffer as necessary. Use uint64_t to avoid overflow.
    const uint64_t current_used = bufferSize_ - avail;
    const uint64_t required_buffer_size = len + current_used;
    if (required_buffer_size > maxBufferSize_) {
      throw TTransportException(TTransportException::BAD_ARGS,
                                "Internal buffer size overflow when requesting a buffer of size " + std::to_string(required_buffer_size));
    }

    // Always grow to the next bigger power of two:
    const double suggested_buffer_size = std::exp2(std::ceil(std::log2(required_buffer_size)));
    // Unless the power of two exceeds maxBufferSize_:
    const uint64_t new_size = static_cast<uint64_t>((std::min)(suggested_buffer_size, static_cast<double>(maxBufferSize_)));

    // Allocate into a new pointer so we don't bork ours if it fails.
    auto* new_buffer = static_cast<uint8_t*>(std::realloc(buffer_, static_cast<std::size_t>(new_size)));
    if (new_buffer == nullptr) {
      throw std::bad_alloc();
    }

    rBase_ = new_buffer + (rBase_ - buffer_);
    rBound_ = new_buffer + (rBound_ - buffer_);
    wBase_ = new_buffer + (wBase_ - buffer_);
    wBound_ = new_buffer + new_size;
    // Note: with realloc() we do not need to free the previous buffer:
    buffer_ = new_buffer;
    bufferSize_ = static_cast<uint32_t>(new_size);
  }

  void TMemoryBuffer::writeSlow(const uint8_t* buf, uint32_t len) {
    ensureCanWrite(len);

    // Copy into the buffer and increment wBase_.
    memcpy(wBase_, buf, len);
    wBase_ += len;
  }

  void TMemoryBuffer::wroteBytes(uint32_t len) {
    uint32_t avail = available_write();
    if (len > avail) {
      throw TTransportException("Client wrote more bytes than size of buffer.");
    }
    wBase_ += len;
  }

  const uint8_t* TMemoryBuffer::borrowSlow(uint8_t* buf, uint32_t* len) {
    (void)buf;
    rBound_ = wBase_;
    if (available_read() >= *len) {
      *len = available_read();
      return rBase_;
    }
    return nullptr;
  }

  // ***TServerTransport.h***
  /**
   * Server transport framework. A server needs to have some facility for
   * creating base transports to read/write from.  The server is expected
   * to keep track of TTransport children that it creates for purposes of
   * controlling their lifetime.
   */
  class TServerTransport {
  public:
    virtual ~TServerTransport() = default;

    /**
     * Whether this transport is open.
     */
    virtual bool isOpen() const { return false; }

    /**
     * Starts the server transport listening for new connections. Prior to this
     * call most transports will not return anything when accept is called.
     *
     * @throws TTransportException if we were unable to listen
     */
    virtual void listen() {}

    /**
     * Gets a new dynamically allocated transport object and passes it to the
     * caller. Note that it is the explicit duty of the caller to free the
     * allocated object. The returned TTransport object must always be in the
     * opened state. nullptr should never be returned, instead an Exception should
     * always be thrown.
     *
     * @return A new TTransport object
     * @throws TTransportException if there is an error
     */
    std::shared_ptr<TTransport> accept() {
      std::shared_ptr<TTransport> result = acceptImpl();
      if (!result) {
        throw TTransportException("accept() may not return nullptr");
      }
      return result;
    }

    /**
     * For "smart" TServerTransport implementations that work in a multi
     * threaded context this can be used to break out of an accept() call.
     * It is expected that the transport will throw a TTransportException
     * with the INTERRUPTED error code.
     *
     * This will not make an attempt to interrupt any TTransport children.
     */
    virtual void interrupt() {}

    /**
     * This will interrupt the children created by the server transport.
     * allowing them to break out of any blocking data reception call.
     * It is expected that the children will throw a TTransportException
     * with the INTERRUPTED error code.
     */
    virtual void interruptChildren() {}

    /**
    * Utility method
    *
    * @return server socket file descriptor
    * @throw TTransportException If an error occurs
    */

    virtual THRIFT_SOCKET getSocketFD() { return -1; }

    /**
     * Closes this transport such that future calls to accept will do nothing.
     */
    virtual void close() = 0;

  protected:
    TServerTransport() = default;

    /**
     * Subclasses should implement this function for accept.
     *
     * @return A newly allocated TTransport object
     * @throw TTransportException If an error occurs
     */
    virtual std::shared_ptr<TTransport> acceptImpl() = 0;
  };

  // ***TUDPSocket.h***
  class TUDPSocket : public TVirtualTransport<TUDPSocket> {
  public:
    static const uint16_t RX_RING_SIZE = 4096;
    static const uint16_t TX_RING_SIZE = 4096;
    static const uint16_t NUM_MBUFS = 8191;
    static const uint16_t MBUF_CACHE_SIZE = 512;
    static const uint16_t BURST_SIZE = 128;

    TUDPSocket(std::shared_ptr<::apache::thrift::TConfiguration> config = nullptr);
    TUDPSocket(const std::string& host, int port, std::shared_ptr<::apache::thrift::TConfiguration> config = nullptr);
    TUDPSocket(std::shared_ptr<DPDKResources> dpdkResources, 
                std::shared_ptr<::apache::thrift::TConfiguration> config = nullptr);
    ~TUDPSocket() override;

    bool isOpen() const override;
    bool peek() override;
    void open() override;
    void close() override;
    
    uint32_t read(uint8_t* buf, uint32_t len);
    void write(const uint8_t* buf, uint32_t len);
    uint32_t write_partial(const uint8_t* buf, uint32_t len);

    void setRecvTimeout(int ms);
    void setSendTimeout(int ms);
    
    std::string getHost() const { return host_; }
    int getPort() const { return port_; }
    const std::string getOrigin() const override;
    uint32_t GetLocalIPAddress(uint16_t port_id);
    void setLocalIpAddress(const std::string& ipStr);

  protected:
    bool initDPDK();
    bool setupDPDKPort();
    struct rte_mempool* createMempool();
    
  private:
    void handleReceivedData(uint8_t* data, uint32_t len);
    
    std::vector<uint8_t> rxBuffer_;
    std::mutex mutex_;
    std::condition_variable cv_;
    
    std::string host_;
    int port_;
    bool isInitialized_;
    uint32_t localIpAddress_;
    
    // DPDK specific members
    std::shared_ptr<DPDKResources> dpdkResources_;
    struct sockaddr_in peerAddr_;  // Keep this for UDP addressing
    void handleArpPacket(struct rte_mbuf* m);
    
    int sendTimeout_;
    int recvTimeout_;
  };
  // ***TUDPSocket.cpp***
  // Constants for header sizes
  static const size_t ETHER_HDR_LEN = sizeof(struct rte_ether_hdr);
  static const size_t IP_HDR_LEN = sizeof(struct rte_ipv4_hdr);
  static const size_t UDP_HDR_LEN = sizeof(struct rte_udp_hdr);
  static const size_t HEADERS_LEN = ETHER_HDR_LEN + IP_HDR_LEN + UDP_HDR_LEN;



  TUDPSocket::TUDPSocket(std::shared_ptr<::apache::thrift::TConfiguration> config)
    : TVirtualTransport(config)
    , port_(0)
    , isInitialized_(false)
    , localIpAddress_(0)
    , sendTimeout_(0)
    , recvTimeout_(0) {
        // Get DPDK handler instance
        auto& dpdk = DPDKHandler::getInstance();

        // Set callback for received packets
        dpdk.setPacketCallback([this](uint8_t* data, uint32_t len) {
            handleReceivedData(data, len);
        });
  }

  TUDPSocket::TUDPSocket(const std::string& host, int port, std::shared_ptr<::apache::thrift::TConfiguration> config)
    : TVirtualTransport(config)
    , host_(host)
    , port_(port)
    , isInitialized_(false)
    , localIpAddress_(0)
    , sendTimeout_(0)
    , recvTimeout_(0) {
  }

  TUDPSocket::TUDPSocket(std::shared_ptr<DPDKResources> dpdkResources,
                        std::shared_ptr<::apache::thrift::TConfiguration> config)
      : TVirtualTransport(config)
      , port_(0)
      , localIpAddress_(0)
      , dpdkResources_(dpdkResources)
      , sendTimeout_(0)
      , recvTimeout_(0) {
      memset(&peerAddr_, 0, sizeof(peerAddr_));
      setLocalIpAddress("192.168.1.1");
  }

  TUDPSocket::~TUDPSocket() {
    close();
  }

  bool TUDPSocket::initDPDK() {
    // Initialize EAL
    int argc = 0;
    char **argv = nullptr;
    int ret = rte_eal_init(argc, argv);
    if (ret < 0) {
      return false;
    }

    // Create mempool for packet buffers
    dpdkResources_->mbufPool  = createMempool();
    if (!dpdkResources_->mbufPool ) {
      return false;
    }

    // Setup network port
    if (!setupDPDKPort()) {
      return false;
    }

    isInitialized_ = true;
    return true;
  }

  struct rte_mempool* TUDPSocket::createMempool() {
    // Create mempool for packet buffers
    unsigned nb_ports = rte_eth_dev_count_avail();
    if (nb_ports == 0) {
      return nullptr;
    }

    char pool_name[64];
    snprintf(pool_name, sizeof(pool_name), "mbuf_pool_%d", dpdkResources_->portId);

    return rte_pktmbuf_pool_create(pool_name,
                                  NUM_MBUFS * nb_ports,
                                  MBUF_CACHE_SIZE,
                                  0,
                                  RTE_MBUF_DEFAULT_BUF_SIZE,
                                  rte_socket_id());
  }

  bool TUDPSocket::setupDPDKPort() {
    // Get port info
    rte_eth_dev_info_get(dpdkResources_->portId, &dpdkResources_->devInfo);

    // Configure port
    memset(&dpdkResources_->portConf, 0, sizeof(dpdkResources_->portConf));
    dpdkResources_->portConf.rxmode.max_lro_pkt_size = RTE_ETHER_MAX_LEN;
    dpdkResources_->portConf.rxmode.mq_mode = RTE_ETH_MQ_RX_NONE;
    dpdkResources_->portConf.txmode.mq_mode = RTE_ETH_MQ_TX_NONE;

    // Configure device
    int ret = rte_eth_dev_configure(dpdkResources_->portId, 1, 1, &dpdkResources_->portConf);
    if (ret != 0) {
      return false;
    }

    // Setup RX queue
    ret = rte_eth_rx_queue_setup(dpdkResources_->portId,
                                0,
                                RX_RING_SIZE,
                                rte_eth_dev_socket_id(dpdkResources_->portId),
                                nullptr,
                                dpdkResources_->mbufPool );
    if (ret < 0) {
      return false;
    }

    // Setup TX queue
    ret = rte_eth_tx_queue_setup(dpdkResources_->portId,
                                0,
                                TX_RING_SIZE,
                                rte_eth_dev_socket_id(dpdkResources_->portId),
                                nullptr);
    if (ret < 0) {
      return false;
    }

    // Start device
    ret = rte_eth_dev_start(dpdkResources_->portId);
    if (ret < 0) {
      return false;
    }

    return true;
  }

  bool TUDPSocket::isOpen() const {
    return dpdkResources_->isInitialized;
  }

  bool TUDPSocket::peek() {
    if (!dpdkResources_->isInitialized) {
      return false;
    }

    struct rte_mbuf* pkts_burst[BURST_SIZE];
    const uint16_t nb_rx = rte_eth_rx_burst(dpdkResources_->portId, 0, pkts_burst, BURST_SIZE);
    
    if (nb_rx > 0) {
      rte_pktmbuf_free(pkts_burst[0]);
      return true;
    }
    
    return false;
  }

  void TUDPSocket::open() {
    if (dpdkResources_->isInitialized) {
      return;
    }

    if (!initDPDK()) {
      throw TTransportException(TTransportException::NOT_OPEN,
                              "Could not initialize DPDK");
    }
  }

  void TUDPSocket::close() {
    if (!dpdkResources_->isInitialized) {
      return;
    }

    rte_eth_dev_stop(dpdkResources_->portId);
    rte_eth_dev_close(dpdkResources_->portId);
    
    if (dpdkResources_->mbufPool ) {
      rte_mempool_free(dpdkResources_->mbufPool );
      dpdkResources_->mbufPool  = nullptr;
    }

    dpdkResources_->isInitialized = false;
  }

  void TUDPSocket::setLocalIpAddress(const std::string& ipStr) {
          struct in_addr addr;
          inet_aton(ipStr.c_str(), &addr);
          localIpAddress_ = addr.s_addr;
  }

  void TUDPSocket::handleArpPacket(struct rte_mbuf* m) {
      struct rte_ether_hdr* eth_hdr = rte_pktmbuf_mtod(m, struct rte_ether_hdr*);
      struct rte_arp_hdr* arp_hdr = (struct rte_arp_hdr*)(eth_hdr + 1);

      if (rte_be_to_cpu_16(arp_hdr->arp_opcode) == RTE_ARP_OP_REQUEST &&
          arp_hdr->arp_data.arp_tip == localIpAddress_) {

          struct rte_mbuf* reply = rte_pktmbuf_alloc(dpdkResources_->mbufPool);
          if (reply) {
              char* pkt = rte_pktmbuf_append(reply,
                  sizeof(struct rte_ether_hdr) + sizeof(struct rte_arp_hdr));

              if (pkt) {
                  struct rte_ether_hdr* reply_eth =
                      rte_pktmbuf_mtod(reply, struct rte_ether_hdr*);
                  struct rte_arp_hdr* reply_arp =
                      (struct rte_arp_hdr*)(reply_eth + 1);

                  // Set ethernet header
                  reply_eth->dst_addr = eth_hdr->src_addr;
                  rte_eth_macaddr_get(dpdkResources_->portId, &reply_eth->src_addr);
                  reply_eth->ether_type = eth_hdr->ether_type;

                  // Set ARP header
                  reply_arp->arp_hardware = arp_hdr->arp_hardware;
                  reply_arp->arp_protocol = arp_hdr->arp_protocol;
                  reply_arp->arp_hlen = arp_hdr->arp_hlen;
                  reply_arp->arp_plen = arp_hdr->arp_plen;
                  reply_arp->arp_opcode = rte_cpu_to_be_16(RTE_ARP_OP_REPLY);

                  // Set ARP data
                  rte_eth_macaddr_get(dpdkResources_->portId,
                      &reply_arp->arp_data.arp_sha);
                  reply_arp->arp_data.arp_sip = localIpAddress_;
                  reply_arp->arp_data.arp_tha = arp_hdr->arp_data.arp_sha;
                  reply_arp->arp_data.arp_tip = arp_hdr->arp_data.arp_sip;

                  uint16_t nb_tx = rte_eth_tx_burst(dpdkResources_->portId,
                      0, &reply, 1);
                  if (nb_tx == 0) {
                      rte_pktmbuf_free(reply);
                  }
              } else {
                  rte_pktmbuf_free(reply);
              }
          }
      }
  }

//  uint32_t TUDPSocket::read(uint8_t* buf, uint32_t len) {
//      if (!dpdkResources_ || !dpdkResources_->isInitialized) {
//          throw TTransportException(TTransportException::NOT_OPEN, 
//                                  "Called read on non-open socket");
//      }
//
//      uint32_t total_rx = 0;
//      struct rte_mbuf* pkt = nullptr;
//      
//      //printf("Called read()\n"); 
//      
//      // Poll for packets with timeout handling
//      uint64_t start_time = rte_get_timer_cycles();
//      uint64_t timeout_cycles = (uint64_t)recvTimeout_ * rte_get_timer_hz() / 1000;
//
//      while (total_rx == 0) {
//          // Check for timeout
//          if (recvTimeout_ > 0) {
//              uint64_t elapsed = rte_get_timer_cycles() - start_time;
//              if (elapsed > timeout_cycles) {
//                  throw TTransportException(TTransportException::TIMED_OUT,
//                                          "UDP read timeout");
//              }
//          }
//
//          // Use optimized single packet receive
//          uint16_t nb_rx = rte_eth_rx_burst(dpdkResources_->portId, 0, &pkt, 1);
//          if (nb_rx == 0) continue;
//          //if (nb_rx > 0 ) printf("nb_rx: %d\n", nb_rx);
//          // Get packet headers
//          struct rte_ether_hdr* eth_hdr = rte_pktmbuf_mtod(pkt, struct rte_ether_hdr*);
//          uint16_t ether_type = rte_be_to_cpu_16(eth_hdr->ether_type);
//
//          // Handle ARP packets
//          if (ether_type == RTE_ETHER_TYPE_ARP) {
//              //fprintf(stderr, "Debug: Received ARP request\n");
//              handleArpPacket(pkt);
//              rte_pktmbuf_free(pkt);
//              continue;
//          }
//
//          // Skip non-IPv4 packets
//          if (ether_type != RTE_ETHER_TYPE_IPV4) {
//              fprintf(stderr, "Debug: found non-IPv4 packets\n");
//              rte_pktmbuf_free(pkt);
//              continue;
//          }
//
//          //struct rte_ipv4_hdr* ip_hdr = (struct rte_ipv4_hdr*)(eth_hdr + 1);
//          struct rte_ipv4_hdr* ip_hdr = rte_pktmbuf_mtod_offset(pkt, struct rte_ipv4_hdr*,
//                                                              sizeof(struct rte_ether_hdr));
//          struct rte_udp_hdr* udp_hdr = (struct rte_udp_hdr*)(ip_hdr + 1);
//
//          // Skip non-UDP packets
//          if (ip_hdr->next_proto_id != IPPROTO_UDP) {
//              fprintf(stderr, "Debug: found non-UDP packet\n");
//              rte_pktmbuf_free(pkt);
//              continue;
//          }
//
//          // Validate port if set
//          if (port_ != 0 && rte_be_to_cpu_16(udp_hdr->dst_port) != port_) {
//              rte_pktmbuf_free(pkt);
//              continue;
//          }
//
//          // Get and copy payload
//          uint8_t* payload = rte_pktmbuf_mtod_offset(pkt, uint8_t*, HEADERS_LEN);
//          if (!payload) {
//              printf("Error: Invalid payload\n");  // Debug`
//              rte_pktmbuf_free(pkt);
//              continue;
//          }
//          uint32_t payload_len = static_cast<uint32_t>(pkt->data_len - HEADERS_LEN);
//          uint32_t copy_len = std::min(payload_len, len); 
//          rte_memcpy(buf, payload, copy_len);
//          total_rx = copy_len;
//
//          // Cache peer address
//          peerAddr_.sin_family = AF_INET;
//          peerAddr_.sin_port = udp_hdr->src_port;
//          peerAddr_.sin_addr.s_addr = ip_hdr->src_addr;
//          
//          rte_pktmbuf_free(pkt);
//          break;
//      }
//      return total_rx;
//  }
  void TUDPSocket::handleReceivedData(uint8_t* data, uint32_t len) {
      std::lock_guard<std::mutex> lock(mutex_);
      rxBuffer_.insert(rxBuffer_.end(), data, data + len);
      cv_.notify_one();
  }
  
  uint32_t TUDPSocket::read(uint8_t* buf, uint32_t len) {
      // Wait for data to be available
      std::unique_lock<std::mutex> lock(mutex_);
      cv_.wait(lock, [this] { return !rxBuffer_.empty(); });

      // Copy data to buffer
      uint32_t copy_len = std::min(len, static_cast<uint32_t>(rxBuffer_.size()));
      std::memcpy(buf, rxBuffer_.data(), copy_len);
      rxBuffer_.erase(rxBuffer_.begin(), rxBuffer_.begin() + copy_len);

      return copy_len;
  }

  void TUDPSocket::write(const uint8_t* buf, uint32_t len) {
      auto& dpdk = DPDKHandler::getInstance();
      dpdk.sendData(buf, len);
  }

//  void TUDPSocket::write(const uint8_t* buf, uint32_t len) {
//    write_partial(buf, len);
//  }
//
//  uint32_t TUDPSocket::write_partial(const uint8_t* buf, uint32_t len) {
//    if (!dpdkResources_ || !dpdkResources_->isInitialized) {
//      throw TTransportException(TTransportException::NOT_OPEN,
//                              "Called write on non-open socket");
//    }
//
//    // Allocate new mbuf
//    struct rte_mbuf* m = rte_pktmbuf_alloc(dpdkResources_->mbufPool);
//    if (!m) {
//        throw TTransportException(TTransportException::UNKNOWN,
//                                "Failed to allocate mbuf");
//    }
//
//    // Reserve space for headers
//    char* pkt = rte_pktmbuf_append(m, HEADERS_LEN + len);
//    if (!pkt) {
//      rte_pktmbuf_free(m);
//      throw TTransportException(TTransportException::UNKNOWN,
//                              "Failed to reserve space in mbuf");
//    }
//
//    // Set up headers
//    struct rte_ether_hdr* eth_hdr = rte_pktmbuf_mtod(m, struct rte_ether_hdr*);
//    struct rte_ipv4_hdr* ip_hdr = (struct rte_ipv4_hdr*)(eth_hdr + 1);
//    struct rte_udp_hdr* udp_hdr = (struct rte_udp_hdr*)(ip_hdr + 1);
//    uint8_t* payload = (uint8_t*)(udp_hdr + 1);
//
//    // Ethernet header
//    rte_eth_macaddr_get(dpdkResources_->portId, &eth_hdr->src_addr);
//    // Destination MAC should be set based on ARP or known destination
//    // For now using broadcast
//    // memset(&eth_hdr->dst_addr, 0xff, RTE_ETHER_ADDR_LEN); 
//    // Set destination MAC directly
//    // Converting 0c:42:a1:cc:83:82 to bytes
//    eth_hdr->dst_addr.addr_bytes[0] = 0x0c;
//    eth_hdr->dst_addr.addr_bytes[1] = 0x42;
//    eth_hdr->dst_addr.addr_bytes[2] = 0xa1;
//    eth_hdr->dst_addr.addr_bytes[3] = 0xcc;
//    eth_hdr->dst_addr.addr_bytes[4] = 0x83;
//    eth_hdr->dst_addr.addr_bytes[5] = 0x82;
//
//    eth_hdr->ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);
//
//    // IP header
//    memset(ip_hdr, 0, sizeof(*ip_hdr));
//    ip_hdr->version_ihl = (4 << 4) | (sizeof(*ip_hdr) >> 2);
//    ip_hdr->type_of_service = 0;
//    ip_hdr->total_length = rte_cpu_to_be_16(sizeof(*ip_hdr) + sizeof(*udp_hdr) + len);
//    ip_hdr->packet_id = 0;
//    ip_hdr->fragment_offset = 0;
//    ip_hdr->time_to_live = 64;
//    ip_hdr->next_proto_id = IPPROTO_UDP;
//    // Source IP should be interface IP
//    ip_hdr->src_addr = localIpAddress_; //use configured Ip
//    // Destination IP from peer address
//    ip_hdr->dst_addr = peerAddr_.sin_addr.s_addr;
//    ip_hdr->hdr_checksum = 0;
//    ip_hdr->hdr_checksum = rte_ipv4_cksum(ip_hdr);
//
//    // UDP header
//    udp_hdr->src_port = rte_cpu_to_be_16(port_);
//    udp_hdr->dst_port = peerAddr_.sin_port;
//    udp_hdr->dgram_len = rte_cpu_to_be_16(sizeof(*udp_hdr) + len);
//    udp_hdr->dgram_cksum = 0;
//
//    // Copy payload
//    rte_memcpy(payload, buf, len);
//
//    // Calculate UDP checksum
//    udp_hdr->dgram_cksum = rte_ipv4_udptcp_cksum(ip_hdr, udp_hdr);
//
//    // Send packet
//    uint16_t nb_tx = rte_eth_tx_burst(dpdkResources_->portId, 0, &m, 1);
//    if (nb_tx == 0) {
//      rte_pktmbuf_free(m);
//      throw TTransportException(TTransportException::UNKNOWN,
//                              "Failed to send packet");
//    }
//
//    return len;
//  }

  // Helper function to get local IP address
  uint32_t TUDPSocket::GetLocalIPAddress(uint16_t port_id __attribute__((unused))) {
    // This should be implemented to return the IP address assigned to the DPDK port
    // For example, you could store this during initialization
    // For now returning a placeholder
    return rte_cpu_to_be_32(0x0A000001); // 10.0.0.1
  }

  void TUDPSocket::setRecvTimeout(int ms) {
    recvTimeout_ = ms;
  }

  void TUDPSocket::setSendTimeout(int ms) {
    sendTimeout_ = ms;
  }

  const std::string TUDPSocket::getOrigin() const {
    std::ostringstream oss;
    oss << host_ << ":" << port_;
    return oss.str();
  }
  // ***TServerUDPSocket.h***
  class TUDPSocket;

  class TServerUDPSocket : public TServerTransport {
  public:
    typedef std::function<void(uint16_t portId)> port_func_t;

    static const uint16_t RX_RING_SIZE = 1024;
    static const uint16_t TX_RING_SIZE = 1024;
    static const uint16_t NUM_MBUFS = 8191;
    static const uint16_t MBUF_CACHE_SIZE = 250;
    
    TServerUDPSocket(int port);
    TServerUDPSocket(int port, int sendTimeout, int recvTimeout);
    ~TServerUDPSocket() override;

    bool isOpen() const override;
    void listen() override;
    void interrupt() override;
    void close() override;

    void setSendTimeout(int sendTimeout);
    void setRecvTimeout(int recvTimeout);
    void setListenCallback(const port_func_t& listenCallback) {
      listenCallback_ = listenCallback;
    }

    int getPort() const { return port_; }

    std::shared_ptr<DPDKResources> getDPDKResources() const {
          return dpdkResources_;
      }

  protected:
    std::shared_ptr<TTransport> acceptImpl() override;
    virtual std::shared_ptr<TUDPSocket> createSocket(uint16_t portId);
    
    bool initDPDK();
    bool setupDPDKPort();
    struct rte_mempool* createMempool();

  private:
    int port_;
    bool isInitialized_;
    
    // DPDK specific members
    std::shared_ptr<DPDKResources> dpdkResources_;
    
    int sendTimeout_;
    int recvTimeout_;
    
    concurrency::Mutex mutex_;
    port_func_t listenCallback_;
  };

  // ***TServerUDPSocket.cpp***
  TServerUDPSocket::TServerUDPSocket(int port)
    : port_(port)
    , isInitialized_(true)
    , sendTimeout_(0)
    , recvTimeout_(0) {
  }

  TServerUDPSocket::TServerUDPSocket(int port, int sendTimeout, int recvTimeout)
    : port_(port)
    , isInitialized_(true)
    , sendTimeout_(sendTimeout)
    , recvTimeout_(recvTimeout) {
  }

  TServerUDPSocket::~TServerUDPSocket() {
    close();
  }

  bool TServerUDPSocket::initDPDK() {
      dpdkResources_ = std::make_shared<DPDKResources>();
      
      // Minimal EAL arguments
      const char* argv[] = {
          "thrift-server",           // Program name
          "-l", "0-1",              // Use CPU cores 0-1
          "-n", "4",                // Number of memory channels
          "--proc-type=auto",       // Process type
          "--log-level", "8",       // Debug log level
          NULL
      };
      int argc = sizeof(argv) / sizeof(argv[0]) - 1;  // -1 for NULL terminator
      // Initialize EAL
      int ret = rte_eal_init(argc, (char**)argv);
      if (ret < 0) {
          return false;
      }

      // Create mempool
      dpdkResources_->mbufPool = createMempool();
      if (!dpdkResources_->mbufPool) {
          return false;
      }

      // Setup port
      dpdkResources_->portId = 0;  // Or however you select the port
      if (!setupDPDKPort()) {
          return false;
      }

      dpdkResources_->isInitialized = true;
      return true;
  }

  struct rte_mempool* TServerUDPSocket::createMempool() {
    unsigned nb_ports = rte_eth_dev_count_avail();
    if (nb_ports == 0) {
      return nullptr;
    }

    char pool_name[64];
    snprintf(pool_name, sizeof(pool_name), "mbuf_pool_%d", dpdkResources_->portId);

    return rte_pktmbuf_pool_create(pool_name,
                                  NUM_MBUFS * nb_ports,
                                  MBUF_CACHE_SIZE,
                                  0,
                                  RTE_MBUF_DEFAULT_BUF_SIZE,
                                  rte_socket_id());
  }

  bool TServerUDPSocket::setupDPDKPort() {
    // Get port info
    rte_eth_dev_info_get(dpdkResources_->portId, &dpdkResources_->devInfo);

    // Configure port
    memset(&dpdkResources_->portConf, 0, sizeof(dpdkResources_->portConf));
    // Set proper packet type flags
    dpdkResources_->portConf.rxmode.max_lro_pkt_size = RTE_ETHER_MAX_LEN;
    dpdkResources_->portConf.rxmode.mq_mode = RTE_ETH_MQ_RX_NONE;
    dpdkResources_->portConf.rxmode.offloads = RTE_ETH_RX_OFFLOAD_CHECKSUM;
    
    // Accept all packet types
    dpdkResources_->portConf.rxmode.offloads |= RTE_ETH_RX_OFFLOAD_IPV4_CKSUM;
    dpdkResources_->portConf.rxmode.offloads |= RTE_ETH_RX_OFFLOAD_UDP_CKSUM;
    
    dpdkResources_->portConf.txmode.mq_mode = RTE_ETH_MQ_TX_NONE;
    dpdkResources_->portConf.txmode.offloads = RTE_ETH_TX_OFFLOAD_IPV4_CKSUM | 
                                              RTE_ETH_TX_OFFLOAD_UDP_CKSUM;

    // Configure device
    int ret = rte_eth_dev_configure(dpdkResources_->portId, 1, 1, &dpdkResources_->portConf);
    if (ret != 0) {
      fprintf(stderr, "Failed to configure port %d (err=%d)\n", dpdkResources_->portId, ret);
      return false;
    }

    // Get the port MAC address
    struct rte_ether_addr addr;
    ret = rte_eth_macaddr_get(dpdkResources_->portId, &addr);
    if (ret != 0) {
      return false;
    }

    // Setup RX queue
    ret = rte_eth_rx_queue_setup(dpdkResources_->portId,
                                0,
                                RX_RING_SIZE,
                                rte_eth_dev_socket_id(dpdkResources_->portId),
                                nullptr,
                                dpdkResources_->mbufPool );
    if (ret < 0) {
      return false;
    }

    // Setup TX queue
    ret = rte_eth_tx_queue_setup(dpdkResources_->portId,
                                0,
                                TX_RING_SIZE,
                                rte_eth_dev_socket_id(dpdkResources_->portId),
                                nullptr);
    if (ret < 0) {
      return false;
    }

    // Enable promiscuous mode
    ret = rte_eth_promiscuous_enable(dpdkResources_->portId);
    if (ret != 0) {
      return false;
    }

    // Start device
    ret = rte_eth_dev_start(dpdkResources_->portId);
    if (ret < 0) {
      return false;
    }

    // Check link status
    struct rte_eth_link link;
    ret = rte_eth_link_get(dpdkResources_->portId, &link);
    if (ret < 0 || !link.link_status) {
      return false;
    }

    return true;
  }

  void TServerUDPSocket::listen() {
    if (isInitialized_) {
      dpdkResources_ = std::make_shared<DPDKResources>();
      return;
    }

    if (port_ < 0 || port_ > 0xFFFF) {
      throw TTransportException(TTransportException::BAD_ARGS,
                              "Invalid port number");
    }

    if (!initDPDK()) {
      throw TTransportException(TTransportException::NOT_OPEN,
                              "Failed to initialize DPDK");
    }

    if (listenCallback_) {
      listenCallback_(dpdkResources_->portId);
    }
  }

  std::shared_ptr<TTransport> TServerUDPSocket::acceptImpl() {
    if (!dpdkResources_->isInitialized) {
      throw TTransportException(TTransportException::NOT_OPEN,
                              "Server socket not initialized");
    }

    // Create new UDP socket for client communication
    // printf("about to create socket\n");
    std::shared_ptr<TUDPSocket> client = createSocket(dpdkResources_->portId);
    
    if (sendTimeout_ > 0) {
      client->setSendTimeout(sendTimeout_);
    }
    if (recvTimeout_ > 0) {
      client->setRecvTimeout(recvTimeout_);
    }
    // fprintf(stderr, "DPDK Initialized, returning client!\n");
    return client;
  }

  std::shared_ptr<TUDPSocket> TServerUDPSocket::createSocket(uint16_t portId) {
      // Create socket with shared DPDK resources
      //auto socket = std::make_shared<TUDPSocket>(dpdkResources_);
      auto socket = std::make_shared<TUDPSocket>();
      
      // Set socket-specific parameters
      if (sendTimeout_ > 0) {
          socket->setSendTimeout(sendTimeout_);
      }
      if (recvTimeout_ > 0) {
          socket->setRecvTimeout(recvTimeout_);
      }
      
      return socket;
  }
  void TServerUDPSocket::interrupt() {
    concurrency::Guard g(mutex_);
    if (dpdkResources_->isInitialized) {
      // Send an interrupt packet to wake up polling
      struct rte_mbuf* m = rte_pktmbuf_alloc(dpdkResources_->mbufPool );
      if (m) {
        uint16_t nb_tx = rte_eth_tx_burst(dpdkResources_->portId, 0, &m, 1);
        if (nb_tx == 0) {
          rte_pktmbuf_free(m);
        }
      }
    }
  }

  void TServerUDPSocket::close() {
    concurrency::Guard g(mutex_);
    
    if (!dpdkResources_->isInitialized) {
      return;
    }

    rte_eth_dev_stop(dpdkResources_->portId);
    rte_eth_dev_close(dpdkResources_->portId);
    
    if (dpdkResources_->mbufPool ) {
      rte_mempool_free(dpdkResources_->mbufPool );
      dpdkResources_->mbufPool  = nullptr;
    }

    isInitialized_ = false;
    dpdkResources_->isInitialized = false;
  }

  bool TServerUDPSocket::isOpen() const {
    return isInitialized_ || dpdkResources_->isInitialized;
  }

  void TServerUDPSocket::setSendTimeout(int sendTimeout) {
    sendTimeout_ = sendTimeout;
  }

  void TServerUDPSocket::setRecvTimeout(int recvTimeout) {
    recvTimeout_ = recvTimeout;
  }
  } // my_thrift::apache::thrift::transport

  namespace protocol {

  // ***TProtocolException.h***
  /**
   * Class to encapsulate all the possible types of protocol errors that may
   * occur in various protocol systems. This provides a sort of generic
   * wrapper around the vague UNIX E_ error codes that lets a common code
   * base of error handling to be used for various types of protocols, i.e.
   * pipes etc.
   *
   */
  class TProtocolException : public ::apache::thrift::TException {
  public:
    /**
     * Error codes for the various types of exceptions.
     */
    enum TProtocolExceptionType {
      UNKNOWN = 0,
      INVALID_DATA = 1,
      NEGATIVE_SIZE = 2,
      SIZE_LIMIT = 3,
      BAD_VERSION = 4,
      NOT_IMPLEMENTED = 5,
      DEPTH_LIMIT = 6
    };

    TProtocolException() : ::apache::thrift::TException(), type_(UNKNOWN) {}

    TProtocolException(TProtocolExceptionType type) : ::apache::thrift::TException(), type_(type) {}

    TProtocolException(const std::string& message)
      : ::apache::thrift::TException(message), type_(UNKNOWN) {}

    TProtocolException(TProtocolExceptionType type, const std::string& message)
      : ::apache::thrift::TException(message), type_(type) {}

    ~TProtocolException() noexcept override = default;

    /**
     * Returns an error code that provides information about the type of error
     * that has occurred.
     *
     * @return Error code
     */
    TProtocolExceptionType getType() const { return type_; }

    const char* what() const noexcept override {
      if (message_.empty()) {
        switch (type_) {
        case UNKNOWN:
          return "TProtocolException: Unknown protocol exception";
        case INVALID_DATA:
          return "TProtocolException: Invalid data";
        case NEGATIVE_SIZE:
          return "TProtocolException: Negative size";
        case SIZE_LIMIT:
          return "TProtocolException: Exceeded size limit";
        case BAD_VERSION:
          return "TProtocolException: Invalid version";
        case NOT_IMPLEMENTED:
          return "TProtocolException: Not implemented";
        case DEPTH_LIMIT:
          return "TProtocolException: Exceeded depth limit";
        default:
          return "TProtocolException: (Invalid exception type)";
        }
      } else {
        return message_.c_str();
      }
    }

  protected:
    /**
     * Error code
     */
    TProtocolExceptionType type_;
  };
  
  // ***TEnum.h***
  /**
   * Enumerated definition of the types that the Thrift protocol supports.
   * Take special note of the T_END type which is used specifically to mark
   * the end of a sequence of fields.
   */
  enum TType {
    T_STOP       = 0,
    T_VOID       = 1,
    T_BOOL       = 2,
    T_BYTE       = 3,
    T_I08        = 3,
    T_I16        = 6,
    T_I32        = 8,
    T_U64        = 9,
    T_I64        = 10,
    T_DOUBLE     = 4,
    T_STRING     = 11,
    T_UTF7       = 11,
    T_STRUCT     = 12,
    T_MAP        = 13,
    T_SET        = 14,
    T_LIST       = 15,
    T_UUID       = 16,
  };

  /**
   * Enumerated definition of the message types that the Thrift protocol
   * supports.
   */
  enum TMessageType {
    T_CALL       = 1,
    T_REPLY      = 2,
    T_EXCEPTION  = 3,
    T_ONEWAY     = 4
  };

  // ***TList.h***
  /**
   * Helper class that encapsulates list metadata.
   *
   */
  class TList {
  public:
    TList() : elemType_(T_STOP), 
    size_(0) {

    }

    TList(TType t = T_STOP, int s = 0)
      : elemType_(t),
        size_(s) { 
          
    }

    TType elemType_;
    int  size_;
  };

  // ***TSet.h***
  /**
   * Helper class that encapsulates set metadata.
   *
   */
  class TSet {
  public:
    TSet() : elemType_(T_STOP), size_(0) {

    }

    TSet(TType t, int s)
      : elemType_(t),
        size_(s) { 
          
    }

    TSet(TList list)
      : elemType_(list.elemType_),
        size_(list.size_) { 
          
    }

    TType elemType_;
    int  size_;
  };
  
  // ***TMap.h***
  /**
   * Helper class that encapsulates map metadata.
   *
   */
  class TMap {
  public: 
    TMap()
      : keyType_(T_STOP),
        valueType_(T_STOP),
        size_(0) {

    }

    TMap(TType k, TType v, int s)
      : keyType_(k),
        valueType_(v),
        size_(s) {

    }

    TType  keyType_;
    TType  valueType_;
    int   size_;
  };

  // ***TProtocol.h***
  using my_thrift::apache::thrift::transport::TTransport;

  /**
   * Abstract class for a thrift protocol driver. These are all the methods that
   * a protocol must implement. Essentially, there must be some way of reading
   * and writing all the base types, plus a mechanism for writing out structs
   * with indexed fields.
   *
   * TProtocol objects should not be shared across multiple encoding contexts,
   * as they may need to maintain internal state in some protocols (i.e. XML).
   * Note that is is acceptable for the TProtocol module to do its own internal
   * buffered reads/writes to the underlying TTransport where appropriate (i.e.
   * when parsing an input XML stream, reading should be batched rather than
   * looking ahead character by character for a close tag).
   *
   */
  class TProtocol {
  public:
    virtual ~TProtocol();

    /**
     * Writing functions.
     */

    virtual uint32_t writeMessageBegin_virt(const std::string& name,
                                            const TMessageType messageType,
                                            const int32_t seqid) = 0;

    virtual uint32_t writeMessageEnd_virt() = 0;

    virtual uint32_t writeStructBegin_virt(const char* name) = 0;

    virtual uint32_t writeStructEnd_virt() = 0;

    virtual uint32_t writeFieldBegin_virt(const char* name,
                                          const TType fieldType,
                                          const int16_t fieldId) = 0;

    virtual uint32_t writeFieldEnd_virt() = 0;

    virtual uint32_t writeFieldStop_virt() = 0;

    virtual uint32_t writeMapBegin_virt(const TType keyType, const TType valType, const uint32_t size)
        = 0;

    virtual uint32_t writeMapEnd_virt() = 0;

    virtual uint32_t writeListBegin_virt(const TType elemType, const uint32_t size) = 0;

    virtual uint32_t writeListEnd_virt() = 0;

    virtual uint32_t writeSetBegin_virt(const TType elemType, const uint32_t size) = 0;

    virtual uint32_t writeSetEnd_virt() = 0;

    virtual uint32_t writeBool_virt(const bool value) = 0;

    virtual uint32_t writeByte_virt(const int8_t byte) = 0;

    virtual uint32_t writeI16_virt(const int16_t i16) = 0;

    virtual uint32_t writeI32_virt(const int32_t i32) = 0;

    virtual uint32_t writeI64_virt(const int64_t i64) = 0;

    virtual uint32_t writeDouble_virt(const double dub) = 0;

    virtual uint32_t writeString_virt(const std::string& str) = 0;

    virtual uint32_t writeBinary_virt(const std::string& str) = 0;

    virtual uint32_t writeUUID_virt(const ::apache::thrift::TUuid& uuid) = 0;

    uint32_t writeMessageBegin(const std::string& name,
                              const TMessageType messageType,
                              const int32_t seqid) {
      T_VIRTUAL_CALL();
      return writeMessageBegin_virt(name, messageType, seqid);
    }

    uint32_t writeMessageEnd() {
      T_VIRTUAL_CALL();
      return writeMessageEnd_virt();
    }

    uint32_t writeStructBegin(const char* name) {
      T_VIRTUAL_CALL();
      return writeStructBegin_virt(name);
    }

    uint32_t writeStructEnd() {
      T_VIRTUAL_CALL();
      return writeStructEnd_virt();
    }

    uint32_t writeFieldBegin(const char* name, const TType fieldType, const int16_t fieldId) {
      T_VIRTUAL_CALL();
      return writeFieldBegin_virt(name, fieldType, fieldId);
    }

    uint32_t writeFieldEnd() {
      T_VIRTUAL_CALL();
      return writeFieldEnd_virt();
    }

    uint32_t writeFieldStop() {
      T_VIRTUAL_CALL();
      return writeFieldStop_virt();
    }

    uint32_t writeMapBegin(const TType keyType, const TType valType, const uint32_t size) {
      T_VIRTUAL_CALL();
      return writeMapBegin_virt(keyType, valType, size);
    }

    uint32_t writeMapEnd() {
      T_VIRTUAL_CALL();
      return writeMapEnd_virt();
    }

    uint32_t writeListBegin(const TType elemType, const uint32_t size) {
      T_VIRTUAL_CALL();
      return writeListBegin_virt(elemType, size);
    }

    uint32_t writeListEnd() {
      T_VIRTUAL_CALL();
      return writeListEnd_virt();
    }

    uint32_t writeSetBegin(const TType elemType, const uint32_t size) {
      T_VIRTUAL_CALL();
      return writeSetBegin_virt(elemType, size);
    }

    uint32_t writeSetEnd() {
      T_VIRTUAL_CALL();
      return writeSetEnd_virt();
    }

    uint32_t writeBool(const bool value) {
      T_VIRTUAL_CALL();
      return writeBool_virt(value);
    }

    uint32_t writeByte(const int8_t byte) {
      T_VIRTUAL_CALL();
      return writeByte_virt(byte);
    }

    uint32_t writeI16(const int16_t i16) {
      T_VIRTUAL_CALL();
      return writeI16_virt(i16);
    }

    uint32_t writeI32(const int32_t i32) {
      T_VIRTUAL_CALL();
      return writeI32_virt(i32);
    }

    uint32_t writeI64(const int64_t i64) {
      T_VIRTUAL_CALL();
      return writeI64_virt(i64);
    }

    uint32_t writeDouble(const double dub) {
      T_VIRTUAL_CALL();
      return writeDouble_virt(dub);
    }

    uint32_t writeString(const std::string& str) {
      T_VIRTUAL_CALL();
      return writeString_virt(str);
    }

    uint32_t writeBinary(const std::string& str) {
      T_VIRTUAL_CALL();
      return writeBinary_virt(str);
    }

    uint32_t writeUUID(const ::apache::thrift::TUuid& uuid) {
      T_VIRTUAL_CALL();
      return writeUUID_virt(uuid);
    }

    /**
     * Reading functions
     */

    virtual uint32_t readMessageBegin_virt(std::string& name,
                                          TMessageType& messageType,
                                          int32_t& seqid) = 0;

    virtual uint32_t readMessageEnd_virt() = 0;

    virtual uint32_t readStructBegin_virt(std::string& name) = 0;

    virtual uint32_t readStructEnd_virt() = 0;

    virtual uint32_t readFieldBegin_virt(std::string& name, TType& fieldType, int16_t& fieldId) = 0;

    virtual uint32_t readFieldEnd_virt() = 0;

    virtual uint32_t readMapBegin_virt(TType& keyType, TType& valType, uint32_t& size) = 0;

    virtual uint32_t readMapEnd_virt() = 0;

    virtual uint32_t readListBegin_virt(TType& elemType, uint32_t& size) = 0;

    virtual uint32_t readListEnd_virt() = 0;

    virtual uint32_t readSetBegin_virt(TType& elemType, uint32_t& size) = 0;

    virtual uint32_t readSetEnd_virt() = 0;

    virtual uint32_t readBool_virt(bool& value) = 0;

    virtual uint32_t readBool_virt(std::vector<bool>::reference value) = 0;

    virtual uint32_t readByte_virt(int8_t& byte) = 0;

    virtual uint32_t readI16_virt(int16_t& i16) = 0;

    virtual uint32_t readI32_virt(int32_t& i32) = 0;

    virtual uint32_t readI64_virt(int64_t& i64) = 0;

    virtual uint32_t readDouble_virt(double& dub) = 0;

    virtual uint32_t readString_virt(std::string& str) = 0;

    virtual uint32_t readBinary_virt(std::string& str) = 0;

    virtual uint32_t readUUID_virt(::apache::thrift::TUuid& uuid) = 0;

    uint32_t readMessageBegin(std::string& name, TMessageType& messageType, int32_t& seqid) { // links from TDispatchProcess.h:114
      T_VIRTUAL_CALL();
      return readMessageBegin_virt(name, messageType, seqid); //links to TVirtualProtocol.h:404  
    }

    uint32_t readMessageEnd() {
      T_VIRTUAL_CALL();
      return readMessageEnd_virt();
    }

    uint32_t readStructBegin(std::string& name) {
      T_VIRTUAL_CALL();
      return readStructBegin_virt(name);
    }

    uint32_t readStructEnd() {
      T_VIRTUAL_CALL();
      return readStructEnd_virt();
    }

    uint32_t readFieldBegin(std::string& name, TType& fieldType, int16_t& fieldId) {
      T_VIRTUAL_CALL();
      return readFieldBegin_virt(name, fieldType, fieldId);
    }

    uint32_t readFieldEnd() {
      T_VIRTUAL_CALL();
      return readFieldEnd_virt();
    }

    uint32_t readMapBegin(TType& keyType, TType& valType, uint32_t& size) {
      T_VIRTUAL_CALL();
      return readMapBegin_virt(keyType, valType, size);
    }

    uint32_t readMapEnd() {
      T_VIRTUAL_CALL();
      return readMapEnd_virt();
    }

    uint32_t readListBegin(TType& elemType, uint32_t& size) {
      T_VIRTUAL_CALL();
      return readListBegin_virt(elemType, size);
    }

    uint32_t readListEnd() {
      T_VIRTUAL_CALL();
      return readListEnd_virt();
    }

    uint32_t readSetBegin(TType& elemType, uint32_t& size) {
      T_VIRTUAL_CALL();
      return readSetBegin_virt(elemType, size);
    }

    uint32_t readSetEnd() {
      T_VIRTUAL_CALL();
      return readSetEnd_virt();
    }

    uint32_t readBool(bool& value) {
      T_VIRTUAL_CALL();
      return readBool_virt(value);
    }

    uint32_t readByte(int8_t& byte) {
      T_VIRTUAL_CALL();
      return readByte_virt(byte);
    }

    uint32_t readI16(int16_t& i16) {
      T_VIRTUAL_CALL();
      return readI16_virt(i16);
    }

    uint32_t readI32(int32_t& i32) {
      T_VIRTUAL_CALL();
      return readI32_virt(i32);
    }

    uint32_t readI64(int64_t& i64) {
      T_VIRTUAL_CALL();
      return readI64_virt(i64);
    }

    uint32_t readDouble(double& dub) {
      T_VIRTUAL_CALL();
      return readDouble_virt(dub);
    }

    uint32_t readString(std::string& str) {
      T_VIRTUAL_CALL();
      return readString_virt(str);
    }

    uint32_t readBinary(std::string& str) {
      T_VIRTUAL_CALL();
      return readBinary_virt(str);
    }

    uint32_t readUUID(::apache::thrift::TUuid& uuid) {
      T_VIRTUAL_CALL();
      return readUUID_virt(uuid);
    }

    /*
    * std::vector is specialized for bool, and its elements are individual bits
    * rather than bools.   We need to define a different version of readBool()
    * to work with std::vector<bool>.
    */
    uint32_t readBool(std::vector<bool>::reference value) {
      T_VIRTUAL_CALL();
      return readBool_virt(value);
    }

    /**
     * Method to arbitrarily skip over data.
     */
    uint32_t skip(TType type) {
      T_VIRTUAL_CALL();
      return skip_virt(type);
    }
    virtual uint32_t skip_virt(TType type);

    inline std::shared_ptr<TTransport> getTransport() { return ptrans_; }

    // TODO: remove these two calls, they are for backwards
    // compatibility
    inline std::shared_ptr<TTransport> getInputTransport() { return ptrans_; }
    inline std::shared_ptr<TTransport> getOutputTransport() { return ptrans_; }

    // input and output recursion depth are kept separate so that one protocol
    // can be used concurrently for both input and output.
    void incrementInputRecursionDepth() {
      if (recursion_limit_ < ++input_recursion_depth_) {
        throw TProtocolException(TProtocolException::DEPTH_LIMIT);
      }
    }
    void decrementInputRecursionDepth() { --input_recursion_depth_; }

    void incrementOutputRecursionDepth() {
      if (recursion_limit_ < ++output_recursion_depth_) {
        throw TProtocolException(TProtocolException::DEPTH_LIMIT);
      }
    }
    void decrementOutputRecursionDepth() { --output_recursion_depth_; }

    uint32_t getRecursionLimit() const {return recursion_limit_;}
    void setRecurisionLimit(uint32_t depth) {recursion_limit_ = depth;}

    // Returns the minimum amount of bytes needed to store the smallest possible instance of TType.
    virtual int getMinSerializedSize(TType type) {
      THRIFT_UNUSED_VARIABLE(type);
      return 0;
    }

  protected:
    TProtocol(std::shared_ptr<TTransport> ptrans)
      : ptrans_(ptrans), input_recursion_depth_(0), output_recursion_depth_(0),
        recursion_limit_(ptrans->getConfiguration()->getRecursionLimit())
    {}

    virtual void checkReadBytesAvailable(TSet& set)
    {
        ptrans_->checkReadBytesAvailable(set.size_ * getMinSerializedSize(set.elemType_));
    }

    virtual void checkReadBytesAvailable(TList& list)
    {
        ptrans_->checkReadBytesAvailable(list.size_ * getMinSerializedSize(list.elemType_));
    }

    virtual void checkReadBytesAvailable(TMap& map)
    {
        int elmSize = getMinSerializedSize(map.keyType_) + getMinSerializedSize(map.valueType_);
        ptrans_->checkReadBytesAvailable(map.size_ * elmSize);
    }

    std::shared_ptr<TTransport> ptrans_;

  private:
    TProtocol() = default;
    uint32_t input_recursion_depth_;
    uint32_t output_recursion_depth_;
    uint32_t recursion_limit_;
  };

  /**
   * Constructs input and output protocol objects given transports.
   */
  class TProtocolFactory {
  public:
    TProtocolFactory() = default;

    virtual ~TProtocolFactory();

    virtual std::shared_ptr<TProtocol> getProtocol(std::shared_ptr<TTransport> trans) = 0;
    virtual std::shared_ptr<TProtocol> getProtocol(std::shared_ptr<TTransport> inTrans,
                std::shared_ptr<TTransport> outTrans) {
      (void)outTrans;
      return getProtocol(inTrans);
    }
  };

  /**
   * Dummy protocol class.
   *
   * This class does nothing, and should never be instantiated.
   * It is used only by the generator code.
   */
  class TDummyProtocol : public TProtocol {};

  // This is the default / legacy choice
  struct TNetworkBigEndian
  {
    static uint16_t toWire16(uint16_t x)   {return htons(x);}
    static uint32_t toWire32(uint32_t x)   {return htonl(x);}
    static uint64_t toWire64(uint64_t x)   {return THRIFT_htonll(x);}
    static uint16_t fromWire16(uint16_t x) {return ntohs(x);}
    static uint32_t fromWire32(uint32_t x) {return ntohl(x);}
    static uint64_t fromWire64(uint64_t x) {return THRIFT_ntohll(x);}
  };

  // On most systems, this will be a bit faster than TNetworkBigEndian
  struct TNetworkLittleEndian
  {
    static uint16_t toWire16(uint16_t x)   {return THRIFT_htoles(x);}
    static uint32_t toWire32(uint32_t x)   {return THRIFT_htolel(x);}
    static uint64_t toWire64(uint64_t x)   {return THRIFT_htolell(x);}
    static uint16_t fromWire16(uint16_t x) {return THRIFT_letohs(x);}
    static uint32_t fromWire32(uint32_t x) {return THRIFT_letohl(x);}
    static uint64_t fromWire64(uint64_t x) {return THRIFT_letohll(x);}
  };

  struct TOutputRecursionTracker {
    TProtocol &prot_;
    TOutputRecursionTracker(TProtocol &prot) : prot_(prot) {
      prot_.incrementOutputRecursionDepth();
    }
    ~TOutputRecursionTracker() {
      prot_.decrementOutputRecursionDepth();
    }
  };

  struct TInputRecursionTracker {
    TProtocol &prot_;
    TInputRecursionTracker(TProtocol &prot) : prot_(prot) {
      prot_.incrementInputRecursionDepth();
    }
    ~TInputRecursionTracker() {
      prot_.decrementInputRecursionDepth();
    }
  };

  /**
   * Helper template for implementing TProtocol::skip().
   *
   * Templatized to avoid having to make virtual function calls.
   */
  template <class Protocol_>
  uint32_t skip(Protocol_& prot, TType type) {
    TInputRecursionTracker tracker(prot);

    switch (type) {
    case T_BOOL: {
      bool boolv;
      return prot.readBool(boolv);
    }
    case T_BYTE: {
      int8_t bytev = 0;
      return prot.readByte(bytev);
    }
    case T_I16: {
      int16_t i16;
      return prot.readI16(i16);
    }
    case T_I32: {
      int32_t i32;
      return prot.readI32(i32);
    }
    case T_I64: {
      int64_t i64;
      return prot.readI64(i64);
    }
    case T_DOUBLE: {
      double dub;
      return prot.readDouble(dub);
    }
    case T_STRING: {
      std::string str;
      return prot.readBinary(str);
    }
    case T_STRUCT: {
      uint32_t result = 0;
      std::string name;
      int16_t fid;
      TType ftype;
      result += prot.readStructBegin(name);
      while (true) {
        result += prot.readFieldBegin(name, ftype, fid);
        if (ftype == T_STOP) {
          break;
        }
        result += skip(prot, ftype);
        result += prot.readFieldEnd();
      }
      result += prot.readStructEnd();
      return result;
    }
    case T_MAP: {
      uint32_t result = 0;
      TType keyType;
      TType valType;
      uint32_t i, size;
      result += prot.readMapBegin(keyType, valType, size);
      for (i = 0; i < size; i++) {
        result += skip(prot, keyType);
        result += skip(prot, valType);
      }
      result += prot.readMapEnd();
      return result;
    }
    case T_SET: {
      uint32_t result = 0;
      TType elemType;
      uint32_t i, size;
      result += prot.readSetBegin(elemType, size);
      for (i = 0; i < size; i++) {
        result += skip(prot, elemType);
      }
      result += prot.readSetEnd();
      return result;
    }
    case T_LIST: {
      uint32_t result = 0;
      TType elemType;
      uint32_t i, size;
      result += prot.readListBegin(elemType, size);
      for (i = 0; i < size; i++) {
        result += skip(prot, elemType);
      }
      result += prot.readListEnd();
      return result;
    }
    default:
      break;
    }

    throw TProtocolException(TProtocolException::INVALID_DATA,
                            "invalid TType");
  }
  // ***TProtocol.cpp***
  TProtocol::~TProtocol() = default;
  uint32_t TProtocol::skip_virt(TType type) {
    return ::my_thrift::apache::thrift::protocol::skip(*this, type);
  }

  TProtocolFactory::~TProtocolFactory() = default;

  // ***TVirtualProtocol.h***
  using my_thrift::apache::thrift::transport::TTransport;

  /**
   * Helper class that provides default implementations of TProtocol methods.
   *
   * This class provides default implementations of the non-virtual TProtocol
   * methods.  It exists primarily so TVirtualProtocol can derive from it.  It
   * prevents TVirtualProtocol methods from causing infinite recursion if the
   * non-virtual methods are not overridden by the TVirtualProtocol subclass.
   *
   * You probably don't want to use this class directly.  Use TVirtualProtocol
   * instead.
   */
  class TProtocolDefaults : public TProtocol {
  public:
    uint32_t readMessageBegin(std::string& name, TMessageType& messageType, int32_t& seqid) {
      (void)name;
      (void)messageType;
      (void)seqid;
      throw TProtocolException(TProtocolException::NOT_IMPLEMENTED,
                              "this protocol does not support reading (yet).");
    }

    uint32_t readMessageEnd() {
      throw TProtocolException(TProtocolException::NOT_IMPLEMENTED,
                              "this protocol does not support reading (yet).");
    }

    uint32_t readStructBegin(std::string& name) {
      (void)name;
      throw TProtocolException(TProtocolException::NOT_IMPLEMENTED,
                              "this protocol does not support reading (yet).");
    }

    uint32_t readStructEnd() {
      throw TProtocolException(TProtocolException::NOT_IMPLEMENTED,
                              "this protocol does not support reading (yet).");
    }

    uint32_t readFieldBegin(std::string& name, TType& fieldType, int16_t& fieldId) {
      (void)name;
      (void)fieldType;
      (void)fieldId;
      throw TProtocolException(TProtocolException::NOT_IMPLEMENTED,
                              "this protocol does not support reading (yet).");
    }

    uint32_t readFieldEnd() {
      throw TProtocolException(TProtocolException::NOT_IMPLEMENTED,
                              "this protocol does not support reading (yet).");
    }

    uint32_t readMapBegin(TType& keyType, TType& valType, uint32_t& size) {
      (void)keyType;
      (void)valType;
      (void)size;
      throw TProtocolException(TProtocolException::NOT_IMPLEMENTED,
                              "this protocol does not support reading (yet).");
    }

    uint32_t readMapEnd() {
      throw TProtocolException(TProtocolException::NOT_IMPLEMENTED,
                              "this protocol does not support reading (yet).");
    }

    uint32_t readListBegin(TType& elemType, uint32_t& size) {
      (void)elemType;
      (void)size;
      throw TProtocolException(TProtocolException::NOT_IMPLEMENTED,
                              "this protocol does not support reading (yet).");
    }

    uint32_t readListEnd() {
      throw TProtocolException(TProtocolException::NOT_IMPLEMENTED,
                              "this protocol does not support reading (yet).");
    }

    uint32_t readSetBegin(TType& elemType, uint32_t& size) {
      (void)elemType;
      (void)size;
      throw TProtocolException(TProtocolException::NOT_IMPLEMENTED,
                              "this protocol does not support reading (yet).");
    }

    uint32_t readSetEnd() {
      throw TProtocolException(TProtocolException::NOT_IMPLEMENTED,
                              "this protocol does not support reading (yet).");
    }

    uint32_t readBool(bool& value) {
      (void)value;
      throw TProtocolException(TProtocolException::NOT_IMPLEMENTED,
                              "this protocol does not support reading (yet).");
    }

    uint32_t readBool(std::vector<bool>::reference value) {
      (void)value;
      throw TProtocolException(TProtocolException::NOT_IMPLEMENTED,
                              "this protocol does not support reading (yet).");
    }

    uint32_t readByte(int8_t& byte) {
      (void)byte;
      throw TProtocolException(TProtocolException::NOT_IMPLEMENTED,
                              "this protocol does not support reading (yet).");
    }

    uint32_t readI16(int16_t& i16) {
      (void)i16;
      throw TProtocolException(TProtocolException::NOT_IMPLEMENTED,
                              "this protocol does not support reading (yet).");
    }

    uint32_t readI32(int32_t& i32) {
      (void)i32;
      throw TProtocolException(TProtocolException::NOT_IMPLEMENTED,
                              "this protocol does not support reading (yet).");
    }

    uint32_t readI64(int64_t& i64) {
      (void)i64;
      throw TProtocolException(TProtocolException::NOT_IMPLEMENTED,
                              "this protocol does not support reading (yet).");
    }

    uint32_t readDouble(double& dub) {
      (void)dub;
      throw TProtocolException(TProtocolException::NOT_IMPLEMENTED,
                              "this protocol does not support reading (yet).");
    }

    uint32_t readString(std::string& str) {
      (void)str;
      throw TProtocolException(TProtocolException::NOT_IMPLEMENTED,
                              "this protocol does not support reading (yet).");
    }

    uint32_t readBinary(std::string& str) {
      (void)str;
      throw TProtocolException(TProtocolException::NOT_IMPLEMENTED,
                              "this protocol does not support reading (yet).");
    }

    uint32_t writeMessageBegin(const std::string& name,
                              const TMessageType messageType,
                              const int32_t seqid) {
      (void)name;
      (void)messageType;
      (void)seqid;
      throw TProtocolException(TProtocolException::NOT_IMPLEMENTED,
                              "this protocol does not support writing (yet).");
    }

    uint32_t writeMessageEnd() {
      throw TProtocolException(TProtocolException::NOT_IMPLEMENTED,
                              "this protocol does not support writing (yet).");
    }

    uint32_t writeStructBegin(const char* name) {
      (void)name;
      throw TProtocolException(TProtocolException::NOT_IMPLEMENTED,
                              "this protocol does not support writing (yet).");
    }

    uint32_t writeStructEnd() {
      throw TProtocolException(TProtocolException::NOT_IMPLEMENTED,
                              "this protocol does not support writing (yet).");
    }

    uint32_t writeFieldBegin(const char* name, const TType fieldType, const int16_t fieldId) {
      (void)name;
      (void)fieldType;
      (void)fieldId;
      throw TProtocolException(TProtocolException::NOT_IMPLEMENTED,
                              "this protocol does not support writing (yet).");
    }

    uint32_t writeFieldEnd() {
      throw TProtocolException(TProtocolException::NOT_IMPLEMENTED,
                              "this protocol does not support writing (yet).");
    }

    uint32_t writeFieldStop() {
      throw TProtocolException(TProtocolException::NOT_IMPLEMENTED,
                              "this protocol does not support writing (yet).");
    }

    uint32_t writeMapBegin(const TType keyType, const TType valType, const uint32_t size) {
      (void)keyType;
      (void)valType;
      (void)size;
      throw TProtocolException(TProtocolException::NOT_IMPLEMENTED,
                              "this protocol does not support writing (yet).");
    }

    uint32_t writeMapEnd() {
      throw TProtocolException(TProtocolException::NOT_IMPLEMENTED,
                              "this protocol does not support writing (yet).");
    }

    uint32_t writeListBegin(const TType elemType, const uint32_t size) {
      (void)elemType;
      (void)size;
      throw TProtocolException(TProtocolException::NOT_IMPLEMENTED,
                              "this protocol does not support writing (yet).");
    }

    uint32_t writeListEnd() {
      throw TProtocolException(TProtocolException::NOT_IMPLEMENTED,
                              "this protocol does not support writing (yet).");
    }

    uint32_t writeSetBegin(const TType elemType, const uint32_t size) {
      (void)elemType;
      (void)size;
      throw TProtocolException(TProtocolException::NOT_IMPLEMENTED,
                              "this protocol does not support writing (yet).");
    }

    uint32_t writeSetEnd() {
      throw TProtocolException(TProtocolException::NOT_IMPLEMENTED,
                              "this protocol does not support writing (yet).");
    }

    uint32_t writeBool(const bool value) {
      (void)value;
      throw TProtocolException(TProtocolException::NOT_IMPLEMENTED,
                              "this protocol does not support writing (yet).");
    }

    uint32_t writeByte(const int8_t byte) {
      (void)byte;
      throw TProtocolException(TProtocolException::NOT_IMPLEMENTED,
                              "this protocol does not support writing (yet).");
    }

    uint32_t writeI16(const int16_t i16) {
      (void)i16;
      throw TProtocolException(TProtocolException::NOT_IMPLEMENTED,
                              "this protocol does not support writing (yet).");
    }

    uint32_t writeI32(const int32_t i32) {
      (void)i32;
      throw TProtocolException(TProtocolException::NOT_IMPLEMENTED,
                              "this protocol does not support writing (yet).");
    }

    uint32_t writeI64(const int64_t i64) {
      (void)i64;
      throw TProtocolException(TProtocolException::NOT_IMPLEMENTED,
                              "this protocol does not support writing (yet).");
    }

    uint32_t writeDouble(const double dub) {
      (void)dub;
      throw TProtocolException(TProtocolException::NOT_IMPLEMENTED,
                              "this protocol does not support writing (yet).");
    }

    uint32_t writeString(const std::string& str) {
      (void)str;
      throw TProtocolException(TProtocolException::NOT_IMPLEMENTED,
                              "this protocol does not support writing (yet).");
    }

    uint32_t writeBinary(const std::string& str) {
      (void)str;
      throw TProtocolException(TProtocolException::NOT_IMPLEMENTED,
                              "this protocol does not support writing (yet).");
    }

    uint32_t skip(TType type) { return ::my_thrift::apache::thrift::protocol::skip(*this, type); }

  protected:
    TProtocolDefaults(std::shared_ptr<TTransport> ptrans) : TProtocol(ptrans) {}
  };

  /**
   * Concrete TProtocol classes should inherit from TVirtualProtocol
   * so they don't have to manually override virtual methods.
   */
  template <class Protocol_, class Super_ = TProtocolDefaults>
  class TVirtualProtocol : public Super_ {
  public:
    /**
     * Writing functions.
     */

    uint32_t writeMessageBegin_virt(const std::string& name,
                                            const TMessageType messageType,
                                            const int32_t seqid) override {
      return static_cast<Protocol_*>(this)->writeMessageBegin(name, messageType, seqid);
    }

    uint32_t writeMessageEnd_virt() override {
      return static_cast<Protocol_*>(this)->writeMessageEnd();
    }

    uint32_t writeStructBegin_virt(const char* name) override {
      return static_cast<Protocol_*>(this)->writeStructBegin(name);
    }

    uint32_t writeStructEnd_virt() override { return static_cast<Protocol_*>(this)->writeStructEnd(); }

    uint32_t writeFieldBegin_virt(const char* name,
                                          const TType fieldType,
                                          const int16_t fieldId) override {
      return static_cast<Protocol_*>(this)->writeFieldBegin(name, fieldType, fieldId);
    }

    uint32_t writeFieldEnd_virt() override { return static_cast<Protocol_*>(this)->writeFieldEnd(); }

    uint32_t writeFieldStop_virt() override { return static_cast<Protocol_*>(this)->writeFieldStop(); }

    uint32_t writeMapBegin_virt(const TType keyType,
                                        const TType valType,
                                        const uint32_t size) override {
      return static_cast<Protocol_*>(this)->writeMapBegin(keyType, valType, size);
    }

    uint32_t writeMapEnd_virt() override { return static_cast<Protocol_*>(this)->writeMapEnd(); }

    uint32_t writeListBegin_virt(const TType elemType, const uint32_t size) override {
      return static_cast<Protocol_*>(this)->writeListBegin(elemType, size);
    }

    uint32_t writeListEnd_virt() override { return static_cast<Protocol_*>(this)->writeListEnd(); }

    uint32_t writeSetBegin_virt(const TType elemType, const uint32_t size) override {
      return static_cast<Protocol_*>(this)->writeSetBegin(elemType, size);
    }

    uint32_t writeSetEnd_virt() override { return static_cast<Protocol_*>(this)->writeSetEnd(); }

    uint32_t writeBool_virt(const bool value) override {
      return static_cast<Protocol_*>(this)->writeBool(value);
    }

    uint32_t writeByte_virt(const int8_t byte) override {
      return static_cast<Protocol_*>(this)->writeByte(byte);
    }

    uint32_t writeI16_virt(const int16_t i16) override {
      return static_cast<Protocol_*>(this)->writeI16(i16);
    }

    uint32_t writeI32_virt(const int32_t i32) override {
      return static_cast<Protocol_*>(this)->writeI32(i32);
    }

    uint32_t writeI64_virt(const int64_t i64) override {
      return static_cast<Protocol_*>(this)->writeI64(i64);
    }

    uint32_t writeDouble_virt(const double dub) override {
      return static_cast<Protocol_*>(this)->writeDouble(dub);
    }

    uint32_t writeString_virt(const std::string& str) override {
      return static_cast<Protocol_*>(this)->writeString(str);
    }

    uint32_t writeBinary_virt(const std::string& str) override {
      return static_cast<Protocol_*>(this)->writeBinary(str);
    }

    uint32_t writeUUID_virt(const ::apache::thrift::TUuid& uuid) override {
      return static_cast<Protocol_*>(this)->writeUUID(uuid);
    }

    /**
     * Reading functions
     */

    uint32_t readMessageBegin_virt(std::string& name,
                                          TMessageType& messageType,
                                          int32_t& seqid) override {
      return static_cast<Protocol_*>(this)->readMessageBegin(name, messageType, seqid); // links to TBinaryProtocol.tcc:208 
    }

    uint32_t readMessageEnd_virt() override { return static_cast<Protocol_*>(this)->readMessageEnd(); }

    uint32_t readStructBegin_virt(std::string& name) override {
      return static_cast<Protocol_*>(this)->readStructBegin(name);
    }

    uint32_t readStructEnd_virt() override { return static_cast<Protocol_*>(this)->readStructEnd(); }

    uint32_t readFieldBegin_virt(std::string& name, TType& fieldType, int16_t& fieldId) override {
      return static_cast<Protocol_*>(this)->readFieldBegin(name, fieldType, fieldId);
    }

    uint32_t readFieldEnd_virt() override { return static_cast<Protocol_*>(this)->readFieldEnd(); }

    uint32_t readMapBegin_virt(TType& keyType, TType& valType, uint32_t& size) override {
      return static_cast<Protocol_*>(this)->readMapBegin(keyType, valType, size);
    }

    uint32_t readMapEnd_virt() override { return static_cast<Protocol_*>(this)->readMapEnd(); }

    uint32_t readListBegin_virt(TType& elemType, uint32_t& size) override {
      return static_cast<Protocol_*>(this)->readListBegin(elemType, size);
    }

    uint32_t readListEnd_virt() override { return static_cast<Protocol_*>(this)->readListEnd(); }

    uint32_t readSetBegin_virt(TType& elemType, uint32_t& size) override {
      return static_cast<Protocol_*>(this)->readSetBegin(elemType, size);
    }

    uint32_t readSetEnd_virt() override { return static_cast<Protocol_*>(this)->readSetEnd(); }

    uint32_t readBool_virt(bool& value) override {
      return static_cast<Protocol_*>(this)->readBool(value);
    }

    uint32_t readBool_virt(std::vector<bool>::reference value) override {
      return static_cast<Protocol_*>(this)->readBool(value);
    }

    uint32_t readByte_virt(int8_t& byte) override {
      return static_cast<Protocol_*>(this)->readByte(byte);
    }

    uint32_t readI16_virt(int16_t& i16) override {
      return static_cast<Protocol_*>(this)->readI16(i16);
    }

    uint32_t readI32_virt(int32_t& i32) override {
      return static_cast<Protocol_*>(this)->readI32(i32);
    }

    uint32_t readI64_virt(int64_t& i64) override {
      return static_cast<Protocol_*>(this)->readI64(i64);
    }

    uint32_t readDouble_virt(double& dub) override {
      return static_cast<Protocol_*>(this)->readDouble(dub);
    }

    uint32_t readString_virt(std::string& str) override {
      return static_cast<Protocol_*>(this)->readString(str);
    }

    uint32_t readBinary_virt(std::string& str) override {
      return static_cast<Protocol_*>(this)->readBinary(str);
    }

    uint32_t readUUID_virt(::apache::thrift::TUuid& uuid) override {
      return static_cast<Protocol_*>(this)->readUUID(uuid);
    }

    uint32_t skip_virt(TType type) override { return static_cast<Protocol_*>(this)->skip(type); }

    /*
    * Provide a default skip() implementation that uses non-virtual read
    * methods.
    *
    * Note: subclasses that use TVirtualProtocol to derive from another protocol
    * implementation (i.e., not TProtocolDefaults) should beware that this may
    * override any non-default skip() implementation provided by the parent
    * transport class.  They may need to explicitly redefine skip() to call the
    * correct parent implementation, if desired.
    */
    uint32_t skip(TType type) {
      auto* const prot = static_cast<Protocol_*>(this);
      return ::my_thrift::apache::thrift::protocol::skip(*prot, type);
    }

    /*
    * Provide a default readBool() implementation for use with
    * std::vector<bool>, that behaves the same as reading into a normal bool.
    *
    * Subclasses can override this if desired, but there normally shouldn't
    * be a need to.
    */
    uint32_t readBool(std::vector<bool>::reference value) {
      bool b = false;
      uint32_t ret = static_cast<Protocol_*>(this)->readBool(b);
      value = b;
      return ret;
    }
    using Super_::readBool; // so we don't hide readBool(bool&)

  protected:
    TVirtualProtocol(std::shared_ptr<TTransport> ptrans) : Super_(ptrans) {}
  };
  
  // ***TBinaryProtocol.h***
  /**
   * The default binary protocol for thrift. Writes all data in a very basic
   * binary format, essentially just spitting out the raw bytes.
   *
   */
  template <class Transport_, class ByteOrder_ = TNetworkBigEndian>
  class TBinaryProtocolT : public TVirtualProtocol<TBinaryProtocolT<Transport_, ByteOrder_> > {
  public:
    static const int32_t VERSION_MASK = ((int32_t)0xffff0000);
    static const int32_t VERSION_1 = ((int32_t)0x80010000);
    // VERSION_2 (0x80020000) was taken by TDenseProtocol (which has since been removed)

    TBinaryProtocolT(std::shared_ptr<Transport_> trans)
      : TVirtualProtocol<TBinaryProtocolT<Transport_, ByteOrder_> >(trans),
        trans_(trans.get()),
        string_limit_(0),
        container_limit_(0),
        strict_read_(false),
        strict_write_(true) {}

    TBinaryProtocolT(std::shared_ptr<Transport_> trans,
                    int32_t string_limit,
                    int32_t container_limit,
                    bool strict_read,
                    bool strict_write)
      : TVirtualProtocol<TBinaryProtocolT<Transport_, ByteOrder_> >(trans),
        trans_(trans.get()),
        string_limit_(string_limit),
        container_limit_(container_limit),
        strict_read_(strict_read),
        strict_write_(strict_write) {}

    void setStringSizeLimit(int32_t string_limit) { string_limit_ = string_limit; }

    void setContainerSizeLimit(int32_t container_limit) { container_limit_ = container_limit; }

    void setStrict(bool strict_read, bool strict_write) {
      strict_read_ = strict_read;
      strict_write_ = strict_write;
    }

    /**
     * Writing functions.
     */

    /*ol*/ uint32_t writeMessageBegin(const std::string& name,
                                      const TMessageType messageType,
                                      const int32_t seqid);

    /*ol*/ uint32_t writeMessageEnd();

    inline uint32_t writeStructBegin(const char* name);

    inline uint32_t writeStructEnd();

    inline uint32_t writeFieldBegin(const char* name, const TType fieldType, const int16_t fieldId);

    inline uint32_t writeFieldEnd();

    inline uint32_t writeFieldStop();

    inline uint32_t writeMapBegin(const TType keyType, const TType valType, const uint32_t size);

    inline uint32_t writeMapEnd();

    inline uint32_t writeListBegin(const TType elemType, const uint32_t size);

    inline uint32_t writeListEnd();

    inline uint32_t writeSetBegin(const TType elemType, const uint32_t size);

    inline uint32_t writeSetEnd();

    inline uint32_t writeBool(const bool value);

    inline uint32_t writeByte(const int8_t byte);

    inline uint32_t writeI16(const int16_t i16);

    inline uint32_t writeI32(const int32_t i32);

    inline uint32_t writeI64(const int64_t i64);

    inline uint32_t writeDouble(const double dub);

    template <typename StrType>
    inline uint32_t writeString(const StrType& str);

    inline uint32_t writeBinary(const std::string& str);

    inline uint32_t writeUUID(const ::apache::thrift::TUuid& uuid);

    /**
     * Reading functions
     */

    /*ol*/ uint32_t readMessageBegin(std::string& name, TMessageType& messageType, int32_t& seqid);

    /*ol*/ uint32_t readMessageEnd();

    inline uint32_t readStructBegin(std::string& name);

    inline uint32_t readStructEnd();

    inline uint32_t readFieldBegin(std::string& name, TType& fieldType, int16_t& fieldId);

    inline uint32_t readFieldEnd();

    inline uint32_t readMapBegin(TType& keyType, TType& valType, uint32_t& size);

    inline uint32_t readMapEnd();

    inline uint32_t readListBegin(TType& elemType, uint32_t& size);

    inline uint32_t readListEnd();

    inline uint32_t readSetBegin(TType& elemType, uint32_t& size);

    inline uint32_t readSetEnd();

    inline uint32_t readBool(bool& value);
    // Provide the default readBool() implementation for std::vector<bool>
    using TVirtualProtocol<TBinaryProtocolT<Transport_, ByteOrder_> >::readBool;

    inline uint32_t readByte(int8_t& byte);

    inline uint32_t readI16(int16_t& i16);

    inline uint32_t readI32(int32_t& i32);

    inline uint32_t readI64(int64_t& i64);

    inline uint32_t readDouble(double& dub);

    template <typename StrType>
    inline uint32_t readString(StrType& str);

    inline uint32_t readBinary(std::string& str);

    inline uint32_t readUUID(::apache::thrift::TUuid& uuid);

    int getMinSerializedSize(TType type) override;

    void checkReadBytesAvailable(TSet& set) override
    {
        trans_->checkReadBytesAvailable(set.size_ * getMinSerializedSize(set.elemType_));
    }

    void checkReadBytesAvailable(TList& list) override
    {
        trans_->checkReadBytesAvailable(list.size_ * getMinSerializedSize(list.elemType_));
    }

    void checkReadBytesAvailable(TMap& map) override
    {
        int elmSize = getMinSerializedSize(map.keyType_) + getMinSerializedSize(map.valueType_);
        trans_->checkReadBytesAvailable(map.size_ * elmSize);
    }

  protected:
    template <typename StrType>
    uint32_t readStringBody(StrType& str, int32_t sz);

    Transport_* trans_;

    int32_t string_limit_;
    int32_t container_limit_;

    // Enforce presence of version identifier
    bool strict_read_;
    bool strict_write_;
  };

  typedef TBinaryProtocolT<TTransport> TBinaryProtocol;
  typedef TBinaryProtocolT<TTransport, TNetworkLittleEndian> TLEBinaryProtocol;

  /**
   * Constructs binary protocol handlers
   */
  template <class Transport_, class ByteOrder_ = TNetworkBigEndian>
  class TBinaryProtocolFactoryT : public TProtocolFactory {
  public:
    TBinaryProtocolFactoryT()
      : string_limit_(0), container_limit_(0), strict_read_(false), strict_write_(true) {}

    TBinaryProtocolFactoryT(int32_t string_limit,
                            int32_t container_limit,
                            bool strict_read,
                            bool strict_write)
      : string_limit_(string_limit),
        container_limit_(container_limit),
        strict_read_(strict_read),
        strict_write_(strict_write) {}

    ~TBinaryProtocolFactoryT() override = default;

    void setStringSizeLimit(int32_t string_limit) { string_limit_ = string_limit; }

    void setContainerSizeLimit(int32_t container_limit) { container_limit_ = container_limit; }

    void setStrict(bool strict_read, bool strict_write) {
      strict_read_ = strict_read;
      strict_write_ = strict_write;
    }

    std::shared_ptr<TProtocol> getProtocol(std::shared_ptr<TTransport> trans) override {
      std::shared_ptr<Transport_> specific_trans = std::dynamic_pointer_cast<Transport_>(trans);
      TProtocol* prot;
      if (specific_trans) {
        prot = new TBinaryProtocolT<Transport_, ByteOrder_>(specific_trans,
                                                            string_limit_,
                                                            container_limit_,
                                                            strict_read_,
                                                            strict_write_);
      } else {
        prot = new TBinaryProtocolT<TTransport, ByteOrder_>(trans,
                                                            string_limit_,
                                                            container_limit_,
                                                            strict_read_,
                                                            strict_write_);
      }

      return std::shared_ptr<TProtocol>(prot);
    }

  private:
    int32_t string_limit_;
    int32_t container_limit_;
    bool strict_read_;
    bool strict_write_;
  };

  typedef TBinaryProtocolFactoryT<TTransport> TBinaryProtocolFactory;
  typedef TBinaryProtocolFactoryT<TTransport, TNetworkLittleEndian> TLEBinaryProtocolFactory;

  // ***TBinaryProtocol.tcc***
  template <class Transport_, class ByteOrder_>
  uint32_t TBinaryProtocolT<Transport_, ByteOrder_>::writeMessageBegin(const std::string& name,
                                                                      const TMessageType messageType,
                                                                      const int32_t seqid) {
    if (this->strict_write_) {
      int32_t version = (VERSION_1) | ((int32_t)messageType);
      uint32_t wsize = 0;
      wsize += writeI32(version);
      wsize += writeString(name);
      wsize += writeI32(seqid);
      return wsize;
    } else {
      uint32_t wsize = 0;
      wsize += writeString(name);
      wsize += writeByte((int8_t)messageType);
      wsize += writeI32(seqid);
      return wsize;
    }
  }

  template <class Transport_, class ByteOrder_>
  uint32_t TBinaryProtocolT<Transport_, ByteOrder_>::writeMessageEnd() {
    return 0;
  }

  template <class Transport_, class ByteOrder_>
  uint32_t TBinaryProtocolT<Transport_, ByteOrder_>::writeStructBegin(const char* name) {
    (void)name;
    return 0;
  }

  template <class Transport_, class ByteOrder_>
  uint32_t TBinaryProtocolT<Transport_, ByteOrder_>::writeStructEnd() {
    return 0;
  }

  template <class Transport_, class ByteOrder_>
  uint32_t TBinaryProtocolT<Transport_, ByteOrder_>::writeFieldBegin(const char* name,
                                                                    const TType fieldType,
                                                                    const int16_t fieldId) {
    (void)name;
    uint32_t wsize = 0;
    wsize += writeByte((int8_t)fieldType);
    wsize += writeI16(fieldId);
    return wsize;
  }

  template <class Transport_, class ByteOrder_>
  uint32_t TBinaryProtocolT<Transport_, ByteOrder_>::writeFieldEnd() {
    return 0;
  }

  template <class Transport_, class ByteOrder_>
  uint32_t TBinaryProtocolT<Transport_, ByteOrder_>::writeFieldStop() {
    return writeByte((int8_t)T_STOP);
  }

  template <class Transport_, class ByteOrder_>
  uint32_t TBinaryProtocolT<Transport_, ByteOrder_>::writeMapBegin(const TType keyType,
                                                                  const TType valType,
                                                                  const uint32_t size) {
    uint32_t wsize = 0;
    wsize += writeByte((int8_t)keyType);
    wsize += writeByte((int8_t)valType);
    wsize += writeI32((int32_t)size);
    return wsize;
  }

  template <class Transport_, class ByteOrder_>
  uint32_t TBinaryProtocolT<Transport_, ByteOrder_>::writeMapEnd() {
    return 0;
  }

  template <class Transport_, class ByteOrder_>
  uint32_t TBinaryProtocolT<Transport_, ByteOrder_>::writeListBegin(const TType elemType,
                                                                    const uint32_t size) {
    uint32_t wsize = 0;
    wsize += writeByte((int8_t)elemType);
    wsize += writeI32((int32_t)size);
    return wsize;
  }

  template <class Transport_, class ByteOrder_>
  uint32_t TBinaryProtocolT<Transport_, ByteOrder_>::writeListEnd() {
    return 0;
  }

  template <class Transport_, class ByteOrder_>
  uint32_t TBinaryProtocolT<Transport_, ByteOrder_>::writeSetBegin(const TType elemType,
                                                                  const uint32_t size) {
    uint32_t wsize = 0;
    wsize += writeByte((int8_t)elemType);
    wsize += writeI32((int32_t)size);
    return wsize;
  }

  template <class Transport_, class ByteOrder_>
  uint32_t TBinaryProtocolT<Transport_, ByteOrder_>::writeSetEnd() {
    return 0;
  }

  template <class Transport_, class ByteOrder_>
  uint32_t TBinaryProtocolT<Transport_, ByteOrder_>::writeBool(const bool value) {
    uint8_t tmp = value ? 1 : 0;
    this->trans_->write(&tmp, 1);
    return 1;
  }

  template <class Transport_, class ByteOrder_>
  uint32_t TBinaryProtocolT<Transport_, ByteOrder_>::writeByte(const int8_t byte) {
    this->trans_->write((uint8_t*)&byte, 1);
    return 1;
  }

  template <class Transport_, class ByteOrder_>
  uint32_t TBinaryProtocolT<Transport_, ByteOrder_>::writeI16(const int16_t i16) {
    auto net = (int16_t)ByteOrder_::toWire16(i16);
    this->trans_->write((uint8_t*)&net, 2);
    return 2;
  }

  template <class Transport_, class ByteOrder_>
  uint32_t TBinaryProtocolT<Transport_, ByteOrder_>::writeI32(const int32_t i32) {
    auto net = (int32_t)ByteOrder_::toWire32(i32);
    this->trans_->write((uint8_t*)&net, 4);
    return 4;
  }

  template <class Transport_, class ByteOrder_>
  uint32_t TBinaryProtocolT<Transport_, ByteOrder_>::writeI64(const int64_t i64) {
    auto net = (int64_t)ByteOrder_::toWire64(i64);
    this->trans_->write((uint8_t*)&net, 8);
    return 8;
  }

  template <class Transport_, class ByteOrder_>
  uint32_t TBinaryProtocolT<Transport_, ByteOrder_>::writeDouble(const double dub) {
    static_assert(sizeof(double) == sizeof(uint64_t), "sizeof(double) == sizeof(uint64_t)");
    static_assert(std::numeric_limits<double>::is_iec559, "std::numeric_limits<double>::is_iec559");

    auto bits = bitwise_cast<uint64_t>(dub);
    bits = ByteOrder_::toWire64(bits);
    this->trans_->write((uint8_t*)&bits, 8);
    return 8;
  }

  template <class Transport_, class ByteOrder_>
  template <typename StrType>
  uint32_t TBinaryProtocolT<Transport_, ByteOrder_>::writeString(const StrType& str) {
    if (str.size() > static_cast<size_t>((std::numeric_limits<int32_t>::max)()))
      throw TProtocolException(TProtocolException::SIZE_LIMIT);
    auto size = static_cast<uint32_t>(str.size());
    uint32_t result = writeI32((int32_t)size);
    if (size > 0) {
      this->trans_->write((uint8_t*)str.data(), size);
    }
    return result + size;
  }

  template <class Transport_, class ByteOrder_>
  uint32_t TBinaryProtocolT<Transport_, ByteOrder_>::writeBinary(const std::string& str) {
    return TBinaryProtocolT<Transport_, ByteOrder_>::writeString(str);
  }

  template <class Transport_, class ByteOrder_>
  uint32_t TBinaryProtocolT<Transport_, ByteOrder_>::writeUUID(const ::apache::thrift::TUuid& uuid) {
    // TODO: Consider endian swapping, see lib/delphi/src/Thrift.Utils.pas:377
    this->trans_->write(uuid.data(), uuid.size());
    return 16;
  }

  /**
   * Reading functions
   */

  template <class Transport_, class ByteOrder_>
  uint32_t TBinaryProtocolT<Transport_, ByteOrder_>::readMessageBegin(std::string& name,
                                                                      TMessageType& messageType,
                                                                      int32_t& seqid) { // links from TVirtualProcol.h:404
    uint32_t result = 0;
    int32_t sz;
    result += readI32(sz); // links to TBinaryProtocol.tcc:393

    if (sz < 0) {
      // Check for correct version number
      int32_t version = sz & VERSION_MASK;
      if (version != VERSION_1) {
        throw TProtocolException(TProtocolException::BAD_VERSION, "Bad version identifier");
      }
      messageType = (TMessageType)(sz & 0x000000ff);
      result += readString(name);
      result += readI32(seqid);
    } else {
      if (this->strict_read_) {
        throw TProtocolException(TProtocolException::BAD_VERSION,
                                "No version identifier... old protocol client in strict mode?");
      } else {
        // Handle pre-versioned input
        int8_t type;
        result += readStringBody(name, sz);
        result += readByte(type);
        messageType = (TMessageType)type;
        result += readI32(seqid);
      }
    }
    return result;
  }

  template <class Transport_, class ByteOrder_>
  uint32_t TBinaryProtocolT<Transport_, ByteOrder_>::readMessageEnd() {
    return 0;
  }

  template <class Transport_, class ByteOrder_>
  uint32_t TBinaryProtocolT<Transport_, ByteOrder_>::readStructBegin(std::string& name) {
    name = "";
    return 0;
  }

  template <class Transport_, class ByteOrder_>
  uint32_t TBinaryProtocolT<Transport_, ByteOrder_>::readStructEnd() {
    return 0;
  }

  template <class Transport_, class ByteOrder_>
  uint32_t TBinaryProtocolT<Transport_, ByteOrder_>::readFieldBegin(std::string& name,
                                                                    TType& fieldType,
                                                                    int16_t& fieldId) {
    (void)name;
    uint32_t result = 0;
    int8_t type;
    result += readByte(type);
    fieldType = (TType)type;
    if (fieldType == T_STOP) {
      fieldId = 0;
      return result;
    }
    result += readI16(fieldId);
    return result;
  }

  template <class Transport_, class ByteOrder_>
  uint32_t TBinaryProtocolT<Transport_, ByteOrder_>::readFieldEnd() {
    return 0;
  }

  template <class Transport_, class ByteOrder_>
  uint32_t TBinaryProtocolT<Transport_, ByteOrder_>::readMapBegin(TType& keyType,
                                                                  TType& valType,
                                                                  uint32_t& size) {
    int8_t k, v;
    uint32_t result = 0;
    int32_t sizei;
    result += readByte(k);
    keyType = (TType)k;
    result += readByte(v);
    valType = (TType)v;
    result += readI32(sizei);
    if (sizei < 0) {
      throw TProtocolException(TProtocolException::NEGATIVE_SIZE);
    } else if (this->container_limit_ && sizei > this->container_limit_) {
      throw TProtocolException(TProtocolException::SIZE_LIMIT);
    }
    size = (uint32_t)sizei;

    TMap map(keyType, valType, size);
    checkReadBytesAvailable(map);

    return result;
  }

  template <class Transport_, class ByteOrder_>
  uint32_t TBinaryProtocolT<Transport_, ByteOrder_>::readMapEnd() {
    return 0;
  }

  template <class Transport_, class ByteOrder_>
  uint32_t TBinaryProtocolT<Transport_, ByteOrder_>::readListBegin(TType& elemType, uint32_t& size) {
    int8_t e;
    uint32_t result = 0;
    int32_t sizei;
    result += readByte(e);
    elemType = (TType)e;
    result += readI32(sizei);
    if (sizei < 0) {
      throw TProtocolException(TProtocolException::NEGATIVE_SIZE);
    } else if (this->container_limit_ && sizei > this->container_limit_) {
      throw TProtocolException(TProtocolException::SIZE_LIMIT);
    }
    size = (uint32_t)sizei;

    TList list(elemType, size);
    checkReadBytesAvailable(list);

    return result;
  }

  template <class Transport_, class ByteOrder_>
  uint32_t TBinaryProtocolT<Transport_, ByteOrder_>::readListEnd() {
    return 0;
  }

  template <class Transport_, class ByteOrder_>
  uint32_t TBinaryProtocolT<Transport_, ByteOrder_>::readSetBegin(TType& elemType, uint32_t& size) {
    int8_t e;
    uint32_t result = 0;
    int32_t sizei;
    result += readByte(e);
    elemType = (TType)e;
    result += readI32(sizei);
    if (sizei < 0) {
      throw TProtocolException(TProtocolException::NEGATIVE_SIZE);
    } else if (this->container_limit_ && sizei > this->container_limit_) {
      throw TProtocolException(TProtocolException::SIZE_LIMIT);
    }
    size = (uint32_t)sizei;

    TSet set(elemType, size);
    checkReadBytesAvailable(set);

    return result;
  }

  template <class Transport_, class ByteOrder_>
  uint32_t TBinaryProtocolT<Transport_, ByteOrder_>::readSetEnd() {
    return 0;
  }

  template <class Transport_, class ByteOrder_>
  uint32_t TBinaryProtocolT<Transport_, ByteOrder_>::readBool(bool& value) {
    uint8_t b[1];
    this->trans_->readAll(b, 1); 
    value = *(int8_t*)b != 0;
    return 1;
  }

  template <class Transport_, class ByteOrder_>
  uint32_t TBinaryProtocolT<Transport_, ByteOrder_>::readByte(int8_t& byte) {
    uint8_t b[1];
    this->trans_->readAll(b, 1);
    byte = *(int8_t*)b;
    return 1;
  }

  template <class Transport_, class ByteOrder_>
  uint32_t TBinaryProtocolT<Transport_, ByteOrder_>::readI16(int16_t& i16) {
    union bytes {
      uint8_t b[2];
      int16_t all;
    } theBytes;
    this->trans_->readAll(theBytes.b, 2);
    i16 = (int16_t)ByteOrder_::fromWire16(theBytes.all);
    return 2;
  }

  template <class Transport_, class ByteOrder_>
  uint32_t TBinaryProtocolT<Transport_, ByteOrder_>::readI32(int32_t& i32) {
    union bytes {
      uint8_t b[4];
      int32_t all;
    } theBytes;
    this->trans_->readAll(theBytes.b, 4); //links to TTransport.h:131 (readAll)
    i32 = (int32_t)ByteOrder_::fromWire32(theBytes.all);
    return 4;
  }

  template <class Transport_, class ByteOrder_>
  uint32_t TBinaryProtocolT<Transport_, ByteOrder_>::readI64(int64_t& i64) {
    union bytes {
      uint8_t b[8];
      int64_t all;
    } theBytes;
    this->trans_->readAll(theBytes.b, 8);
    i64 = (int64_t)ByteOrder_::fromWire64(theBytes.all);
    return 8;
  }

  template <class Transport_, class ByteOrder_>
  uint32_t TBinaryProtocolT<Transport_, ByteOrder_>::readDouble(double& dub) {
    static_assert(sizeof(double) == sizeof(uint64_t), "sizeof(double) == sizeof(uint64_t)");
    static_assert(std::numeric_limits<double>::is_iec559, "std::numeric_limits<double>::is_iec559");

    union bytes {
      uint8_t b[8];
      uint64_t all;
    } theBytes;
    this->trans_->readAll(theBytes.b, 8);
    theBytes.all = ByteOrder_::fromWire64(theBytes.all);
    dub = bitwise_cast<double>(theBytes.all);
    return 8;
  }

  template <class Transport_, class ByteOrder_>
  template <typename StrType>
  uint32_t TBinaryProtocolT<Transport_, ByteOrder_>::readString(StrType& str) {
    uint32_t result;
    int32_t size;
    result = readI32(size);
    return result + readStringBody(str, size);
  }

  template <class Transport_, class ByteOrder_>
  uint32_t TBinaryProtocolT<Transport_, ByteOrder_>::readBinary(std::string& str) {
    return TBinaryProtocolT<Transport_, ByteOrder_>::readString(str);
  }

  template <class Transport_, class ByteOrder_>
  uint32_t TBinaryProtocolT<Transport_, ByteOrder_>::readUUID(::apache::thrift::TUuid& uuid) {
    this->trans_->readAll(uuid.begin(), uuid.size());
    return 16;
  }

  template <class Transport_, class ByteOrder_>
  template <typename StrType>
  uint32_t TBinaryProtocolT<Transport_, ByteOrder_>::readStringBody(StrType& str, int32_t size) {
    uint32_t result = 0;

    // Catch error cases
    if (size < 0) {
      throw TProtocolException(TProtocolException::NEGATIVE_SIZE);
    }
    if (this->string_limit_ > 0 && size > this->string_limit_) {
      throw TProtocolException(TProtocolException::SIZE_LIMIT);
    }

    // Catch empty string case
    if (size == 0) {
      str.clear();
      return result;
    }

    // Try to borrow first
    uint32_t got = size;
    const uint8_t* borrow_buf = this->trans_->borrow(nullptr, &got);
    if (borrow_buf) {
      str.assign((const char*)borrow_buf, size);
      this->trans_->consume(size);
      return size;
    }

    str.resize(size);
    this->trans_->readAll(reinterpret_cast<uint8_t*>(&str[0]), size);
    return (uint32_t)size;
  }

  // Return the minimum number of bytes a type will consume on the wire
  template <class Transport_, class ByteOrder_>
  int TBinaryProtocolT<Transport_, ByteOrder_>::getMinSerializedSize(TType type)
  {
    switch (type)
    {
        case T_STOP: return 0;
        case T_VOID: return 0;
        case T_BOOL: return sizeof(int8_t);
        case T_BYTE: return sizeof(int8_t);
        case T_DOUBLE: return sizeof(double);
        case T_I16: return sizeof(short);
        case T_I32: return sizeof(int);
        case T_I64: return sizeof(long);
        case T_STRING: return sizeof(int);  // string length
        case T_STRUCT: return 0;  // empty struct
        case T_MAP: return sizeof(int);  // element count
        case T_SET: return sizeof(int);  // element count
        case T_LIST: return sizeof(int);  // element count
        default: throw TProtocolException(TProtocolException::UNKNOWN, "unrecognized type code");
    }
  }
  
  } // my_thrift::apache::thrift::protocol
  
  // ***TApplicationException.h***
  class TApplicationException : public ::apache::thrift::TException {
  public:
    /**
     * Error codes for the various types of exceptions.
     */
    enum TApplicationExceptionType {
      UNKNOWN = 0,
      UNKNOWN_METHOD = 1,
      INVALID_MESSAGE_TYPE = 2,
      WRONG_METHOD_NAME = 3,
      BAD_SEQUENCE_ID = 4,
      MISSING_RESULT = 5,
      INTERNAL_ERROR = 6,
      PROTOCOL_ERROR = 7,
      INVALID_TRANSFORM = 8,
      INVALID_PROTOCOL = 9,
      UNSUPPORTED_CLIENT_TYPE = 10
    };

    TApplicationException() : ::apache::thrift::TException(), type_(UNKNOWN) {}

    TApplicationException(TApplicationExceptionType type) : ::apache::thrift::TException(), type_(type) {}

    TApplicationException(const std::string& message) : ::apache::thrift::TException(message), type_(UNKNOWN) {}

    TApplicationException(TApplicationExceptionType type, const std::string& message)
      : ::apache::thrift::TException(message), type_(type) {}

    ~TApplicationException() noexcept override = default;

    /**
     * Returns an error code that provides information about the type of error
     * that has occurred.
     *
     * @return Error code
     */
    TApplicationExceptionType getType() const { return type_; }

    const char* what() const noexcept override {
      if (message_.empty()) {
        switch (type_) {
        case UNKNOWN:
          return "TApplicationException: Unknown application exception";
        case UNKNOWN_METHOD:
          return "TApplicationException: Unknown method";
        case INVALID_MESSAGE_TYPE:
          return "TApplicationException: Invalid message type";
        case WRONG_METHOD_NAME:
          return "TApplicationException: Wrong method name";
        case BAD_SEQUENCE_ID:
          return "TApplicationException: Bad sequence identifier";
        case MISSING_RESULT:
          return "TApplicationException: Missing result";
        case INTERNAL_ERROR:
          return "TApplicationException: Internal error";
        case PROTOCOL_ERROR:
          return "TApplicationException: Protocol error";
        case INVALID_TRANSFORM:
          return "TApplicationException: Invalid transform";
        case INVALID_PROTOCOL:
          return "TApplicationException: Invalid protocol";
        case UNSUPPORTED_CLIENT_TYPE:
          return "TApplicationException: Unsupported client type";
        default:
          return "TApplicationException: (Invalid exception type)";
        };
      } else {
        return message_.c_str();
      }
    }

    uint32_t read(protocol::TProtocol* iprot);
    uint32_t write(protocol::TProtocol* oprot) const;

  protected:
    /**
     * Error code
     */
    TApplicationExceptionType type_;
  };
  
  // ***TApplicationException.cpp***
  uint32_t TApplicationException::read(my_thrift::apache::thrift::protocol::TProtocol* iprot) {
    uint32_t xfer = 0;
    std::string fname;
    my_thrift::apache::thrift::protocol::TType ftype;
    int16_t fid;

    xfer += iprot->readStructBegin(fname);

    while (true) {
      xfer += iprot->readFieldBegin(fname, ftype, fid);
      if (ftype == my_thrift::apache::thrift::protocol::T_STOP) {
        break;
      }
      switch (fid) {
      case 1:
        if (ftype == my_thrift::apache::thrift::protocol::T_STRING) {
          xfer += iprot->readString(message_);
        } else {
          xfer += iprot->skip(ftype);
        }
        break;
      case 2:
        if (ftype == my_thrift::apache::thrift::protocol::T_I32) {
          int32_t type;
          xfer += iprot->readI32(type);
          type_ = (TApplicationExceptionType)type;
        } else {
          xfer += iprot->skip(ftype);
        }
        break;
      default:
        xfer += iprot->skip(ftype);
        break;
      }
      xfer += iprot->readFieldEnd();
    }

    xfer += iprot->readStructEnd();
    return xfer;
  }

  uint32_t TApplicationException::write(my_thrift::apache::thrift::protocol::TProtocol* oprot) const {
    uint32_t xfer = 0;
    xfer += oprot->writeStructBegin("TApplicationException");
    xfer += oprot->writeFieldBegin("message", my_thrift::apache::thrift::protocol::T_STRING, 1);
    xfer += oprot->writeString(message_);
    xfer += oprot->writeFieldEnd();
    xfer += oprot->writeFieldBegin("type", my_thrift::apache::thrift::protocol::T_I32, 2);
    xfer += oprot->writeI32(type_);
    xfer += oprot->writeFieldEnd();
    xfer += oprot->writeFieldStop();
    xfer += oprot->writeStructEnd();
    return xfer;
  }
  // ***TBase.h***
  class TBase {
    public:
      virtual ~TBase() = default;
      virtual uint32_t read(protocol::TProtocol* iprot) = 0;
      virtual uint32_t write(protocol::TProtocol* oprot) const = 0;
  };

  // ***TProcesss.h***
  /**
   * Virtual interface class that can handle events from the processor. To
   * use this you should subclass it and implement the methods that you care
   * about. Your subclass can also store local data that you may care about,
   * such as additional "arguments" to these methods (stored in the object
   * instance's state).
   */
  class TProcessorEventHandler {
  public:
    virtual ~TProcessorEventHandler() = default;

    /**
     * Called before calling other callback methods.
     * Expected to return some sort of context object.
     * The return value is passed to all other callbacks
     * for that function invocation.
     */
    virtual void* getContext(const char* fn_name, void* serverContext) {
      (void)fn_name;
      (void)serverContext;
      return nullptr;
    }

    /**
     * Expected to free resources associated with a context.
     */
    virtual void freeContext(void* ctx, const char* fn_name) {
      (void)ctx;
      (void)fn_name;
    }

    /**
     * Called before reading arguments.
     */
    virtual void preRead(void* ctx, const char* fn_name) {
      (void)ctx;
      (void)fn_name;
    }

    /**
     * Called between reading arguments and calling the handler.
     */
    virtual void postRead(void* ctx, const char* fn_name, uint32_t bytes) {
      (void)ctx;
      (void)fn_name;
      (void)bytes;
    }

    /**
     * Called between calling the handler and writing the response.
     */
    virtual void preWrite(void* ctx, const char* fn_name) {
      (void)ctx;
      (void)fn_name;
    }

    /**
     * Called after writing the response.
     */
    virtual void postWrite(void* ctx, const char* fn_name, uint32_t bytes) {
      (void)ctx;
      (void)fn_name;
      (void)bytes;
    }

    /**
     * Called when an async function call completes successfully.
     */
    virtual void asyncComplete(void* ctx, const char* fn_name) {
      (void)ctx;
      (void)fn_name;
    }

    /**
     * Called if the handler throws an undeclared exception.
     */
    virtual void handlerError(void* ctx, const char* fn_name) {
      (void)ctx;
      (void)fn_name;
    }

  protected:
    TProcessorEventHandler() = default;
  };

  /**
   * A helper class used by the generated code to free each context.
   */
  class TProcessorContextFreer {
  public:
    TProcessorContextFreer(TProcessorEventHandler* handler, void* context, const char* method)
      : handler_(handler), context_(context), method_(method) {}
    ~TProcessorContextFreer() {
      if (handler_ != nullptr)
        handler_->freeContext(context_, method_);
    }
    void unregister() { handler_ = nullptr; }

  private:
    my_thrift::apache::thrift::TProcessorEventHandler* handler_;
    void* context_;
    const char* method_;
  };

  /**
   * A processor is a generic object that acts upon two streams of data, one
   * an input and the other an output. The definition of this object is loose,
   * though the typical case is for some sort of server that either generates
   * responses to an input stream or forwards data from one pipe onto another.
   *
   */
  class TProcessor {
  public:
    virtual ~TProcessor() = default;

    virtual bool process(std::shared_ptr<my_thrift::apache::thrift::protocol::TProtocol> in,
                        std::shared_ptr<my_thrift::apache::thrift::protocol::TProtocol> out,
                        void* connectionContext) = 0;

    bool process(std::shared_ptr<my_thrift::apache::thrift::protocol::TProtocol> io, void* connectionContext) {
      return process(io, io, connectionContext);
    }

    std::shared_ptr<TProcessorEventHandler> getEventHandler() const { return eventHandler_; }

    void setEventHandler(std::shared_ptr<TProcessorEventHandler> eventHandler) {
      eventHandler_ = eventHandler;
    }

  protected:
    TProcessor() = default;

    std::shared_ptr<TProcessorEventHandler> eventHandler_;
  };

  /**
   * This is a helper class to allow std::shared_ptr to be used with handler
   * pointers returned by the generated handler factories.
   *
   * The handler factory classes generated by the thrift compiler return raw
   * pointers, and factory->releaseHandler() must be called when the handler is
   * no longer needed.
   *
   * A ReleaseHandler object can be instantiated and passed as the second
   * parameter to a shared_ptr, so that factory->releaseHandler() will be called
   * when the object is no longer needed, instead of deleting the pointer.
   */
  template <typename HandlerFactory_>
  class ReleaseHandler {
  public:
    ReleaseHandler(const std::shared_ptr<HandlerFactory_>& handlerFactory)
      : handlerFactory_(handlerFactory) {}

    void operator()(typename HandlerFactory_::Handler* handler) {
      if (handler) {
        handlerFactory_->releaseHandler(handler);
      }
    }

  private:
    std::shared_ptr<HandlerFactory_> handlerFactory_;
  };

  struct TConnectionInfo {
    // The input and output protocols
    std::shared_ptr<protocol::TProtocol> input;
    std::shared_ptr<protocol::TProtocol> output;
    // The underlying transport used for the connection
    // This is the transport that was returned by TServerTransport::accept(),
    // and it may be different than the transport pointed to by the input and
    // output protocols.
    std::shared_ptr<transport::TTransport> transport;
  };

  class TProcessorFactory {
  public:
    virtual ~TProcessorFactory() = default;

    /**
     * Get the TProcessor to use for a particular connection.
     *
     * This method is always invoked in the same thread that the connection was
     * accepted on.  This generally means that this call does not need to be
     * thread safe, as it will always be invoked from a single thread.
     */
    virtual std::shared_ptr<TProcessor> getProcessor(const TConnectionInfo& connInfo) = 0;
  };

  class TSingletonProcessorFactory : public TProcessorFactory {
  public:
    TSingletonProcessorFactory(std::shared_ptr<TProcessor> processor) : processor_(processor) {}

    std::shared_ptr<TProcessor> getProcessor(const TConnectionInfo&) override { return processor_; }

  private:
    std::shared_ptr<TProcessor> processor_;
  };

  // ***TDispatchProcessor.h***
  /**
   * Non-templatized version of TDispatchProcessor, that doesn't bother trying to
   * perform a dynamic_cast.
   */
  class TDispatchProcessor : public TProcessor {
  public:
    bool process(std::shared_ptr<my_thrift::apache::thrift::protocol::TProtocol> in,
                        std::shared_ptr<my_thrift::apache::thrift::protocol::TProtocol> out,
                        void* connectionContext) override {
      std::string fname;
      protocol::TMessageType mtype;
      int32_t seqid;
      in->readMessageBegin(fname, mtype, seqid); // links to TProtocol.h:445 

      if (mtype != protocol::T_CALL && mtype != protocol::T_ONEWAY) {
        GlobalOutput.printf("received invalid message type %d from client", mtype);
        return false;
      }

      return dispatchCall(in.get(), out.get(), fname, seqid, connectionContext); // links to MemcachedService.cpp
    }

  protected:
    virtual bool dispatchCall(my_thrift::apache::thrift::protocol::TProtocol* in,
                              my_thrift::apache::thrift::protocol::TProtocol* out,
                              const std::string& fname,
                              int32_t seqid,
                              void* callContext) = 0;
  };

  namespace async {
  
  // ***TConcurrentClientSyncInfo.h***
  class TConcurrentClientSyncInfo;

  class TConcurrentSendSentry {
  public:
    explicit TConcurrentSendSentry(TConcurrentClientSyncInfo* sync);
    virtual ~TConcurrentSendSentry();

    void commit();

  private:
    TConcurrentClientSyncInfo& sync_;
    bool committed_;
  };

  class TConcurrentRecvSentry {
  public:
    TConcurrentRecvSentry(TConcurrentClientSyncInfo* sync, int32_t seqid);
    virtual ~TConcurrentRecvSentry();

    void commit();

  private:
    TConcurrentClientSyncInfo& sync_;
    int32_t seqid_;
    bool committed_;
  };

  class TConcurrentClientSyncInfo {
  private: // typedefs
    typedef std::shared_ptr< ::my_thrift::apache::thrift::concurrency::Monitor> MonitorPtr;
    typedef std::map<int32_t, MonitorPtr> MonitorMap;

  public:
    TConcurrentClientSyncInfo();

    int32_t generateSeqId();

    bool getPending(std::string& fname,
                    ::my_thrift::apache::thrift::protocol::TMessageType& mtype,
                    int32_t& rseqid); /* requires readMutex_ */

    void updatePending(const std::string& fname,
                      ::my_thrift::apache::thrift::protocol::TMessageType mtype,
                      int32_t rseqid); /* requires readMutex_ */

    void waitForWork(int32_t seqid); /* requires readMutex_ */

    ::my_thrift::apache::thrift::concurrency::Mutex& getReadMutex() { return readMutex_; }
    ::my_thrift::apache::thrift::concurrency::Mutex& getWriteMutex() { return writeMutex_; }

  private: // constants
    enum { MONITOR_CACHE_SIZE = 10 };

  private: // functions
    MonitorPtr newMonitor_(
        const ::my_thrift::apache::thrift::concurrency::Guard& seqidGuard); /* requires seqidMutex_ */
    void deleteMonitor_(const ::my_thrift::apache::thrift::concurrency::Guard& seqidGuard, MonitorPtr& m);
        /*noexcept*/ /* requires seqidMutex_ */
    void wakeupAnyone_(
        const ::my_thrift::apache::thrift::concurrency::Guard& seqidGuard);           /* requires seqidMutex_ */
    void markBad_(const ::my_thrift::apache::thrift::concurrency::Guard& seqidGuard); /* requires seqidMutex_ */
    void throwBadSeqId_();
    void throwDeadConnection_();

  private: // data members
    volatile bool stop_;

    ::my_thrift::apache::thrift::concurrency::Mutex seqidMutex_;
    // begin seqidMutex_ protected members
    int32_t nextseqid_;
    MonitorMap seqidToMonitorMap_;
    std::vector<MonitorPtr> freeMonitors_;
    // end seqidMutex_ protected members

    ::my_thrift::apache::thrift::concurrency::Mutex writeMutex_;

    ::my_thrift::apache::thrift::concurrency::Mutex readMutex_;
    // begin readMutex_ protected members
    bool recvPending_;
    bool wakeupSomeone_;
    int32_t seqidPending_;
    std::string fnamePending_;
    ::my_thrift::apache::thrift::protocol::TMessageType mtypePending_;
    // end readMutex_ protected members

    friend class TConcurrentSendSentry;
    friend class TConcurrentRecvSentry;
  };

  // ***TConcurrentClientSyncInfo.h***
  using namespace ::my_thrift::apache::thrift::concurrency;

  TConcurrentClientSyncInfo::TConcurrentClientSyncInfo() :
    stop_(false),
    seqidMutex_(),
    // test rollover all the time
    nextseqid_((std::numeric_limits<int32_t>::max)()-10),
    seqidToMonitorMap_(),
    freeMonitors_(),
    writeMutex_(),
    readMutex_(),
    recvPending_(false),
    wakeupSomeone_(false),
    seqidPending_(0),
    fnamePending_(),
    mtypePending_(::my_thrift::apache::thrift::protocol::T_CALL)
  {
    freeMonitors_.reserve(MONITOR_CACHE_SIZE);
  }

  bool TConcurrentClientSyncInfo::getPending(
    std::string &fname,
    ::my_thrift::apache::thrift::protocol::TMessageType &mtype,
    int32_t &rseqid)
  {
    if(stop_)
      throwDeadConnection_();
    wakeupSomeone_ = false;
    if(recvPending_)
    {
      recvPending_ = false;
      rseqid = seqidPending_;
      fname  = fnamePending_;
      mtype  = mtypePending_;
      return true;
    }
    return false;
  }

  void TConcurrentClientSyncInfo::updatePending(
    const std::string &fname,
    ::my_thrift::apache::thrift::protocol::TMessageType mtype,
    int32_t rseqid)
  {
    recvPending_ = true;
    seqidPending_ = rseqid;
    fnamePending_ = fname;
    mtypePending_ = mtype;
    MonitorPtr monitor;
    {
      Guard seqidGuard(seqidMutex_);
      auto i = seqidToMonitorMap_.find(rseqid);
      if(i == seqidToMonitorMap_.end())
        throwBadSeqId_();
      monitor = i->second;
    }
    monitor->notify();
  }

  void TConcurrentClientSyncInfo::waitForWork(int32_t seqid)
  {
    MonitorPtr m;
    {
      Guard seqidGuard(seqidMutex_);
      m = seqidToMonitorMap_[seqid];
    }
    while(true)
    {
      // be very careful about setting state in this loop that affects waking up.  You may exit
      // this function, attempt to grab some work, and someone else could have beaten you (or not
      // left) the read mutex, and that will put you right back in this loop, with the mangled
      // state you left behind.
      if(stop_)
        throwDeadConnection_();
      if(wakeupSomeone_)
        return;
      if(recvPending_ && seqidPending_ == seqid)
        return;
      m->waitForever();
    }
  }

  void TConcurrentClientSyncInfo::throwBadSeqId_()
  {
    throw apache::thrift::TApplicationException(
      TApplicationException::BAD_SEQUENCE_ID,
      "server sent a bad seqid");
  }

  void TConcurrentClientSyncInfo::throwDeadConnection_()
  {
    throw apache::thrift::transport::TTransportException(
      apache::thrift::transport::TTransportException::NOT_OPEN,
      "this client died on another thread, and is now in an unusable state");
  }

  void TConcurrentClientSyncInfo::wakeupAnyone_(const Guard &)
  {
    wakeupSomeone_ = true;
    if(!seqidToMonitorMap_.empty())
    {
      // The monitor map maps integers to monitors.  Larger integers are more recent
      // messages.  Since this is ordered, it means that the last element is the most recent.
      // We are trying to guess which thread will have its message complete next, so we are picking
      // the most recent. The oldest message is likely to be some polling, long lived message.
      // If we guess right, the thread we wake up will handle the message that comes in.
      // If we guess wrong, the thread we wake up will hand off the work to the correct thread,
      // costing us an extra context switch.
      seqidToMonitorMap_.rbegin()->second->notify();
    }
  }

  void TConcurrentClientSyncInfo::markBad_(const Guard &)
  {
    wakeupSomeone_ = true;
    stop_ = true;
    for(auto & i : seqidToMonitorMap_)
      i.second->notify();
  }

  TConcurrentClientSyncInfo::MonitorPtr
  TConcurrentClientSyncInfo::newMonitor_(const Guard &)
  {
    if(freeMonitors_.empty())
      return std::make_shared<Monitor>(&readMutex_);
    MonitorPtr retval;
    //swapping to avoid an atomic operation
    retval.swap(freeMonitors_.back());
    freeMonitors_.pop_back();
    return retval;
  }

  void TConcurrentClientSyncInfo::deleteMonitor_(
    const Guard &,
    TConcurrentClientSyncInfo::MonitorPtr &m) /*noexcept*/
  {
    if(freeMonitors_.size() > MONITOR_CACHE_SIZE)
    {
      m.reset();
      return;
    }
    //freeMonitors_ was reserved up to MONITOR_CACHE_SIZE in the ctor,
    //so this shouldn't throw
    freeMonitors_.push_back(TConcurrentClientSyncInfo::MonitorPtr());
    //swapping to avoid an atomic operation
    m.swap(freeMonitors_.back());
  }

  int32_t TConcurrentClientSyncInfo::generateSeqId()
  {
    Guard seqidGuard(seqidMutex_);
    if(stop_)
      throwDeadConnection_();

    if(!seqidToMonitorMap_.empty())
      if(nextseqid_ == seqidToMonitorMap_.begin()->first)
        throw apache::thrift::TApplicationException(
          TApplicationException::BAD_SEQUENCE_ID,
          "about to repeat a seqid");
    int32_t newSeqId = nextseqid_;
    if (nextseqid_ == (std::numeric_limits<int32_t>::max)())
      nextseqid_ = (std::numeric_limits<int32_t>::min)();
    else
      ++nextseqid_;
    seqidToMonitorMap_[newSeqId] = newMonitor_(seqidGuard);
    return newSeqId;
  }

  TConcurrentRecvSentry::TConcurrentRecvSentry(TConcurrentClientSyncInfo *sync, int32_t seqid) :
    sync_(*sync),
    seqid_(seqid),
    committed_(false)
  {
    sync_.getReadMutex().lock();
  }

  TConcurrentRecvSentry::~TConcurrentRecvSentry()
  {
    {
      Guard seqidGuard(sync_.seqidMutex_);
      sync_.deleteMonitor_(seqidGuard, sync_.seqidToMonitorMap_[seqid_]);

      sync_.seqidToMonitorMap_.erase(seqid_);
      if(committed_)
        sync_.wakeupAnyone_(seqidGuard);
      else
        sync_.markBad_(seqidGuard);
    }
    sync_.getReadMutex().unlock();
  }

  void TConcurrentRecvSentry::commit()
  {
    committed_ = true;
  }

  TConcurrentSendSentry::TConcurrentSendSentry(TConcurrentClientSyncInfo *sync) :
    sync_(*sync),
    committed_(false)
  {
    sync_.getWriteMutex().lock();
  }

  TConcurrentSendSentry::~TConcurrentSendSentry()
  {
    if(!committed_)
    {
      Guard seqidGuard(sync_.seqidMutex_);
      sync_.markBad_(seqidGuard);
    }
    sync_.getWriteMutex().unlock();
  }

  void TConcurrentSendSentry::commit()
  {
    committed_ = true;
  }
  }

  namespace server {
  
  // ***TServer.h***
  using my_thrift::apache::thrift::TProcessor;
  using my_thrift::apache::thrift::protocol::TBinaryProtocolFactory;
  using my_thrift::apache::thrift::protocol::TProtocol;
  using my_thrift::apache::thrift::protocol::TProtocolFactory;
  using my_thrift::apache::thrift::transport::TServerTransport;
  using my_thrift::apache::thrift::transport::TTransport;
  using my_thrift::apache::thrift::transport::TTransportFactory;

  /**
   * Virtual interface class that can handle events from the server core. To
   * use this you should subclass it and implement the methods that you care
   * about. Your subclass can also store local data that you may care about,
   * such as additional "arguments" to these methods (stored in the object
   * instance's state).
   */
  class TServerEventHandler {
  public:
    virtual ~TServerEventHandler() = default;

    /**
     * Called before the server begins.
     */
    virtual void preServe() {}

    /**
     * Called when a new client has connected and is about to being processing.
     */
    virtual void* createContext(std::shared_ptr<TProtocol> input,
                                std::shared_ptr<TProtocol> output) {
      (void)input;
      (void)output;
      return nullptr;
    }

    /**
     * Called when a client has finished request-handling to delete server
     * context.
     */
    virtual void deleteContext(void* serverContext,
                              std::shared_ptr<TProtocol> input,
                              std::shared_ptr<TProtocol> output) {
      (void)serverContext;
      (void)input;
      (void)output;
    }

    /**
     * Called when a client is about to call the processor.
     */
    virtual void processContext(void* serverContext, std::shared_ptr<TTransport> transport) {
      (void)serverContext;
      (void)transport;
    }

  protected:
    /**
     * Prevent direct instantiation.
     */
    TServerEventHandler() = default;
  };

  /**
   * Thrift server.
   *
   */
  class TServer : public concurrency::Runnable {
  public:
    ~TServer() override = default;

    virtual void serve() = 0;

    virtual void stop() {}

    // Allows running the server as a Runnable thread
    void run() override { serve(); }

    std::shared_ptr<TProcessorFactory> getProcessorFactory() { return processorFactory_; }

    std::shared_ptr<TServerTransport> getServerTransport() { return serverTransport_; }

    std::shared_ptr<TTransportFactory> getInputTransportFactory() { return inputTransportFactory_; }

    std::shared_ptr<TTransportFactory> getOutputTransportFactory() {
      return outputTransportFactory_;
    }

    std::shared_ptr<TProtocolFactory> getInputProtocolFactory() { return inputProtocolFactory_; }

    std::shared_ptr<TProtocolFactory> getOutputProtocolFactory() { return outputProtocolFactory_; }

    std::shared_ptr<TServerEventHandler> getEventHandler() { return eventHandler_; }

  protected:
    TServer(const std::shared_ptr<TProcessorFactory>& processorFactory)
      : processorFactory_(processorFactory) {
      setInputTransportFactory(std::shared_ptr<TTransportFactory>(new TTransportFactory()));
      setOutputTransportFactory(std::shared_ptr<TTransportFactory>(new TTransportFactory()));
      setInputProtocolFactory(std::shared_ptr<TProtocolFactory>(new TBinaryProtocolFactory()));
      setOutputProtocolFactory(std::shared_ptr<TProtocolFactory>(new TBinaryProtocolFactory()));
    }

    TServer(const std::shared_ptr<TProcessor>& processor)
      : processorFactory_(new TSingletonProcessorFactory(processor)) {
      setInputTransportFactory(std::shared_ptr<TTransportFactory>(new TTransportFactory()));
      setOutputTransportFactory(std::shared_ptr<TTransportFactory>(new TTransportFactory()));
      setInputProtocolFactory(std::shared_ptr<TProtocolFactory>(new TBinaryProtocolFactory()));
      setOutputProtocolFactory(std::shared_ptr<TProtocolFactory>(new TBinaryProtocolFactory()));
    }

    TServer(const std::shared_ptr<TProcessorFactory>& processorFactory,
            const std::shared_ptr<TServerTransport>& serverTransport)
      : processorFactory_(processorFactory), serverTransport_(serverTransport) {
      setInputTransportFactory(std::shared_ptr<TTransportFactory>(new TTransportFactory()));
      setOutputTransportFactory(std::shared_ptr<TTransportFactory>(new TTransportFactory()));
      setInputProtocolFactory(std::shared_ptr<TProtocolFactory>(new TBinaryProtocolFactory()));
      setOutputProtocolFactory(std::shared_ptr<TProtocolFactory>(new TBinaryProtocolFactory()));
    }

    TServer(const std::shared_ptr<TProcessor>& processor,
            const std::shared_ptr<TServerTransport>& serverTransport)
      : processorFactory_(new TSingletonProcessorFactory(processor)),
        serverTransport_(serverTransport) {
      setInputTransportFactory(std::shared_ptr<TTransportFactory>(new TTransportFactory()));
      setOutputTransportFactory(std::shared_ptr<TTransportFactory>(new TTransportFactory()));
      setInputProtocolFactory(std::shared_ptr<TProtocolFactory>(new TBinaryProtocolFactory()));
      setOutputProtocolFactory(std::shared_ptr<TProtocolFactory>(new TBinaryProtocolFactory()));
    }

    TServer(const std::shared_ptr<TProcessorFactory>& processorFactory,
            const std::shared_ptr<TServerTransport>& serverTransport,
            const std::shared_ptr<TTransportFactory>& transportFactory,
            const std::shared_ptr<TProtocolFactory>& protocolFactory)
      : processorFactory_(processorFactory),
        serverTransport_(serverTransport),
        inputTransportFactory_(transportFactory),
        outputTransportFactory_(transportFactory),
        inputProtocolFactory_(protocolFactory),
        outputProtocolFactory_(protocolFactory) {}

    TServer(const std::shared_ptr<TProcessor>& processor,
            const std::shared_ptr<TServerTransport>& serverTransport,
            const std::shared_ptr<TTransportFactory>& transportFactory,
            const std::shared_ptr<TProtocolFactory>& protocolFactory)
      : processorFactory_(new TSingletonProcessorFactory(processor)),
        serverTransport_(serverTransport),
        inputTransportFactory_(transportFactory),
        outputTransportFactory_(transportFactory),
        inputProtocolFactory_(protocolFactory),
        outputProtocolFactory_(protocolFactory) {}

    TServer(const std::shared_ptr<TProcessorFactory>& processorFactory,
            const std::shared_ptr<TServerTransport>& serverTransport,
            const std::shared_ptr<TTransportFactory>& inputTransportFactory,
            const std::shared_ptr<TTransportFactory>& outputTransportFactory,
            const std::shared_ptr<TProtocolFactory>& inputProtocolFactory,
            const std::shared_ptr<TProtocolFactory>& outputProtocolFactory)
      : processorFactory_(processorFactory),
        serverTransport_(serverTransport),
        inputTransportFactory_(inputTransportFactory),
        outputTransportFactory_(outputTransportFactory),
        inputProtocolFactory_(inputProtocolFactory),
        outputProtocolFactory_(outputProtocolFactory) {}

    TServer(const std::shared_ptr<TProcessor>& processor,
            const std::shared_ptr<TServerTransport>& serverTransport,
            const std::shared_ptr<TTransportFactory>& inputTransportFactory,
            const std::shared_ptr<TTransportFactory>& outputTransportFactory,
            const std::shared_ptr<TProtocolFactory>& inputProtocolFactory,
            const std::shared_ptr<TProtocolFactory>& outputProtocolFactory)
      : processorFactory_(new TSingletonProcessorFactory(processor)),
        serverTransport_(serverTransport),
        inputTransportFactory_(inputTransportFactory),
        outputTransportFactory_(outputTransportFactory),
        inputProtocolFactory_(inputProtocolFactory),
        outputProtocolFactory_(outputProtocolFactory) {}

    /**
     * Get a TProcessor to handle calls on a particular connection.
     *
     * This method should only be called once per connection (never once per
     * call).  This allows the TProcessorFactory to return a different processor
     * for each connection if it desires.
     */
    std::shared_ptr<TProcessor> getProcessor(std::shared_ptr<TProtocol> inputProtocol,
                                              std::shared_ptr<TProtocol> outputProtocol,
                                              std::shared_ptr<TTransport> transport) {
      TConnectionInfo connInfo;
      connInfo.input = inputProtocol;
      connInfo.output = outputProtocol;
      connInfo.transport = transport;
      return processorFactory_->getProcessor(connInfo); //calls TProcessor.h:221
    }

    // Class variables
    std::shared_ptr<TProcessorFactory> processorFactory_;
    std::shared_ptr<TServerTransport> serverTransport_;

    std::shared_ptr<TTransportFactory> inputTransportFactory_;
    std::shared_ptr<TTransportFactory> outputTransportFactory_;

    std::shared_ptr<TProtocolFactory> inputProtocolFactory_;
    std::shared_ptr<TProtocolFactory> outputProtocolFactory_;

    std::shared_ptr<TServerEventHandler> eventHandler_;

  public:
    void setInputTransportFactory(std::shared_ptr<TTransportFactory> inputTransportFactory) {
      inputTransportFactory_ = inputTransportFactory;
    }

    void setOutputTransportFactory(std::shared_ptr<TTransportFactory> outputTransportFactory) {
      outputTransportFactory_ = outputTransportFactory;
    }

    void setInputProtocolFactory(std::shared_ptr<TProtocolFactory> inputProtocolFactory) {
      inputProtocolFactory_ = inputProtocolFactory;
    }

    void setOutputProtocolFactory(std::shared_ptr<TProtocolFactory> outputProtocolFactory) {
      outputProtocolFactory_ = outputProtocolFactory;
    }

    void setServerEventHandler(std::shared_ptr<TServerEventHandler> eventHandler) {
      eventHandler_ = eventHandler;
    }
  };

  /**
   * Helper function to increase the max file descriptors limit
   * for the current process and all of its children.
   * By default, tries to increase it to as much as 2^24.
   */
  // #ifdef HAVE_SYS_RESOURCE_H
  // int increase_max_fds(int max_fds = (1 << 24));
  // #endif

  // ***TServer.cpp***
#ifdef HAVE_SYS_RESOURCE_H
  int increase_max_fds(int max_fds = (1 << 24)) {
    struct rlimit fdmaxrl;

    for (fdmaxrl.rlim_cur = max_fds, fdmaxrl.rlim_max = max_fds;
        max_fds && (setrlimit(RLIMIT_NOFILE, &fdmaxrl) < 0);
        fdmaxrl.rlim_cur = max_fds, fdmaxrl.rlim_max = max_fds) {
      max_fds /= 2;
    }

    return static_cast<int>(fdmaxrl.rlim_cur);
  }
#endif
  // ***TConnectedClient.h***
  /**
   * This represents a client connected to a TServer.  The
   * processing loop for a client must provide some required
   * functionality common to all implementations so it is
   * encapsulated here.
   */

  class TConnectedClient : public my_thrift::apache::thrift::concurrency::Runnable {
  public:
    /**
     * Constructor.
     *
     * @param[in] processor      the TProcessor
     * @param[in] inputProtocol  the input TProtocol
     * @param[in] outputProtocol the output TProtocol
     * @param[in] eventHandler   the server event handler
     * @param[in] client         the TTransport representing the client
     */
    TConnectedClient(
        const std::shared_ptr<my_thrift::apache::thrift::TProcessor>& processor,
        const std::shared_ptr<my_thrift::apache::thrift::protocol::TProtocol>& inputProtocol,
        const std::shared_ptr<my_thrift::apache::thrift::protocol::TProtocol>& outputProtocol,
        const std::shared_ptr<my_thrift::apache::thrift::server::TServerEventHandler>& eventHandler,
        const std::shared_ptr<my_thrift::apache::thrift::transport::TTransport>& client);

    /**
     * Destructor.
     */
    ~TConnectedClient() override;

    /**
     * Drive the client until it is done.
     * The client processing loop is:
     *
     * [optional] call eventHandler->createContext once
     * [optional] call eventHandler->processContext per request
     *            call processor->process per request
     *              handle expected transport exceptions:
     *                END_OF_FILE means the client is gone
     *                INTERRUPTED means the client was interrupted
     *                            by TServerTransport::interruptChildren()
     *              handle unexpected transport exceptions by logging
     *              handle standard exceptions by logging
     *              handle unexpected exceptions by logging
     *            cleanup()
     */
    void run() override /* override */;

  // protected:
    /**
     * Cleanup after a client.  This happens if the client disconnects,
     * or if the server is stopped, or if an exception occurs.
     *
     * The cleanup processing is:
     * [optional] call eventHandler->deleteContext once
     *            close the inputProtocol's TTransport
     *            close the outputProtocol's TTransport
     *            close the client
     */
    virtual void cleanup();

  private:
    std::shared_ptr<my_thrift::apache::thrift::TProcessor> processor_;
    std::shared_ptr<my_thrift::apache::thrift::protocol::TProtocol> inputProtocol_;
    std::shared_ptr<my_thrift::apache::thrift::protocol::TProtocol> outputProtocol_;
    std::shared_ptr<my_thrift::apache::thrift::server::TServerEventHandler> eventHandler_;
    std::shared_ptr<my_thrift::apache::thrift::transport::TTransport> client_;

    /**
     * Context acquired from the eventHandler_ if one exists.
     */
    void* opaqueContext_;
  };
  // ***TConnectedClient.cpp***
  using my_thrift::apache::thrift::TProcessor;
  using my_thrift::apache::thrift::protocol::TProtocol;
  using my_thrift::apache::thrift::server::TServerEventHandler;
  using my_thrift::apache::thrift::transport::TTransport;
  using my_thrift::apache::thrift::transport::TTransportException;
  using std::shared_ptr;
  using std::string;

  TConnectedClient::TConnectedClient(const shared_ptr<TProcessor>& processor,
                                    const shared_ptr<TProtocol>& inputProtocol,
                                    const shared_ptr<TProtocol>& outputProtocol,
                                    const shared_ptr<TServerEventHandler>& eventHandler,
                                    const shared_ptr<TTransport>& client)

    : processor_(processor),
      inputProtocol_(inputProtocol),
      outputProtocol_(outputProtocol),
      eventHandler_(eventHandler),
      client_(client),
      opaqueContext_(nullptr) {
  }

  TConnectedClient::~TConnectedClient() = default;

  void TConnectedClient::run() {
    if (eventHandler_) {
      opaqueContext_ = eventHandler_->createContext(inputProtocol_, outputProtocol_);
    }

    for (bool done = false; !done;) {
      if (eventHandler_) {
        eventHandler_->processContext(opaqueContext_, client_);
      }

      try {
        if (!processor_->process(inputProtocol_, outputProtocol_, opaqueContext_)) { //to TDispatchProcessor.h:108
          break;
        }
      } catch (const TTransportException& ttx) {
        switch (ttx.getType()) {
          case TTransportException::END_OF_FILE:
          case TTransportException::INTERRUPTED:
          case TTransportException::TIMED_OUT:
            // Client disconnected or was interrupted or did not respond within the receive timeout.
            // No logging needed.  Done.
            done = true;
            break;

          default: {
            // All other transport exceptions are logged.
            // State of connection is unknown.  Done.
            string errStr = string("TConnectedClient died: ") + ttx.what();
            GlobalOutput(errStr.c_str());
            done = true;
            break;
          }
        }
      } catch (const ::apache::thrift::TException& tex) {
        string errStr = string("TConnectedClient processing exception: ") + tex.what();
        GlobalOutput(errStr.c_str());
        // Disconnect from client, because we could not process the message.
        done = true;
      }
    }

    cleanup();
  }

  void TConnectedClient::cleanup() {
    if (eventHandler_) {
      eventHandler_->deleteContext(opaqueContext_, inputProtocol_, outputProtocol_);
    }

    try {
      inputProtocol_->getTransport()->close();
    } catch (const TTransportException& ttx) {
      string errStr = string("TConnectedClient input close failed: ") + ttx.what();
      GlobalOutput(errStr.c_str());
    }

    try {
      outputProtocol_->getTransport()->close();
    } catch (const TTransportException& ttx) {
      string errStr = string("TConnectedClient output close failed: ") + ttx.what();
      GlobalOutput(errStr.c_str());
    }

    try {
      client_->close();
    } catch (const TTransportException& ttx) {
      string errStr = string("TConnectedClient client close failed: ") + ttx.what();
      GlobalOutput(errStr.c_str());
    }
  }
  // ***TServerFramework.h***
  /**
   * TServerFramework provides a single consolidated processing loop for
   * servers.  By having a single processing loop, behavior between servers
   * is more predictable and maintenance cost is lowered.  Implementations
   * of TServerFramework must provide a method to deal with a client that
   * connects and one that disconnects.
   *
   * While this functionality could be rolled directly into TServer, and
   * probably should be, it would break the TServer interface contract so
   * to maintain backwards compatibility for third party servers, no TServers
   * were harmed in the making of this class.
   */
  class TServerFramework : public TServer {
  public:
    TServerFramework(
        const std::shared_ptr<my_thrift::apache::thrift::TProcessorFactory>& processorFactory,
        const std::shared_ptr<my_thrift::apache::thrift::transport::TServerTransport>& serverTransport,
        const std::shared_ptr<my_thrift::apache::thrift::transport::TTransportFactory>& transportFactory,
        const std::shared_ptr<my_thrift::apache::thrift::protocol::TProtocolFactory>& protocolFactory);

    TServerFramework(
        const std::shared_ptr<my_thrift::apache::thrift::TProcessor>& processor,
        const std::shared_ptr<my_thrift::apache::thrift::transport::TServerTransport>& serverTransport,
        const std::shared_ptr<my_thrift::apache::thrift::transport::TTransportFactory>& transportFactory,
        const std::shared_ptr<my_thrift::apache::thrift::protocol::TProtocolFactory>& protocolFactory);

    TServerFramework(
        const std::shared_ptr<my_thrift::apache::thrift::TProcessorFactory>& processorFactory,
        const std::shared_ptr<my_thrift::apache::thrift::transport::TServerTransport>& serverTransport,
        const std::shared_ptr<my_thrift::apache::thrift::transport::TTransportFactory>& inputTransportFactory,
        const std::shared_ptr<my_thrift::apache::thrift::transport::TTransportFactory>& outputTransportFactory,
        const std::shared_ptr<my_thrift::apache::thrift::protocol::TProtocolFactory>& inputProtocolFactory,
        const std::shared_ptr<my_thrift::apache::thrift::protocol::TProtocolFactory>& outputProtocolFactory);

    TServerFramework(
        const std::shared_ptr<my_thrift::apache::thrift::TProcessor>& processor,
        const std::shared_ptr<my_thrift::apache::thrift::transport::TServerTransport>& serverTransport,
        const std::shared_ptr<my_thrift::apache::thrift::transport::TTransportFactory>& inputTransportFactory,
        const std::shared_ptr<my_thrift::apache::thrift::transport::TTransportFactory>& outputTransportFactory,
        const std::shared_ptr<my_thrift::apache::thrift::protocol::TProtocolFactory>& inputProtocolFactory,
        const std::shared_ptr<my_thrift::apache::thrift::protocol::TProtocolFactory>& outputProtocolFactory);

    ~TServerFramework() override;

    /**
     * Accept clients from the TServerTransport and add them for processing.
     * Call stop() on another thread to interrupt processing
     * and return control to the caller.
     * Post-conditions (return guarantees):
     *   The serverTransport will be closed.
     */
    void serve() override;

    /**
     * Interrupt serve() so that it meets post-conditions and returns.
     */
    void stop() override;

    /**
     * Get the concurrent client limit.
     * \returns the concurrent client limit
     */
    virtual int64_t getConcurrentClientLimit() const;

    /**
     * Get the number of currently connected clients.
     * \returns the number of currently connected clients
     */
    virtual int64_t getConcurrentClientCount() const;

    /**
     * Get the highest number of concurrent clients.
     * \returns the highest number of concurrent clients
     */
    virtual int64_t getConcurrentClientCountHWM() const;

    /**
     * Set the concurrent client limit.  This can be changed while
     * the server is serving however it will not necessarily be
     * enforced until the next client is accepted and added.  If the
     * limit is lowered below the number of connected clients, no
     * action is taken to disconnect the clients.
     * The default value used if this is not called is INT64_MAX.
     * \param[in]  newLimit  the new limit of concurrent clients
     * \throws std::invalid_argument if newLimit is less than 1
     */
    virtual void setConcurrentClientLimit(int64_t newLimit);

  protected:
    /**
     * A client has connected.  The implementation is responsible for managing the
     * lifetime of the client object.  This is called during the serve() thread,
     * therefore a failure to return quickly will result in new client connection
     * delays.
     *
     * \param[in]  pClient  the newly connected client
     */
    virtual void onClientConnected(const std::shared_ptr<TConnectedClient>& pClient) = 0;

    /**
     * A client has disconnected.
     * When called:
     *   The server no longer tracks the client.
     *   The client TTransport has already been closed.
     *   The implementation must not delete the pointer.
     *
     * \param[in]  pClient  the disconnected client
     */
    virtual void onClientDisconnected(TConnectedClient* pClient) = 0;

  private:
    /**
     * Common handling for new connected clients.  Implements concurrent
     * client rate limiting after onClientConnected returns by blocking the
     * serve() thread if the limit has been reached.
     */
    void newlyConnectedClient(const std::shared_ptr<TConnectedClient>& pClient);

    /**
     * Smart pointer client deletion.
     * Calls onClientDisconnected and then deletes pClient.
     */
    void disposeConnectedClient(TConnectedClient* pClient);

    /**
     * Monitor for limiting the number of concurrent clients.
     */
    my_thrift::apache::thrift::concurrency::Monitor mon_;

    /**
     * The number of concurrent clients.
     */
    int64_t clients_;

    /**
     * The high water mark of concurrent clients.
     */
    int64_t hwm_;

    /**
     * The limit on the number of concurrent clients.
     */
    int64_t limit_;
  };
  // ***TServerFramework.cpp***
  using my_thrift::apache::thrift::concurrency::Synchronized;
  using my_thrift::apache::thrift::protocol::TProtocol;
  using my_thrift::apache::thrift::protocol::TProtocolFactory;
  using std::bind;
  using std::shared_ptr;
  using my_thrift::apache::thrift::transport::TServerTransport;
  using my_thrift::apache::thrift::transport::TTransport;
  using my_thrift::apache::thrift::transport::TTransportException;
  using my_thrift::apache::thrift::transport::TTransportFactory;
  using std::string;

  TServerFramework::TServerFramework(const shared_ptr<TProcessorFactory>& processorFactory,
                                    const shared_ptr<TServerTransport>& serverTransport,
                                    const shared_ptr<TTransportFactory>& transportFactory,
                                    const shared_ptr<TProtocolFactory>& protocolFactory)
    : TServer(processorFactory, serverTransport, transportFactory, protocolFactory),
      clients_(0),
      hwm_(0),
      limit_(INT64_MAX) {
  }

  TServerFramework::TServerFramework(const shared_ptr<TProcessor>& processor,
                                    const shared_ptr<TServerTransport>& serverTransport,
                                    const shared_ptr<TTransportFactory>& transportFactory,
                                    const shared_ptr<TProtocolFactory>& protocolFactory)
    : TServer(processor, serverTransport, transportFactory, protocolFactory),
      clients_(0),
      hwm_(0),
      limit_(INT64_MAX) {
  }

  TServerFramework::TServerFramework(const shared_ptr<TProcessorFactory>& processorFactory,
                                    const shared_ptr<TServerTransport>& serverTransport,
                                    const shared_ptr<TTransportFactory>& inputTransportFactory,
                                    const shared_ptr<TTransportFactory>& outputTransportFactory,
                                    const shared_ptr<TProtocolFactory>& inputProtocolFactory,
                                    const shared_ptr<TProtocolFactory>& outputProtocolFactory)
    : TServer(processorFactory,
              serverTransport,
              inputTransportFactory,
              outputTransportFactory,
              inputProtocolFactory,
              outputProtocolFactory),
      clients_(0),
      hwm_(0),
      limit_(INT64_MAX) {
  }

  TServerFramework::TServerFramework(const shared_ptr<TProcessor>& processor,
                                    const shared_ptr<TServerTransport>& serverTransport,
                                    const shared_ptr<TTransportFactory>& inputTransportFactory,
                                    const shared_ptr<TTransportFactory>& outputTransportFactory,
                                    const shared_ptr<TProtocolFactory>& inputProtocolFactory,
                                    const shared_ptr<TProtocolFactory>& outputProtocolFactory)
    : TServer(processor,
              serverTransport,
              inputTransportFactory,
              outputTransportFactory,
              inputProtocolFactory,
              outputProtocolFactory),
      clients_(0),
      hwm_(0),
      limit_(INT64_MAX) {
  }

  TServerFramework::~TServerFramework() = default;

  template <typename T>
  static void releaseOneDescriptor(const string& name, T& pTransport) {
    if (pTransport) {
      try {
        pTransport->close();
      } catch (const TTransportException& ttx) {
        string errStr = string("TServerFramework " + name + " close failed: ") + ttx.what();
        GlobalOutput(errStr.c_str());
      }
    }
  }

  void TServerFramework::serve() {
    shared_ptr<TTransport> client;
    shared_ptr<TTransport> inputTransport;
    shared_ptr<TTransport> outputTransport;
    shared_ptr<TProtocol> inputProtocol;
    shared_ptr<TProtocol> outputProtocol;

    // Start the server listening
    serverTransport_->listen();

    // Run the preServe event to indicate server is now listening
    // and that it is safe to connect.
    if (eventHandler_) { //skip
      eventHandler_->preServe();
    }

    // Fetch client from server
    for (;;) {
      try {
        // Dereference any resources from any previous client creation
        // such that a blocking accept does not hold them indefinitely.
        outputProtocol.reset();
        inputProtocol.reset();
        outputTransport.reset();
        inputTransport.reset();
        client.reset();

        // If we have reached the limit on the number of concurrent
        // clients allowed, wait for one or more clients to drain before
        // accepting another.
        {
          Synchronized sync(mon_);
          while (clients_ >= limit_) {
            mon_.wait();
          }
        }

        client = serverTransport_->accept();
        
        inputTransport = inputTransportFactory_->getTransport(client);
        outputTransport = outputTransportFactory_->getTransport(client);
        if (!outputProtocolFactory_) {
          inputProtocol = inputProtocolFactory_->getProtocol(inputTransport, outputTransport);
          outputProtocol = inputProtocol;
        } else {
          inputProtocol = inputProtocolFactory_->getProtocol(inputTransport);
          outputProtocol = outputProtocolFactory_->getProtocol(outputTransport);
        }

        newlyConnectedClient(shared_ptr<TConnectedClient>(
            new TConnectedClient(getProcessor(inputProtocol, outputProtocol, client),
                                inputProtocol,
                                outputProtocol,
                                eventHandler_,
                                client),
            bind(&TServerFramework::disposeConnectedClient, this, std::placeholders::_1)));

      } catch (TTransportException& ttx) {
        releaseOneDescriptor("inputTransport", inputTransport);
        releaseOneDescriptor("outputTransport", outputTransport);
        releaseOneDescriptor("client", client);
        if (ttx.getType() == TTransportException::TIMED_OUT
            || ttx.getType() == TTransportException::CLIENT_DISCONNECT) {
          // Accept timeout and client disconnect - continue processing.
          continue;
        } else if (ttx.getType() == TTransportException::END_OF_FILE
                  || ttx.getType() == TTransportException::INTERRUPTED) {
          // Server was interrupted.  This only happens when stopping.
          break;
        } else {
          // All other transport exceptions are logged.
          // State of connection is unknown.  Done.
          string errStr = string("TServerTransport died: ") + ttx.what();
          GlobalOutput(errStr.c_str());
          break;
        }
      }
    }

    releaseOneDescriptor("serverTransport", serverTransport_);
  }

  int64_t TServerFramework::getConcurrentClientLimit() const {
    Synchronized sync(mon_);
    return limit_;
  }

  int64_t TServerFramework::getConcurrentClientCount() const {
    Synchronized sync(mon_);
    return clients_;
  }

  int64_t TServerFramework::getConcurrentClientCountHWM() const {
    Synchronized sync(mon_);
    return hwm_;
  }

  void TServerFramework::setConcurrentClientLimit(int64_t newLimit) {
    if (newLimit < 1) {
      throw std::invalid_argument("newLimit must be greater than zero");
    }
    Synchronized sync(mon_);
    limit_ = newLimit;
    if (limit_ - clients_ > 0) {
      mon_.notify();
    }
  }

  void TServerFramework::stop() {
    // Order is important because serve() releases serverTransport_ when it is
    // interrupted, which closes the socket that interruptChildren uses.
    serverTransport_->interruptChildren();
    serverTransport_->interrupt();
  }

  void TServerFramework::newlyConnectedClient(const shared_ptr<TConnectedClient>& pClient) {
    {
      Synchronized sync(mon_);
      ++clients_;
      hwm_ = (std::max)(hwm_, clients_);
    }

    onClientConnected(pClient); //in TSimpleServer.cpp:88
  }

  void TServerFramework::disposeConnectedClient(TConnectedClient* pClient) {
    onClientDisconnected(pClient);
    delete pClient;

    Synchronized sync(mon_);
    if (limit_ - --clients_ > 0) {
      mon_.notify();
    }
  }
  // ***TSimpleServer.h***
  /**
   * This is the most basic simple server. It is single-threaded and runs a
   * continuous loop of accepting a single connection, processing requests on
   * that connection until it closes, and then repeating.
   */
  class TSimpleServer : public TServerFramework {
  public:
    TSimpleServer(
        const std::shared_ptr<my_thrift::apache::thrift::TProcessorFactory>& processorFactory,
        const std::shared_ptr<my_thrift::apache::thrift::transport::TServerTransport>& serverTransport,
        const std::shared_ptr<my_thrift::apache::thrift::transport::TTransportFactory>& transportFactory,
        const std::shared_ptr<my_thrift::apache::thrift::protocol::TProtocolFactory>& protocolFactory);

    TSimpleServer(
        const std::shared_ptr<my_thrift::apache::thrift::TProcessor>& processor,
        const std::shared_ptr<my_thrift::apache::thrift::transport::TServerTransport>& serverTransport,
        const std::shared_ptr<my_thrift::apache::thrift::transport::TTransportFactory>& transportFactory,
        const std::shared_ptr<my_thrift::apache::thrift::protocol::TProtocolFactory>& protocolFactory);

    TSimpleServer(
        const std::shared_ptr<my_thrift::apache::thrift::TProcessorFactory>& processorFactory,
        const std::shared_ptr<my_thrift::apache::thrift::transport::TServerTransport>& serverTransport,
        const std::shared_ptr<my_thrift::apache::thrift::transport::TTransportFactory>& inputTransportFactory,
        const std::shared_ptr<my_thrift::apache::thrift::transport::TTransportFactory>& outputTransportFactory,
        const std::shared_ptr<my_thrift::apache::thrift::protocol::TProtocolFactory>& inputProtocolFactory,
        const std::shared_ptr<my_thrift::apache::thrift::protocol::TProtocolFactory>& outputProtocolFactory);

    TSimpleServer(
        const std::shared_ptr<my_thrift::apache::thrift::TProcessor>& processor,
        const std::shared_ptr<my_thrift::apache::thrift::transport::TServerTransport>& serverTransport,
        const std::shared_ptr<my_thrift::apache::thrift::transport::TTransportFactory>& inputTransportFactory,
        const std::shared_ptr<my_thrift::apache::thrift::transport::TTransportFactory>& outputTransportFactory,
        const std::shared_ptr<my_thrift::apache::thrift::protocol::TProtocolFactory>& inputProtocolFactory,
        const std::shared_ptr<my_thrift::apache::thrift::protocol::TProtocolFactory>& outputProtocolFactory);

    ~TSimpleServer() override;

  protected:
    void onClientConnected(const std::shared_ptr<TConnectedClient>& pClient) override /* override */;
    void onClientDisconnected(TConnectedClient* pClient) override /* override */;

  private:
    void setConcurrentClientLimit(int64_t newLimit) override; // hide
  };
  // ***TSimpleServer.cpp***
  using my_thrift::apache::thrift::protocol::TProtocol;
  using my_thrift::apache::thrift::protocol::TProtocolFactory;
  using my_thrift::apache::thrift::transport::TServerTransport;
  using my_thrift::apache::thrift::transport::TTransport;
  using my_thrift::apache::thrift::transport::TTransportException;
  using my_thrift::apache::thrift::transport::TTransportFactory;
  using std::shared_ptr;
  using std::string;

  TSimpleServer::TSimpleServer(const shared_ptr<TProcessorFactory>& processorFactory,
                              const shared_ptr<TServerTransport>& serverTransport,
                              const shared_ptr<TTransportFactory>& transportFactory,
                              const shared_ptr<TProtocolFactory>& protocolFactory)
    : TServerFramework(processorFactory, serverTransport, transportFactory, protocolFactory) {
    TServerFramework::setConcurrentClientLimit(1);
  }

  TSimpleServer::TSimpleServer(const shared_ptr<TProcessor>& processor,
                              const shared_ptr<TServerTransport>& serverTransport,
                              const shared_ptr<TTransportFactory>& transportFactory,
                              const shared_ptr<TProtocolFactory>& protocolFactory)
    : TServerFramework(processor, serverTransport, transportFactory, protocolFactory) {
    TServerFramework::setConcurrentClientLimit(1);
  }

  TSimpleServer::TSimpleServer(const shared_ptr<TProcessorFactory>& processorFactory,
                              const shared_ptr<TServerTransport>& serverTransport,
                              const shared_ptr<TTransportFactory>& inputTransportFactory,
                              const shared_ptr<TTransportFactory>& outputTransportFactory,
                              const shared_ptr<TProtocolFactory>& inputProtocolFactory,
                              const shared_ptr<TProtocolFactory>& outputProtocolFactory)
    : TServerFramework(processorFactory,
                      serverTransport,
                      inputTransportFactory,
                      outputTransportFactory,
                      inputProtocolFactory,
                      outputProtocolFactory) {
    TServerFramework::setConcurrentClientLimit(1);
  }

  TSimpleServer::TSimpleServer(const shared_ptr<TProcessor>& processor,
                              const shared_ptr<TServerTransport>& serverTransport,
                              const shared_ptr<TTransportFactory>& inputTransportFactory,
                              const shared_ptr<TTransportFactory>& outputTransportFactory,
                              const shared_ptr<TProtocolFactory>& inputProtocolFactory,
                              const shared_ptr<TProtocolFactory>& outputProtocolFactory)
    : TServerFramework(processor,
                      serverTransport,
                      inputTransportFactory,
                      outputTransportFactory,
                      inputProtocolFactory,
                      outputProtocolFactory) {
    TServerFramework::setConcurrentClientLimit(1);
  }

  TSimpleServer::~TSimpleServer() = default;

  /**
   * The main body of customized implementation for TSimpleServer is quite simple:
   * When a client connects, use the serve() thread to drive it to completion thus
   * blocking new connections.
   */
  void TSimpleServer::onClientConnected(const shared_ptr<TConnectedClient>& pClient) {
    pClient->run(); //in TConnectedClient.cpp:50
  }

  /**
   * TSimpleServer does not track clients so there is nothing to do here.
   */
  void TSimpleServer::onClientDisconnected(TConnectedClient*) {
  }

  /**
   * This makes little sense to the simple server because it is not capable
   * of having more than one client at a time, so we hide it.
   */
  void TSimpleServer::setConcurrentClientLimit(int64_t) {
  }

  // ***TThreadedServer.h***
  /**
   * Manage clients using threads - threads are created one for each client and are
   * released when the client disconnects.  This server is used to make a dynamically
   * scalable server up to the concurrent connection limit.
   */
  class TThreadedServer : public TServerFramework {
  public:
    TThreadedServer(
        const std::shared_ptr<my_thrift::apache::thrift::TProcessorFactory>& processorFactory,
        const std::shared_ptr<my_thrift::apache::thrift::transport::TServerTransport>& serverTransport,
        const std::shared_ptr<my_thrift::apache::thrift::transport::TTransportFactory>& transportFactory,
        const std::shared_ptr<my_thrift::apache::thrift::protocol::TProtocolFactory>& protocolFactory,
        const std::shared_ptr<my_thrift::apache::thrift::concurrency::ThreadFactory>& threadFactory
        = std::shared_ptr<my_thrift::apache::thrift::concurrency::ThreadFactory>(
            new my_thrift::apache::thrift::concurrency::ThreadFactory(false)));

    TThreadedServer(
        const std::shared_ptr<my_thrift::apache::thrift::TProcessor>& processor,
        const std::shared_ptr<my_thrift::apache::thrift::transport::TServerTransport>& serverTransport,
        const std::shared_ptr<my_thrift::apache::thrift::transport::TTransportFactory>& transportFactory,
        const std::shared_ptr<my_thrift::apache::thrift::protocol::TProtocolFactory>& protocolFactory,
        const std::shared_ptr<my_thrift::apache::thrift::concurrency::ThreadFactory>& threadFactory
        = std::shared_ptr<my_thrift::apache::thrift::concurrency::ThreadFactory>(
            new my_thrift::apache::thrift::concurrency::ThreadFactory(false)));

    TThreadedServer(
        const std::shared_ptr<my_thrift::apache::thrift::TProcessorFactory>& processorFactory,
        const std::shared_ptr<my_thrift::apache::thrift::transport::TServerTransport>& serverTransport,
        const std::shared_ptr<my_thrift::apache::thrift::transport::TTransportFactory>& inputTransportFactory,
        const std::shared_ptr<my_thrift::apache::thrift::transport::TTransportFactory>& outputTransportFactory,
        const std::shared_ptr<my_thrift::apache::thrift::protocol::TProtocolFactory>& inputProtocolFactory,
        const std::shared_ptr<my_thrift::apache::thrift::protocol::TProtocolFactory>& outputProtocolFactory,
        const std::shared_ptr<my_thrift::apache::thrift::concurrency::ThreadFactory>& threadFactory
        = std::shared_ptr<my_thrift::apache::thrift::concurrency::ThreadFactory>(
            new my_thrift::apache::thrift::concurrency::ThreadFactory(false)));

    TThreadedServer(
        const std::shared_ptr<my_thrift::apache::thrift::TProcessor>& processor,
        const std::shared_ptr<my_thrift::apache::thrift::transport::TServerTransport>& serverTransport,
        const std::shared_ptr<my_thrift::apache::thrift::transport::TTransportFactory>& inputTransportFactory,
        const std::shared_ptr<my_thrift::apache::thrift::transport::TTransportFactory>& outputTransportFactory,
        const std::shared_ptr<my_thrift::apache::thrift::protocol::TProtocolFactory>& inputProtocolFactory,
        const std::shared_ptr<my_thrift::apache::thrift::protocol::TProtocolFactory>& outputProtocolFactory,
        const std::shared_ptr<my_thrift::apache::thrift::concurrency::ThreadFactory>& threadFactory
        = std::shared_ptr<my_thrift::apache::thrift::concurrency::ThreadFactory>(
            new my_thrift::apache::thrift::concurrency::ThreadFactory(false)));

    ~TThreadedServer() override;

    /**
     * Post-conditions (return guarantees):
     *   There will be no clients connected.
     */
    void serve() override;

  protected:
    /**
     * Drain recently connected clients by joining their threads - this is done lazily because
     * we cannot do it inside the thread context that is disconnecting.
     */
    virtual void drainDeadClients();

    /**
     * Implementation of TServerFramework::onClientConnected
     */
    void onClientConnected(const std::shared_ptr<TConnectedClient>& pClient) override /* override */;

    /**
     * Implementation of TServerFramework::onClientDisconnected
     */
    void onClientDisconnected(TConnectedClient *pClient) override /* override */;

    std::shared_ptr<my_thrift::apache::thrift::concurrency::ThreadFactory> threadFactory_;

    /**
     * A helper wrapper used to wrap the client in something we can use to maintain
     * the lifetime of the connected client within a detached thread.  We cannot simply
     * track the threads because a shared_ptr<Thread> hangs on to the Runnable it is
     * passed, and TServerFramework requires the runnable (TConnectedClient) to be
     * destroyed in order to work properly.
     */
    class TConnectedClientRunner : public my_thrift::apache::thrift::concurrency::Runnable
    {
    public:
      TConnectedClientRunner(const std::shared_ptr<TConnectedClient>& pClient);
      ~TConnectedClientRunner() override;
      void run() override /* override */;
    private:
      std::shared_ptr<TConnectedClient> pClient_;
    };

    my_thrift::apache::thrift::concurrency::Monitor clientMonitor_;

    typedef std::map<TConnectedClient *, std::shared_ptr<my_thrift::apache::thrift::concurrency::Thread> > ClientMap;

    /**
     * A map of active clients
     */
    ClientMap activeClientMap_;

    /**
     * A map of clients that have disconnected but their threads have not been joined
     */
    ClientMap deadClientMap_;
  };
  // ***TThreadedServer.cpp***
  using my_thrift::apache::thrift::concurrency::Runnable;
  using my_thrift::apache::thrift::concurrency::Synchronized;
  using my_thrift::apache::thrift::concurrency::Thread;
  using my_thrift::apache::thrift::concurrency::ThreadFactory;
  using my_thrift::apache::thrift::protocol::TProtocol;
  using my_thrift::apache::thrift::protocol::TProtocolFactory;
  using std::make_shared;
  using std::shared_ptr;
  using my_thrift::apache::thrift::transport::TServerTransport;
  using my_thrift::apache::thrift::transport::TTransport;
  using my_thrift::apache::thrift::transport::TTransportException;
  using my_thrift::apache::thrift::transport::TTransportFactory;

  TThreadedServer::TThreadedServer(const shared_ptr<TProcessorFactory>& processorFactory,
                                  const shared_ptr<TServerTransport>& serverTransport,
                                  const shared_ptr<TTransportFactory>& transportFactory,
                                  const shared_ptr<TProtocolFactory>& protocolFactory,
                                  const shared_ptr<ThreadFactory>& threadFactory)
    : TServerFramework(processorFactory, serverTransport, transportFactory, protocolFactory),
      threadFactory_(threadFactory) {
  }

  TThreadedServer::TThreadedServer(const shared_ptr<TProcessor>& processor,
                                  const shared_ptr<TServerTransport>& serverTransport,
                                  const shared_ptr<TTransportFactory>& transportFactory,
                                  const shared_ptr<TProtocolFactory>& protocolFactory,
                                  const shared_ptr<ThreadFactory>& threadFactory)
    : TServerFramework(processor, serverTransport, transportFactory, protocolFactory),
      threadFactory_(threadFactory) {
  }

  TThreadedServer::TThreadedServer(const shared_ptr<TProcessorFactory>& processorFactory,
                                  const shared_ptr<TServerTransport>& serverTransport,
                                  const shared_ptr<TTransportFactory>& inputTransportFactory,
                                  const shared_ptr<TTransportFactory>& outputTransportFactory,
                                  const shared_ptr<TProtocolFactory>& inputProtocolFactory,
                                  const shared_ptr<TProtocolFactory>& outputProtocolFactory,
                                  const shared_ptr<ThreadFactory>& threadFactory)
    : TServerFramework(processorFactory,
                      serverTransport,
                      inputTransportFactory,
                      outputTransportFactory,
                      inputProtocolFactory,
                      outputProtocolFactory),
      threadFactory_(threadFactory) {
  }

  TThreadedServer::TThreadedServer(const shared_ptr<TProcessor>& processor,
                                  const shared_ptr<TServerTransport>& serverTransport,
                                  const shared_ptr<TTransportFactory>& inputTransportFactory,
                                  const shared_ptr<TTransportFactory>& outputTransportFactory,
                                  const shared_ptr<TProtocolFactory>& inputProtocolFactory,
                                  const shared_ptr<TProtocolFactory>& outputProtocolFactory,
                                  const shared_ptr<ThreadFactory>& threadFactory)
    : TServerFramework(processor,
                      serverTransport,
                      inputTransportFactory,
                      outputTransportFactory,
                      inputProtocolFactory,
                      outputProtocolFactory),
      threadFactory_(threadFactory) {
  }

  TThreadedServer::~TThreadedServer() = default;

  void TThreadedServer::serve() {
    TServerFramework::serve();

    // Ensure post-condition of no active clients
    Synchronized s(clientMonitor_);
    while (!activeClientMap_.empty()) {
      clientMonitor_.wait();
    }

    drainDeadClients();
  }

  void TThreadedServer::drainDeadClients() {
    // we're in a monitor here
    while (!deadClientMap_.empty()) {
      auto it = deadClientMap_.begin();
      it->second->join();
      deadClientMap_.erase(it);
    }
  }

  void TThreadedServer::onClientConnected(const shared_ptr<TConnectedClient>& pClient) {
    Synchronized sync(clientMonitor_);
    shared_ptr<TConnectedClientRunner> pRunnable = make_shared<TConnectedClientRunner>(pClient);
    shared_ptr<Thread> pThread = threadFactory_->newThread(pRunnable);
    pRunnable->thread(pThread);
    activeClientMap_.insert(ClientMap::value_type(pClient.get(), pThread));
    pThread->start();
  }

  void TThreadedServer::onClientDisconnected(TConnectedClient* pClient) {
    Synchronized sync(clientMonitor_);
    drainDeadClients(); // use the outgoing thread to do some maintenance on our dead client backlog
    auto it = activeClientMap_.find(pClient);
    if (it != activeClientMap_.end()) {
      auto end = it;
      deadClientMap_.insert(it, ++end);
      activeClientMap_.erase(it);
    }
    if (activeClientMap_.empty()) {
      clientMonitor_.notify();
    }
  }

  TThreadedServer::TConnectedClientRunner::TConnectedClientRunner(const shared_ptr<TConnectedClient>& pClient)
    : pClient_(pClient) {
  }

  TThreadedServer::TConnectedClientRunner::~TConnectedClientRunner() = default;

  void TThreadedServer::TConnectedClientRunner::run() /* override */ {
    pClient_->run();  // Run the client
    pClient_.reset(); // The client is done - release it here rather than in the destructor for safety
  }
  
  } // my_thrift::apache::thrift::server
}
}
} // my_thrift 

namespace thrift_memcached {

// ***MemcachedService.h***
#ifdef _MSC_VER
  #pragma warning( push )
  #pragma warning (disable : 4250 ) //inheriting methods via dominance 
#endif

class MemcachedServiceIf {
 public:
  virtual ~MemcachedServiceIf() {}
  virtual void getRequest(std::string& _return, const std::string& key) = 0;
  virtual bool setRequest(const std::string& key, const std::string& value) = 0;
};

class MemcachedServiceIfFactory {
 public:
  typedef MemcachedServiceIf Handler;

  virtual ~MemcachedServiceIfFactory() {}

  virtual MemcachedServiceIf* getHandler(const ::my_thrift::apache::thrift::TConnectionInfo& connInfo) = 0;
  virtual void releaseHandler(MemcachedServiceIf* /* handler */) = 0;
  };

class MemcachedServiceIfSingletonFactory : virtual public MemcachedServiceIfFactory {
 public:
  MemcachedServiceIfSingletonFactory(const ::std::shared_ptr<MemcachedServiceIf>& iface) : iface_(iface) {}
  virtual ~MemcachedServiceIfSingletonFactory() {}

  virtual MemcachedServiceIf* getHandler(const ::my_thrift::apache::thrift::TConnectionInfo&) override {
    return iface_.get();
  }
  virtual void releaseHandler(MemcachedServiceIf* /* handler */) override {}

 protected:
  ::std::shared_ptr<MemcachedServiceIf> iface_;
};

class MemcachedServiceNull : virtual public MemcachedServiceIf {
 public:
  virtual ~MemcachedServiceNull() {}
  void getRequest(std::string& /* _return */, const std::string& /* key */) override {
    return;
  }
  bool setRequest(const std::string& /* key */, const std::string& /* value */) override {
    bool _return = false;
    return _return;
  }
};

typedef struct _MemcachedService_getRequest_args__isset {
  _MemcachedService_getRequest_args__isset() : key(false) {}
  bool key :1;
} _MemcachedService_getRequest_args__isset;

class MemcachedService_getRequest_args {
 public:

  MemcachedService_getRequest_args(const MemcachedService_getRequest_args&);
  MemcachedService_getRequest_args& operator=(const MemcachedService_getRequest_args&);
  MemcachedService_getRequest_args() noexcept;

  virtual ~MemcachedService_getRequest_args() noexcept;
  std::string key;

  _MemcachedService_getRequest_args__isset __isset;

  void __set_key(const std::string& val);

  bool operator == (const MemcachedService_getRequest_args & rhs) const;
  bool operator != (const MemcachedService_getRequest_args &rhs) const {
    return !(*this == rhs);
  }

  bool operator < (const MemcachedService_getRequest_args & ) const;

  uint32_t read(::my_thrift::apache::thrift::protocol::TProtocol* iprot);
  uint32_t write(::my_thrift::apache::thrift::protocol::TProtocol* oprot) const;

};


class MemcachedService_getRequest_pargs {
 public:


  virtual ~MemcachedService_getRequest_pargs() noexcept;
  const std::string* key;

  uint32_t write(::my_thrift::apache::thrift::protocol::TProtocol* oprot) const;

};

typedef struct _MemcachedService_getRequest_result__isset {
  _MemcachedService_getRequest_result__isset() : success(false) {}
  bool success :1;
} _MemcachedService_getRequest_result__isset;

class MemcachedService_getRequest_result {
 public:

  MemcachedService_getRequest_result(const MemcachedService_getRequest_result&);
  MemcachedService_getRequest_result& operator=(const MemcachedService_getRequest_result&);
  MemcachedService_getRequest_result() noexcept;

  virtual ~MemcachedService_getRequest_result() noexcept;
  std::string success;

  _MemcachedService_getRequest_result__isset __isset;

  void __set_success(const std::string& val);

  bool operator == (const MemcachedService_getRequest_result & rhs) const;
  bool operator != (const MemcachedService_getRequest_result &rhs) const {
    return !(*this == rhs);
  }

  bool operator < (const MemcachedService_getRequest_result & ) const;

  uint32_t read(::my_thrift::apache::thrift::protocol::TProtocol* iprot);
  uint32_t write(::my_thrift::apache::thrift::protocol::TProtocol* oprot) const;

};

typedef struct _MemcachedService_getRequest_presult__isset {
  _MemcachedService_getRequest_presult__isset() : success(false) {}
  bool success :1;
} _MemcachedService_getRequest_presult__isset;

class MemcachedService_getRequest_presult {
 public:


  virtual ~MemcachedService_getRequest_presult() noexcept;
  std::string* success;

  _MemcachedService_getRequest_presult__isset __isset;

  uint32_t read(::my_thrift::apache::thrift::protocol::TProtocol* iprot);

};

typedef struct _MemcachedService_setRequest_args__isset {
  _MemcachedService_setRequest_args__isset() : key(false), value(false) {}
  bool key :1;
  bool value :1;
} _MemcachedService_setRequest_args__isset;

class MemcachedService_setRequest_args {
 public:

  MemcachedService_setRequest_args(const MemcachedService_setRequest_args&);
  MemcachedService_setRequest_args& operator=(const MemcachedService_setRequest_args&);
  MemcachedService_setRequest_args() noexcept;

  virtual ~MemcachedService_setRequest_args() noexcept;
  std::string key;
  std::string value;

  _MemcachedService_setRequest_args__isset __isset;

  void __set_key(const std::string& val);

  void __set_value(const std::string& val);

  bool operator == (const MemcachedService_setRequest_args & rhs) const;
  bool operator != (const MemcachedService_setRequest_args &rhs) const {
    return !(*this == rhs);
  }

  bool operator < (const MemcachedService_setRequest_args & ) const;

  uint32_t read(::my_thrift::apache::thrift::protocol::TProtocol* iprot);
  uint32_t write(::my_thrift::apache::thrift::protocol::TProtocol* oprot) const;

};


class MemcachedService_setRequest_pargs {
 public:


  virtual ~MemcachedService_setRequest_pargs() noexcept;
  const std::string* key;
  const std::string* value;

  uint32_t write(::my_thrift::apache::thrift::protocol::TProtocol* oprot) const;

};

typedef struct _MemcachedService_setRequest_result__isset {
  _MemcachedService_setRequest_result__isset() : success(false) {}
  bool success :1;
} _MemcachedService_setRequest_result__isset;

class MemcachedService_setRequest_result {
 public:

  MemcachedService_setRequest_result(const MemcachedService_setRequest_result&) noexcept;
  MemcachedService_setRequest_result& operator=(const MemcachedService_setRequest_result&) noexcept;
  MemcachedService_setRequest_result() noexcept;

  virtual ~MemcachedService_setRequest_result() noexcept;
  bool success;

  _MemcachedService_setRequest_result__isset __isset;

  void __set_success(const bool val);

  bool operator == (const MemcachedService_setRequest_result & rhs) const;
  bool operator != (const MemcachedService_setRequest_result &rhs) const {
    return !(*this == rhs);
  }

  bool operator < (const MemcachedService_setRequest_result & ) const;

  uint32_t read(::my_thrift::apache::thrift::protocol::TProtocol* iprot);
  uint32_t write(::my_thrift::apache::thrift::protocol::TProtocol* oprot) const;

};

typedef struct _MemcachedService_setRequest_presult__isset {
  _MemcachedService_setRequest_presult__isset() : success(false) {}
  bool success :1;
} _MemcachedService_setRequest_presult__isset;

class MemcachedService_setRequest_presult {
 public:


  virtual ~MemcachedService_setRequest_presult() noexcept;
  bool* success;

  _MemcachedService_setRequest_presult__isset __isset;

  uint32_t read(::my_thrift::apache::thrift::protocol::TProtocol* iprot);

};

class MemcachedServiceClient : virtual public MemcachedServiceIf {
 public:
  MemcachedServiceClient(std::shared_ptr< ::my_thrift::apache::thrift::protocol::TProtocol> prot) {
    setProtocol(prot);
  }
  MemcachedServiceClient(std::shared_ptr< ::my_thrift::apache::thrift::protocol::TProtocol> iprot, std::shared_ptr< ::my_thrift::apache::thrift::protocol::TProtocol> oprot) {
    setProtocol(iprot,oprot);
  }
 private:
  void setProtocol(std::shared_ptr< ::my_thrift::apache::thrift::protocol::TProtocol> prot) {
  setProtocol(prot,prot);
  }
  void setProtocol(std::shared_ptr< ::my_thrift::apache::thrift::protocol::TProtocol> iprot, std::shared_ptr< ::my_thrift::apache::thrift::protocol::TProtocol> oprot) {
    piprot_=iprot;
    poprot_=oprot;
    iprot_ = iprot.get();
    oprot_ = oprot.get();
  }
 public:
  std::shared_ptr< ::my_thrift::apache::thrift::protocol::TProtocol> getInputProtocol() {
    return piprot_;
  }
  std::shared_ptr< ::my_thrift::apache::thrift::protocol::TProtocol> getOutputProtocol() {
    return poprot_;
  }
  void getRequest(std::string& _return, const std::string& key) override;
  void send_getRequest(const std::string& key);
  void recv_getRequest(std::string& _return);
  bool setRequest(const std::string& key, const std::string& value) override;
  void send_setRequest(const std::string& key, const std::string& value);
  bool recv_setRequest();
 protected:
  std::shared_ptr< ::my_thrift::apache::thrift::protocol::TProtocol> piprot_;
  std::shared_ptr< ::my_thrift::apache::thrift::protocol::TProtocol> poprot_;
  ::my_thrift::apache::thrift::protocol::TProtocol* iprot_;
  ::my_thrift::apache::thrift::protocol::TProtocol* oprot_;
};

class MemcachedServiceProcessor : public ::my_thrift::apache::thrift::TDispatchProcessor {
 protected:
  ::std::shared_ptr<MemcachedServiceIf> iface_;
  virtual bool dispatchCall(::my_thrift::apache::thrift::protocol::TProtocol* iprot, ::my_thrift::apache::thrift::protocol::TProtocol* oprot, const std::string& fname, int32_t seqid, void* callContext) override;
 private:
  typedef  void (MemcachedServiceProcessor::*ProcessFunction)(int32_t, ::my_thrift::apache::thrift::protocol::TProtocol*, ::my_thrift::apache::thrift::protocol::TProtocol*, void*);
  typedef std::map<std::string, ProcessFunction> ProcessMap;
  ProcessMap processMap_;
  void process_getRequest(int32_t seqid, ::my_thrift::apache::thrift::protocol::TProtocol* iprot, ::my_thrift::apache::thrift::protocol::TProtocol* oprot, void* callContext);
  void process_setRequest(int32_t seqid, ::my_thrift::apache::thrift::protocol::TProtocol* iprot, ::my_thrift::apache::thrift::protocol::TProtocol* oprot, void* callContext);
 public:
  MemcachedServiceProcessor(::std::shared_ptr<MemcachedServiceIf> iface) :
    iface_(iface) {
    processMap_["getRequest"] = &MemcachedServiceProcessor::process_getRequest;
    processMap_["setRequest"] = &MemcachedServiceProcessor::process_setRequest;
  }

  virtual ~MemcachedServiceProcessor() {}
};

class MemcachedServiceProcessorFactory : public ::my_thrift::apache::thrift::TProcessorFactory {
 public:
  MemcachedServiceProcessorFactory(const ::std::shared_ptr< MemcachedServiceIfFactory >& handlerFactory) noexcept :
      handlerFactory_(handlerFactory) {}

  ::std::shared_ptr< ::my_thrift::apache::thrift::TProcessor > getProcessor(const ::my_thrift::apache::thrift::TConnectionInfo& connInfo) override;

 protected:
  ::std::shared_ptr< MemcachedServiceIfFactory > handlerFactory_;
};

class MemcachedServiceMultiface : virtual public MemcachedServiceIf {
 public:
  MemcachedServiceMultiface(std::vector<std::shared_ptr<MemcachedServiceIf> >& ifaces) : ifaces_(ifaces) {
  }
  virtual ~MemcachedServiceMultiface() {}
 protected:
  std::vector<std::shared_ptr<MemcachedServiceIf> > ifaces_;
  MemcachedServiceMultiface() {}
  void add(::std::shared_ptr<MemcachedServiceIf> iface) {
    ifaces_.push_back(iface);
  }
 public:
  void getRequest(std::string& _return, const std::string& key) override {
    size_t sz = ifaces_.size();
    size_t i = 0;
    for (; i < (sz - 1); ++i) {
      ifaces_[i]->getRequest(_return, key);
    }
    ifaces_[i]->getRequest(_return, key);
    return;
  }

  bool setRequest(const std::string& key, const std::string& value) override {
    size_t sz = ifaces_.size();
    size_t i = 0;
    for (; i < (sz - 1); ++i) {
      ifaces_[i]->setRequest(key, value);
    }
    return ifaces_[i]->setRequest(key, value);
  }

};

// The 'concurrent' client is a thread safe client that correctly handles
// out of order responses.  It is slower than the regular client, so should
// only be used when you need to share a connection among multiple threads
class MemcachedServiceConcurrentClient : virtual public MemcachedServiceIf {
 public:
  MemcachedServiceConcurrentClient(std::shared_ptr< ::my_thrift::apache::thrift::protocol::TProtocol> prot, std::shared_ptr< ::my_thrift::apache::thrift::async::TConcurrentClientSyncInfo> sync) : sync_(sync)
{
    setProtocol(prot);
  }
  MemcachedServiceConcurrentClient(std::shared_ptr< ::my_thrift::apache::thrift::protocol::TProtocol> iprot, std::shared_ptr< ::my_thrift::apache::thrift::protocol::TProtocol> oprot, std::shared_ptr< ::my_thrift::apache::thrift::async::TConcurrentClientSyncInfo> sync) : sync_(sync)
{
    setProtocol(iprot,oprot);
  }
 private:
  void setProtocol(std::shared_ptr< ::my_thrift::apache::thrift::protocol::TProtocol> prot) {
  setProtocol(prot,prot);
  }
  void setProtocol(std::shared_ptr< ::my_thrift::apache::thrift::protocol::TProtocol> iprot, std::shared_ptr< ::my_thrift::apache::thrift::protocol::TProtocol> oprot) {
    piprot_=iprot;
    poprot_=oprot;
    iprot_ = iprot.get();
    oprot_ = oprot.get();
  }
 public:
  std::shared_ptr< ::my_thrift::apache::thrift::protocol::TProtocol> getInputProtocol() {
    return piprot_;
  }
  std::shared_ptr< ::my_thrift::apache::thrift::protocol::TProtocol> getOutputProtocol() {
    return poprot_;
  }
  void getRequest(std::string& _return, const std::string& key) override;
  int32_t send_getRequest(const std::string& key);
  void recv_getRequest(std::string& _return, const int32_t seqid);
  bool setRequest(const std::string& key, const std::string& value) override;
  int32_t send_setRequest(const std::string& key, const std::string& value);
  bool recv_setRequest(const int32_t seqid);
 protected:
  std::shared_ptr< ::my_thrift::apache::thrift::protocol::TProtocol> piprot_;
  std::shared_ptr< ::my_thrift::apache::thrift::protocol::TProtocol> poprot_;
  ::my_thrift::apache::thrift::protocol::TProtocol* iprot_;
  ::my_thrift::apache::thrift::protocol::TProtocol* oprot_;
  std::shared_ptr< ::my_thrift::apache::thrift::async::TConcurrentClientSyncInfo> sync_;
};

#ifdef _MSC_VER
  #pragma warning( pop )
#endif

// ***MemcachedService.cpp***
MemcachedService_getRequest_args::~MemcachedService_getRequest_args() noexcept {
  }

  MemcachedService_getRequest_args::MemcachedService_getRequest_args() noexcept
    : key() {
  }

  uint32_t MemcachedService_getRequest_args::read(::my_thrift::apache::thrift::protocol::TProtocol* iprot) {

    ::my_thrift::apache::thrift::protocol::TInputRecursionTracker tracker(*iprot);
    uint32_t xfer = 0;
    std::string fname;
    ::my_thrift::apache::thrift::protocol::TType ftype;
    int16_t fid;

    xfer += iprot->readStructBegin(fname);

    using ::my_thrift::apache::thrift::protocol::TProtocolException;


    while (true)
    {
      xfer += iprot->readFieldBegin(fname, ftype, fid);
      if (ftype == ::my_thrift::apache::thrift::protocol::T_STOP) {
        break;
      }
      switch (fid)
      {
        case 1:
          if (ftype == ::my_thrift::apache::thrift::protocol::T_STRING) {
            xfer += iprot->readString(this->key);
            this->__isset.key = true;
          } else {
            xfer += iprot->skip(ftype);
          }
          break;
        default:
          xfer += iprot->skip(ftype);
          break;
      }
      xfer += iprot->readFieldEnd();
    }

    xfer += iprot->readStructEnd();

    return xfer;
  }

  uint32_t MemcachedService_getRequest_args::write(::my_thrift::apache::thrift::protocol::TProtocol* oprot) const {
    uint32_t xfer = 0;
    ::my_thrift::apache::thrift::protocol::TOutputRecursionTracker tracker(*oprot);
    xfer += oprot->writeStructBegin("MemcachedService_getRequest_args");

    xfer += oprot->writeFieldBegin("key", ::my_thrift::apache::thrift::protocol::T_STRING, 1);
    xfer += oprot->writeString(this->key);
    xfer += oprot->writeFieldEnd();

    xfer += oprot->writeFieldStop();
    xfer += oprot->writeStructEnd();
    return xfer;
  }


  MemcachedService_getRequest_pargs::~MemcachedService_getRequest_pargs() noexcept {
  }


  uint32_t MemcachedService_getRequest_pargs::write(::my_thrift::apache::thrift::protocol::TProtocol* oprot) const {
    uint32_t xfer = 0;
    ::my_thrift::apache::thrift::protocol::TOutputRecursionTracker tracker(*oprot);
    xfer += oprot->writeStructBegin("MemcachedService_getRequest_pargs");

    xfer += oprot->writeFieldBegin("key", ::my_thrift::apache::thrift::protocol::T_STRING, 1);
    xfer += oprot->writeString((*(this->key)));
    xfer += oprot->writeFieldEnd();

    xfer += oprot->writeFieldStop();
    xfer += oprot->writeStructEnd();
    return xfer;
  }


  MemcachedService_getRequest_result::~MemcachedService_getRequest_result() noexcept {
  }

  MemcachedService_getRequest_result::MemcachedService_getRequest_result() noexcept
    : success() {
  }

  uint32_t MemcachedService_getRequest_result::read(::my_thrift::apache::thrift::protocol::TProtocol* iprot) {

    ::my_thrift::apache::thrift::protocol::TInputRecursionTracker tracker(*iprot);
    uint32_t xfer = 0;
    std::string fname;
    ::my_thrift::apache::thrift::protocol::TType ftype;
    int16_t fid;

    xfer += iprot->readStructBegin(fname);

    using ::my_thrift::apache::thrift::protocol::TProtocolException;


    while (true)
    {
      xfer += iprot->readFieldBegin(fname, ftype, fid);
      if (ftype == ::my_thrift::apache::thrift::protocol::T_STOP) {
        break;
      }
      switch (fid)
      {
        case 0:
          if (ftype == ::my_thrift::apache::thrift::protocol::T_STRING) {
            xfer += iprot->readString(this->success);
            this->__isset.success = true;
          } else {
            xfer += iprot->skip(ftype);
          }
          break;
        default:
          xfer += iprot->skip(ftype);
          break;
      }
      xfer += iprot->readFieldEnd();
    }

    xfer += iprot->readStructEnd();

    return xfer;
  }

  uint32_t MemcachedService_getRequest_result::write(::my_thrift::apache::thrift::protocol::TProtocol* oprot) const {

    uint32_t xfer = 0;

    xfer += oprot->writeStructBegin("MemcachedService_getRequest_result");

    if (this->__isset.success) {
      xfer += oprot->writeFieldBegin("success", ::my_thrift::apache::thrift::protocol::T_STRING, 0);
      xfer += oprot->writeString(this->success);
      xfer += oprot->writeFieldEnd();
    }
    xfer += oprot->writeFieldStop();
    xfer += oprot->writeStructEnd();
    return xfer;
  }


  MemcachedService_getRequest_presult::~MemcachedService_getRequest_presult() noexcept {
  }


  uint32_t MemcachedService_getRequest_presult::read(::my_thrift::apache::thrift::protocol::TProtocol* iprot) {

    ::my_thrift::apache::thrift::protocol::TInputRecursionTracker tracker(*iprot);
    uint32_t xfer = 0;
    std::string fname;
    ::my_thrift::apache::thrift::protocol::TType ftype;
    int16_t fid;

    xfer += iprot->readStructBegin(fname);

    using ::my_thrift::apache::thrift::protocol::TProtocolException;


    while (true)
    {
      xfer += iprot->readFieldBegin(fname, ftype, fid);
      if (ftype == ::my_thrift::apache::thrift::protocol::T_STOP) {
        break;
      }
      switch (fid)
      {
        case 0:
          if (ftype == ::my_thrift::apache::thrift::protocol::T_STRING) {
            xfer += iprot->readString((*(this->success)));
            this->__isset.success = true;
          } else {
            xfer += iprot->skip(ftype);
          }
          break;
        default:
          xfer += iprot->skip(ftype);
          break;
      }
      xfer += iprot->readFieldEnd();
    }

    xfer += iprot->readStructEnd();

    return xfer;
  }


  MemcachedService_setRequest_args::~MemcachedService_setRequest_args() noexcept {
  }

  MemcachedService_setRequest_args::MemcachedService_setRequest_args() noexcept
    : key(),
      value() {
  }

  uint32_t MemcachedService_setRequest_args::read(::my_thrift::apache::thrift::protocol::TProtocol* iprot) {

    ::my_thrift::apache::thrift::protocol::TInputRecursionTracker tracker(*iprot);
    uint32_t xfer = 0;
    std::string fname;
    ::my_thrift::apache::thrift::protocol::TType ftype;
    int16_t fid;

    xfer += iprot->readStructBegin(fname);

    using ::my_thrift::apache::thrift::protocol::TProtocolException;


    while (true)
    {
      xfer += iprot->readFieldBegin(fname, ftype, fid);
      if (ftype == ::my_thrift::apache::thrift::protocol::T_STOP) {
        break;
      }
      switch (fid)
      {
        case 1:
          if (ftype == ::my_thrift::apache::thrift::protocol::T_STRING) {
            xfer += iprot->readString(this->key);
            this->__isset.key = true;
          } else {
            xfer += iprot->skip(ftype);
          }
          break;
        case 2:
          if (ftype == ::my_thrift::apache::thrift::protocol::T_STRING) {
            xfer += iprot->readString(this->value);
            this->__isset.value = true;
          } else {
            xfer += iprot->skip(ftype);
          }
          break;
        default:
          xfer += iprot->skip(ftype);
          break;
      }
      xfer += iprot->readFieldEnd();
    }

    xfer += iprot->readStructEnd();

    return xfer;
  }

  uint32_t MemcachedService_setRequest_args::write(::my_thrift::apache::thrift::protocol::TProtocol* oprot) const {
    uint32_t xfer = 0;
    ::my_thrift::apache::thrift::protocol::TOutputRecursionTracker tracker(*oprot);
    xfer += oprot->writeStructBegin("MemcachedService_setRequest_args");

    xfer += oprot->writeFieldBegin("key", ::my_thrift::apache::thrift::protocol::T_STRING, 1);
    xfer += oprot->writeString(this->key);
    xfer += oprot->writeFieldEnd();

    xfer += oprot->writeFieldBegin("value", ::my_thrift::apache::thrift::protocol::T_STRING, 2);
    xfer += oprot->writeString(this->value);
    xfer += oprot->writeFieldEnd();

    xfer += oprot->writeFieldStop();
    xfer += oprot->writeStructEnd();
    return xfer;
  }


  MemcachedService_setRequest_pargs::~MemcachedService_setRequest_pargs() noexcept {
  }


  uint32_t MemcachedService_setRequest_pargs::write(::my_thrift::apache::thrift::protocol::TProtocol* oprot) const {
    uint32_t xfer = 0;
    ::my_thrift::apache::thrift::protocol::TOutputRecursionTracker tracker(*oprot);
    xfer += oprot->writeStructBegin("MemcachedService_setRequest_pargs");

    xfer += oprot->writeFieldBegin("key", ::my_thrift::apache::thrift::protocol::T_STRING, 1);
    xfer += oprot->writeString((*(this->key)));
    xfer += oprot->writeFieldEnd();

    xfer += oprot->writeFieldBegin("value", ::my_thrift::apache::thrift::protocol::T_STRING, 2);
    xfer += oprot->writeString((*(this->value)));
    xfer += oprot->writeFieldEnd();

    xfer += oprot->writeFieldStop();
    xfer += oprot->writeStructEnd();
    return xfer;
  }


  MemcachedService_setRequest_result::~MemcachedService_setRequest_result() noexcept {
  }

  MemcachedService_setRequest_result::MemcachedService_setRequest_result() noexcept
    : success(0) {
  }

  uint32_t MemcachedService_setRequest_result::read(::my_thrift::apache::thrift::protocol::TProtocol* iprot) {

    ::my_thrift::apache::thrift::protocol::TInputRecursionTracker tracker(*iprot);
    uint32_t xfer = 0;
    std::string fname;
    ::my_thrift::apache::thrift::protocol::TType ftype;
    int16_t fid;

    xfer += iprot->readStructBegin(fname);

    using ::my_thrift::apache::thrift::protocol::TProtocolException;


    while (true)
    {
      xfer += iprot->readFieldBegin(fname, ftype, fid);
      if (ftype == ::my_thrift::apache::thrift::protocol::T_STOP) {
        break;
      }
      switch (fid)
      {
        case 0:
          if (ftype == ::my_thrift::apache::thrift::protocol::T_BOOL) {
            xfer += iprot->readBool(this->success);
            this->__isset.success = true;
          } else {
            xfer += iprot->skip(ftype);
          }
          break;
        default:
          xfer += iprot->skip(ftype);
          break;
      }
      xfer += iprot->readFieldEnd();
    }

    xfer += iprot->readStructEnd();

    return xfer;
  }

  uint32_t MemcachedService_setRequest_result::write(::my_thrift::apache::thrift::protocol::TProtocol* oprot) const {

    uint32_t xfer = 0;

    xfer += oprot->writeStructBegin("MemcachedService_setRequest_result");

    if (this->__isset.success) {
      xfer += oprot->writeFieldBegin("success", ::my_thrift::apache::thrift::protocol::T_BOOL, 0);
      xfer += oprot->writeBool(this->success);
      xfer += oprot->writeFieldEnd();
    }
    xfer += oprot->writeFieldStop();
    xfer += oprot->writeStructEnd();
    return xfer;
  }


  MemcachedService_setRequest_presult::~MemcachedService_setRequest_presult() noexcept {
  }


  uint32_t MemcachedService_setRequest_presult::read(::my_thrift::apache::thrift::protocol::TProtocol* iprot) {

    ::my_thrift::apache::thrift::protocol::TInputRecursionTracker tracker(*iprot);
    uint32_t xfer = 0;
    std::string fname;
    ::my_thrift::apache::thrift::protocol::TType ftype;
    int16_t fid;

    xfer += iprot->readStructBegin(fname);

    using ::my_thrift::apache::thrift::protocol::TProtocolException;


    while (true)
    {
      xfer += iprot->readFieldBegin(fname, ftype, fid);
      if (ftype == ::my_thrift::apache::thrift::protocol::T_STOP) {
        break;
      }
      switch (fid)
      {
        case 0:
          if (ftype == ::my_thrift::apache::thrift::protocol::T_BOOL) {
            xfer += iprot->readBool((*(this->success)));
            this->__isset.success = true;
          } else {
            xfer += iprot->skip(ftype);
          }
          break;
        default:
          xfer += iprot->skip(ftype);
          break;
      }
      xfer += iprot->readFieldEnd();
    }

    xfer += iprot->readStructEnd();

    return xfer;
  }

  void MemcachedServiceClient::getRequest(std::string& _return, const std::string& key)
  {
    send_getRequest(key);
    recv_getRequest(_return);
  }

  void MemcachedServiceClient::send_getRequest(const std::string& key)
  {
    int32_t cseqid = 0;
    oprot_->writeMessageBegin("getRequest", ::my_thrift::apache::thrift::protocol::T_CALL, cseqid);

    MemcachedService_getRequest_pargs args;
    args.key = &key;
    args.write(oprot_);

    oprot_->writeMessageEnd();
    oprot_->getTransport()->writeEnd();
    oprot_->getTransport()->flush();
  }

  void MemcachedServiceClient::recv_getRequest(std::string& _return)
  {

    int32_t rseqid = 0;
    std::string fname;
    ::my_thrift::apache::thrift::protocol::TMessageType mtype;

    iprot_->readMessageBegin(fname, mtype, rseqid);
    if (mtype == ::my_thrift::apache::thrift::protocol::T_EXCEPTION) {
      ::my_thrift::apache::thrift::TApplicationException x;
      x.read(iprot_);
      iprot_->readMessageEnd();
      iprot_->getTransport()->readEnd();
      throw x;
    }
    if (mtype != ::my_thrift::apache::thrift::protocol::T_REPLY) {
      iprot_->skip(::my_thrift::apache::thrift::protocol::T_STRUCT);
      iprot_->readMessageEnd();
      iprot_->getTransport()->readEnd();
    }
    if (fname.compare("getRequest") != 0) {
      iprot_->skip(::my_thrift::apache::thrift::protocol::T_STRUCT);
      iprot_->readMessageEnd();
      iprot_->getTransport()->readEnd();
    }
    MemcachedService_getRequest_presult result;
    result.success = &_return;
    result.read(iprot_);
    iprot_->readMessageEnd();
    iprot_->getTransport()->readEnd();

    if (result.__isset.success) {
      // _return pointer has now been filled
      return;
    }
    throw ::my_thrift::apache::thrift::TApplicationException(::my_thrift::apache::thrift::TApplicationException::MISSING_RESULT, "getRequest failed: unknown result");
  }

  bool MemcachedServiceClient::setRequest(const std::string& key, const std::string& value)
  {
    send_setRequest(key, value);
    return recv_setRequest();
  }

  void MemcachedServiceClient::send_setRequest(const std::string& key, const std::string& value)
  {
    int32_t cseqid = 0;
    oprot_->writeMessageBegin("setRequest", ::my_thrift::apache::thrift::protocol::T_CALL, cseqid);

    MemcachedService_setRequest_pargs args;
    args.key = &key;
    args.value = &value;
    args.write(oprot_);

    oprot_->writeMessageEnd();
    oprot_->getTransport()->writeEnd();
    oprot_->getTransport()->flush();
  }

  bool MemcachedServiceClient::recv_setRequest()
  {

    int32_t rseqid = 0;
    std::string fname;
    ::my_thrift::apache::thrift::protocol::TMessageType mtype;

    iprot_->readMessageBegin(fname, mtype, rseqid);
    if (mtype == ::my_thrift::apache::thrift::protocol::T_EXCEPTION) {
      ::my_thrift::apache::thrift::TApplicationException x;
      x.read(iprot_);
      iprot_->readMessageEnd();
      iprot_->getTransport()->readEnd();
      throw x;
    }
    if (mtype != ::my_thrift::apache::thrift::protocol::T_REPLY) {
      iprot_->skip(::my_thrift::apache::thrift::protocol::T_STRUCT);
      iprot_->readMessageEnd();
      iprot_->getTransport()->readEnd();
    }
    if (fname.compare("setRequest") != 0) {
      iprot_->skip(::my_thrift::apache::thrift::protocol::T_STRUCT);
      iprot_->readMessageEnd();
      iprot_->getTransport()->readEnd();
    }
    bool _return;
    MemcachedService_setRequest_presult result;
    result.success = &_return;
    result.read(iprot_);
    iprot_->readMessageEnd();
    iprot_->getTransport()->readEnd();

    if (result.__isset.success) {
      return _return;
    }
    throw ::my_thrift::apache::thrift::TApplicationException(::my_thrift::apache::thrift::TApplicationException::MISSING_RESULT, "setRequest failed: unknown result");
  }

  bool MemcachedServiceProcessor::dispatchCall(::my_thrift::apache::thrift::protocol::TProtocol* iprot, ::my_thrift::apache::thrift::protocol::TProtocol* oprot, const std::string& fname, int32_t seqid, void* callContext) {
    ProcessMap::iterator pfn;
    pfn = processMap_.find(fname);
    if (pfn == processMap_.end()) {
      iprot->skip(::my_thrift::apache::thrift::protocol::T_STRUCT);
      iprot->readMessageEnd();
      iprot->getTransport()->readEnd();
      ::my_thrift::apache::thrift::TApplicationException x(::my_thrift::apache::thrift::TApplicationException::UNKNOWN_METHOD, "Invalid method name: '"+fname+"'");
      oprot->writeMessageBegin(fname, ::my_thrift::apache::thrift::protocol::T_EXCEPTION, seqid);
      x.write(oprot);
      oprot->writeMessageEnd();
      oprot->getTransport()->writeEnd();
      oprot->getTransport()->flush();
      return true;
    }
    (this->*(pfn->second))(seqid, iprot, oprot, callContext);
    return true;
  }

  void MemcachedServiceProcessor::process_getRequest(int32_t seqid, ::my_thrift::apache::thrift::protocol::TProtocol* iprot, ::my_thrift::apache::thrift::protocol::TProtocol* oprot, void* callContext)
  {
    void* ctx = nullptr;
    if (this->eventHandler_.get() != nullptr) {
      ctx = this->eventHandler_->getContext("MemcachedService.getRequest", callContext);
    }
    ::my_thrift::apache::thrift::TProcessorContextFreer freer(this->eventHandler_.get(), ctx, "MemcachedService.getRequest");

    if (this->eventHandler_.get() != nullptr) {
      this->eventHandler_->preRead(ctx, "MemcachedService.getRequest");
    }

    MemcachedService_getRequest_args args;
    args.read(iprot);
    iprot->readMessageEnd();
    uint32_t bytes = iprot->getTransport()->readEnd();

    if (this->eventHandler_.get() != nullptr) {
      this->eventHandler_->postRead(ctx, "MemcachedService.getRequest", bytes);
    }

    MemcachedService_getRequest_result result;
    try {
      iface_->getRequest(result.success, args.key);
      //printf("In process_getRequest, because value returned!\n");
      result.__isset.success = true;
    } catch (const std::exception& e) {
      if (this->eventHandler_.get() != nullptr) {
        this->eventHandler_->handlerError(ctx, "MemcachedService.getRequest");
      }

      ::my_thrift::apache::thrift::TApplicationException x(e.what());
      oprot->writeMessageBegin("getRequest", ::my_thrift::apache::thrift::protocol::T_EXCEPTION, seqid);
      x.write(oprot);
      oprot->writeMessageEnd();
      oprot->getTransport()->writeEnd();
      oprot->getTransport()->flush();
      return;
    }

    if (this->eventHandler_.get() != nullptr) {
      this->eventHandler_->preWrite(ctx, "MemcachedService.getRequest");
    }

    oprot->writeMessageBegin("getRequest", ::my_thrift::apache::thrift::protocol::T_REPLY, seqid);
    result.write(oprot);
    oprot->writeMessageEnd();
    bytes = oprot->getTransport()->writeEnd();
    oprot->getTransport()->flush();

    if (this->eventHandler_.get() != nullptr) {
      this->eventHandler_->postWrite(ctx, "MemcachedService.getRequest", bytes);
    }
  }

  void MemcachedServiceProcessor::process_setRequest(int32_t seqid, ::my_thrift::apache::thrift::protocol::TProtocol* iprot, ::my_thrift::apache::thrift::protocol::TProtocol* oprot, void* callContext)
  {
    void* ctx = nullptr;
    if (this->eventHandler_.get() != nullptr) {
      ctx = this->eventHandler_->getContext("MemcachedService.setRequest", callContext);
    }
    ::my_thrift::apache::thrift::TProcessorContextFreer freer(this->eventHandler_.get(), ctx, "MemcachedService.setRequest");

    if (this->eventHandler_.get() != nullptr) {
      this->eventHandler_->preRead(ctx, "MemcachedService.setRequest");
    }

    MemcachedService_setRequest_args args;
    args.read(iprot);
    iprot->readMessageEnd();
    uint32_t bytes = iprot->getTransport()->readEnd();

    if (this->eventHandler_.get() != nullptr) {
      this->eventHandler_->postRead(ctx, "MemcachedService.setRequest", bytes);
    }

    MemcachedService_setRequest_result result;
    try {
      result.success = iface_->setRequest(args.key, args.value);
      //printf("In process_setRequest, because value returned!\n");
      result.__isset.success = true;
    } catch (const std::exception& e) {
      if (this->eventHandler_.get() != nullptr) {
        this->eventHandler_->handlerError(ctx, "MemcachedService.setRequest");
      }

      ::my_thrift::apache::thrift::TApplicationException x(e.what());
      oprot->writeMessageBegin("setRequest", ::my_thrift::apache::thrift::protocol::T_EXCEPTION, seqid);
      x.write(oprot);
      oprot->writeMessageEnd();
      oprot->getTransport()->writeEnd();
      oprot->getTransport()->flush();
      return;
    }

    if (this->eventHandler_.get() != nullptr) {
      this->eventHandler_->preWrite(ctx, "MemcachedService.setRequest");
    }

    oprot->writeMessageBegin("setRequest", ::my_thrift::apache::thrift::protocol::T_REPLY, seqid);
    result.write(oprot);
    oprot->writeMessageEnd();
    bytes = oprot->getTransport()->writeEnd();
    oprot->getTransport()->flush();

    if (this->eventHandler_.get() != nullptr) {
      this->eventHandler_->postWrite(ctx, "MemcachedService.setRequest", bytes);
    }
  }

  ::std::shared_ptr< ::my_thrift::apache::thrift::TProcessor > MemcachedServiceProcessorFactory::getProcessor(const ::my_thrift::apache::thrift::TConnectionInfo& connInfo) {
    ::my_thrift::apache::thrift::ReleaseHandler< MemcachedServiceIfFactory > cleanup(handlerFactory_);
    ::std::shared_ptr< MemcachedServiceIf > handler(handlerFactory_->getHandler(connInfo), cleanup);
    ::std::shared_ptr< ::my_thrift::apache::thrift::TProcessor > processor(new MemcachedServiceProcessor(handler));
    return processor;
  }

  void MemcachedServiceConcurrentClient::getRequest(std::string& _return, const std::string& key)
  {
    int32_t seqid = send_getRequest(key);
    recv_getRequest(_return, seqid);
  }

  int32_t MemcachedServiceConcurrentClient::send_getRequest(const std::string& key)
  {
    int32_t cseqid = this->sync_->generateSeqId();
    ::my_thrift::apache::thrift::async::TConcurrentSendSentry sentry(this->sync_.get());
    oprot_->writeMessageBegin("getRequest", ::my_thrift::apache::thrift::protocol::T_CALL, cseqid);

    MemcachedService_getRequest_pargs args;
    args.key = &key;
    args.write(oprot_);

    oprot_->writeMessageEnd();
    oprot_->getTransport()->writeEnd();
    oprot_->getTransport()->flush();

    sentry.commit();
    return cseqid;
  }

  void MemcachedServiceConcurrentClient::recv_getRequest(std::string& _return, const int32_t seqid)
  {

    int32_t rseqid = 0;
    std::string fname;
    ::my_thrift::apache::thrift::protocol::TMessageType mtype;

    // the read mutex gets dropped and reacquired as part of waitForWork()
    // The destructor of this sentry wakes up other clients
    ::my_thrift::apache::thrift::async::TConcurrentRecvSentry sentry(this->sync_.get(), seqid);

    while(true) {
      if(!this->sync_->getPending(fname, mtype, rseqid)) {
        iprot_->readMessageBegin(fname, mtype, rseqid);
      }
      if(seqid == rseqid) {
        if (mtype == ::my_thrift::apache::thrift::protocol::T_EXCEPTION) {
          ::my_thrift::apache::thrift::TApplicationException x;
          x.read(iprot_);
          iprot_->readMessageEnd();
          iprot_->getTransport()->readEnd();
          sentry.commit();
          throw x;
        }
        if (mtype != ::my_thrift::apache::thrift::protocol::T_REPLY) {
          iprot_->skip(::my_thrift::apache::thrift::protocol::T_STRUCT);
          iprot_->readMessageEnd();
          iprot_->getTransport()->readEnd();
        }
        if (fname.compare("getRequest") != 0) {
          iprot_->skip(::my_thrift::apache::thrift::protocol::T_STRUCT);
          iprot_->readMessageEnd();
          iprot_->getTransport()->readEnd();

          // in a bad state, don't commit
          using ::my_thrift::apache::thrift::protocol::TProtocolException;
          throw TProtocolException(TProtocolException::INVALID_DATA);
        }
        MemcachedService_getRequest_presult result;
        result.success = &_return;
        result.read(iprot_);
        iprot_->readMessageEnd();
        iprot_->getTransport()->readEnd();

        if (result.__isset.success) {
          // _return pointer has now been filled
          sentry.commit();
          return;
        }
        // in a bad state, don't commit
        throw ::my_thrift::apache::thrift::TApplicationException(::my_thrift::apache::thrift::TApplicationException::MISSING_RESULT, "getRequest failed: unknown result");
      }
      // seqid != rseqid
      this->sync_->updatePending(fname, mtype, rseqid);

      // this will temporarily unlock the readMutex, and let other clients get work done
      this->sync_->waitForWork(seqid);
    } // end while(true)
  }

  bool MemcachedServiceConcurrentClient::setRequest(const std::string& key, const std::string& value)
  {
    int32_t seqid = send_setRequest(key, value);
    return recv_setRequest(seqid);
  }

  int32_t MemcachedServiceConcurrentClient::send_setRequest(const std::string& key, const std::string& value)
  {
    int32_t cseqid = this->sync_->generateSeqId();
    ::my_thrift::apache::thrift::async::TConcurrentSendSentry sentry(this->sync_.get());
    oprot_->writeMessageBegin("setRequest", ::my_thrift::apache::thrift::protocol::T_CALL, cseqid);

    MemcachedService_setRequest_pargs args;
    args.key = &key;
    args.value = &value;
    args.write(oprot_);

    oprot_->writeMessageEnd();
    oprot_->getTransport()->writeEnd();
    oprot_->getTransport()->flush();

    sentry.commit();
    return cseqid;
  }

  bool MemcachedServiceConcurrentClient::recv_setRequest(const int32_t seqid)
  {

    int32_t rseqid = 0;
    std::string fname;
    ::my_thrift::apache::thrift::protocol::TMessageType mtype;

    // the read mutex gets dropped and reacquired as part of waitForWork()
    // The destructor of this sentry wakes up other clients
    ::my_thrift::apache::thrift::async::TConcurrentRecvSentry sentry(this->sync_.get(), seqid);

    while(true) {
      if(!this->sync_->getPending(fname, mtype, rseqid)) {
        iprot_->readMessageBegin(fname, mtype, rseqid);
      }
      if(seqid == rseqid) {
        if (mtype == ::my_thrift::apache::thrift::protocol::T_EXCEPTION) {
          ::my_thrift::apache::thrift::TApplicationException x;
          x.read(iprot_);
          iprot_->readMessageEnd();
          iprot_->getTransport()->readEnd();
          sentry.commit();
          throw x;
        }
        if (mtype != ::my_thrift::apache::thrift::protocol::T_REPLY) {
          iprot_->skip(::my_thrift::apache::thrift::protocol::T_STRUCT);
          iprot_->readMessageEnd();
          iprot_->getTransport()->readEnd();
        }
        if (fname.compare("setRequest") != 0) {
          iprot_->skip(::my_thrift::apache::thrift::protocol::T_STRUCT);
          iprot_->readMessageEnd();
          iprot_->getTransport()->readEnd();

          // in a bad state, don't commit
          using ::my_thrift::apache::thrift::protocol::TProtocolException;
          throw TProtocolException(TProtocolException::INVALID_DATA);
        }
        bool _return;
        MemcachedService_setRequest_presult result;
        result.success = &_return;
        result.read(iprot_);
        iprot_->readMessageEnd();
        iprot_->getTransport()->readEnd();

        if (result.__isset.success) {
          sentry.commit();
          return _return;
        }
        // in a bad state, don't commit
        throw ::my_thrift::apache::thrift::TApplicationException(::my_thrift::apache::thrift::TApplicationException::MISSING_RESULT, "setRequest failed: unknown result");
      }
      // seqid != rseqid
      this->sync_->updatePending(fname, mtype, rseqid);

      // this will temporarily unlock the readMutex, and let other clients get work done
      this->sync_->waitForWork(seqid);
    } // end while(true)
  }
  // ***MemcachedServer.cpp(Actual Service Handler)***
  // Modified RPC handler
  
  using namespace shared;
  class MemcachedServiceHandler : virtual public MemcachedServiceIf {
  public:
      void getRequest(std::string& _return, const std::string& key) override {
          // Create message
          auto msg = std::make_shared<MemcachedMessage>();
          msg->type = MemcachedMessage::GET;
          msg->key = key;

          // Get future before pushing message
          auto future = msg->completed.get_future();

          // Push to queue
          MessageQueue::getInstance().pushRequest(msg);

          // Wait for response
          future.wait();
          _return = msg->value;
      }

      bool setRequest(const std::string& key, const std::string& value) override {
          auto msg = std::make_shared<MemcachedMessage>();
          msg->type = MemcachedMessage::SET;
          msg->key = key;
          msg->value = value;

          auto future = msg->completed.get_future();
          MessageQueue::getInstance().pushRequest(msg);

          future.wait();
          return msg->success;
      }
   };
} // namespace thrift_memcached

// Separate business logic handler
using namespace shared;
class MemcachedBusinessLogic {
private:
    using memcached_ptr = std::unique_ptr<memcached_st, decltype(&memcached_free)>;
    memcached_ptr memc;

public:
    MemcachedBusinessLogic() : memc(memcached_create(nullptr), memcached_free) {
        // Initialize memcached connection
        memcached_server_st* servers = memcached_server_list_append(nullptr, "localhost", 11211, nullptr);
        memcached_server_push(memc.get(), servers);
        memcached_server_list_free(servers);
    }

    void processMessages() {
        while(true) {
            auto msg = MessageQueue::getInstance().getRequest();
            switch(msg->type) {
                case MemcachedMessage::GET: {
                    // Process get request
                    //printf("got message: %d msg->key: %s\n", msg->type, msg->key.c_str());
                    size_t value_length;
                    uint32_t flags;
                    memcached_return_t rc;
                    char* value = memcached_get(memc.get(),
                                              msg->key.c_str(),
                                              msg->key.length(),
                                              &value_length,
                                              &flags,
                                              &rc);

                    if (rc == MEMCACHED_SUCCESS && value != nullptr) {
                        msg->value.assign(value, value_length);
                        free(value);
                    } else {
                        msg->value = "Key not found";
                    }
                    msg->completed.set_value();
                    break;
                }

                case MemcachedMessage::SET: {
                    // Process set request
                    //printf("got message: %d msg->key: %s msg->value %s\n", msg->type, msg->key.c_str(), msg->value.c_str());
                    memcached_return_t rc = memcached_set(memc.get(),
                                                        msg->key.c_str(),
                                                        msg->key.length(),
                                                        msg->value.c_str(),
                                                        msg->value.length(),
                                                        (time_t)0,
                                                        (uint32_t)0);

                    msg->success = (rc == MEMCACHED_SUCCESS);
                    //printf("memcached_set returned: %s\n", memcached_strerror(memc.get(), rc));
                    msg->completed.set_value();
                    break;
                }
            }
        }
    }
};

int main(int argc, char **argv) {
    // Initialize DPDK first
    auto& dpdk = DPDKHandler::getInstance();
    if (!dpdk.init(0)) {
        std::cerr << "Failed to initialize DPDK" << std::endl;
        return 1;
    }

    // Start DPDK polling
    dpdk.startPolling();

    // Start business logic thread
    MemcachedBusinessLogic logic;
    std::thread business_thread(&MemcachedBusinessLogic::processMessages, &logic);

    using namespace thrift_memcached;
    using namespace my_thrift::apache::thrift;
    using namespace my_thrift::apache::thrift::protocol;
    using namespace my_thrift::apache::thrift::transport;
    using namespace my_thrift::apache::thrift::server;
    
    int port = 9090;
    ::std::shared_ptr<MemcachedServiceHandler> handler(new MemcachedServiceHandler());
    ::std::shared_ptr<TProcessor> processor(new MemcachedServiceProcessor(handler));
    ::std::shared_ptr<TServerTransport> serverTransport(new TServerUDPSocket(port));
    ::std::shared_ptr<TTransportFactory> transportFactory(new TBufferedTransportFactory());
    ::std::shared_ptr<TProtocolFactory> protocolFactory(new TBinaryProtocolFactory());

    TSimpleServer server(processor, serverTransport, transportFactory, protocolFactory);
    //TThreadedServer server(processor, serverTransport, transportFactory, protocolFactory);
    std::cout << "Starting the server on port " << port << "..." << std::endl;
    server.serve();

    // Cleanup DPDK
    //dpdk.stopPolling();

    return 0;
}
