/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements. See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership. The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License. You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied. See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#ifndef _THRIFT_TPROCESSOR_H_
#define _THRIFT_TPROCESSOR_H_ 1

#include <string>
#include <thrift/protocol/TProtocol.h>

namespace apache {
namespace thrift {

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
  apache::thrift::TProcessorEventHandler* handler_;
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

#ifdef ENABLE_GEM5
 //virtual bool runLoop(std::shared_ptr<::apache::thrift::protocol::TProtocol> in,
 virtual bool process(std::shared_ptr<::apache::thrift::protocol::TProtocol> in,
                       std::shared_ptr<::apache::thrift::protocol::TProtocol> out,
                       void* connectionContext) = 0;
#else
  virtual bool process(std::shared_ptr<protocol::TProtocol> in,
                       std::shared_ptr<protocol::TProtocol> out,
                       void* connectionContext) = 0;

  bool process(std::shared_ptr<apache::thrift::protocol::TProtocol> io, void* connectionContext) {
    return process(io, io, connectionContext);
  }
#endif // ENABLE_GEM5

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
#ifdef ENABLE_GEM5
  std::shared_ptr<::apache::thrift::protocol::TProtocol> input;
  std::shared_ptr<::apache::thrift::protocol::TProtocol> output;
  std::shared_ptr<::apache::thrift::transport::TTransport> transport;
#else
  std::shared_ptr<protocol::TProtocol> input;
  std::shared_ptr<protocol::TProtocol> output;
 // The underlying transport used for the connection
  // This is the transport that was returned by TServerTransport::accept(),
  // and it may be different than the transport pointed to by the input and
  // output protocols.
  std::shared_ptr<transport::TTransport> transport;
#endif // ENABLE_GEM5
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
}
} // apache::thrift

#endif // #ifndef _THRIFT_TPROCESSOR_H_
