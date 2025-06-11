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
#ifndef _THRIFT_TDISPATCHPROCESSOR_H_
#define _THRIFT_TDISPATCHPROCESSOR_H_ 1

#include <thrift/TProcessor.h>

namespace apache {
namespace thrift {

/**
 * TDispatchProcessor is a helper class to parse the message header then call
 * another function to dispatch based on the function name.
 *
 * Subclasses must implement dispatchCall() to dispatch on the function name.
 */
template <class Protocol_>
class TDispatchProcessorT : public TProcessor {
public:
  bool process(std::shared_ptr<::apache::thrift::protocol::TProtocol> in,
                       std::shared_ptr<::apache::thrift::protocol::TProtocol> out,
                       void* connectionContext) override {
    ::apache::thrift::protocol::TProtocol* inRaw = in.get();
    ::apache::thrift::protocol::TProtocol* outRaw = out.get();

    // Try to dynamic cast to the template protocol type
    auto* specificIn = dynamic_cast<Protocol_*>(inRaw);
    auto* specificOut = dynamic_cast<Protocol_*>(outRaw);
    if (specificIn && specificOut) {
      return processFast(specificIn, specificOut, connectionContext);
    }

    // Log the fact that we have to use the slow path
    T_GENERIC_PROTOCOL(this, inRaw, specificIn);
    T_GENERIC_PROTOCOL(this, outRaw, specificOut);

    std::string fname;
    ::apache::thrift::protocol::TMessageType mtype;
    int32_t seqid;
    inRaw->readMessageBegin(fname, mtype, seqid);

    // If this doesn't look like a valid call, log an error and return false so
    // that the server will close the connection.
    //
    // (The old generated processor code used to try to skip a T_STRUCT and
    // continue.  However, that seems unsafe.)
    if (mtype != ::apache::thrift::protocol::T_CALL && mtype != ::apache::thrift::protocol::T_ONEWAY) {
      ::apache::thrift::GlobalOutput.printf("received invalid message type %d from client", mtype);
      return false;
    }

    return this->dispatchCall(inRaw, outRaw, fname, seqid, connectionContext);
  }

protected:
  bool processFast(Protocol_* in, Protocol_* out, void* connectionContext) {
    std::string fname;
    ::apache::thrift::protocol::TMessageType mtype;
    int32_t seqid;
    in->readMessageBegin(fname, mtype, seqid);

    if (mtype != ::apache::thrift::protocol::T_CALL && mtype != ::apache::thrift::protocol::T_ONEWAY) {
      ::apache::thrift::GlobalOutput.printf("received invalid message type %d from client", mtype);
      return false;
    }

    return this->dispatchCallTemplated(in, out, fname, seqid, connectionContext);
  }

  /**
   * dispatchCall() methods must be implemented by subclasses
   */
  virtual bool dispatchCall(::apache::thrift::protocol::TProtocol* in,
                            ::apache::thrift::protocol::TProtocol* out,
                            const std::string& fname,
                            int32_t seqid,
                            void* callContext) = 0;
  
  virtual bool dispatchCallTemplated(Protocol_* in,
                                     Protocol_* out,
                                     const std::string& fname,
                                     int32_t seqid,
                                     void* callContext) = 0;
};

/**
 * Non-templatized version of TDispatchProcessor, that doesn't bother trying to
 * perform a dynamic_cast.
 */
class TDispatchProcessor : public TProcessor {
public:
#ifdef ENABLE_GEM5
  //bool runLoop(std::shared_ptr<::apache::thrift::protocol::TProtocol> in,
  bool process(std::shared_ptr<::apache::thrift::protocol::TProtocol> in,
                       std::shared_ptr<::apache::thrift::protocol::TProtocol> out,
                       void* connectionContext) override { return false; } 
#else
  bool process(std::shared_ptr<protocol::TProtocol> in,
                       std::shared_ptr<protocol::TProtocol> out,
                       void* connectionContext) override {
    std::string fname;
    ::apache::thrift::protocol::TMessageType mtype;
    int32_t seqid;
    in->readMessageBegin(fname, mtype, seqid);

    if (mtype != ::apache::thrift::protocol::T_CALL && mtype != ::apache::thrift::protocol::T_ONEWAY) {
      ::apache::thrift::GlobalOutput.printf("received invalid message type %d from client", mtype);
      return false;
    }

    return dispatchCall(in.get(), out.get(), fname, seqid, connectionContext);
  }
#endif // ENABLE_GEM5

#ifdef ENABLE_GEM5
public:
  virtual bool dispatchCall(::apache::thrift::protocol::TProtocol* in,
                            ::apache::thrift::protocol::TProtocol* out,
                            const std::string& fname,
                            int32_t seqid,
                            void* callContext) = 0;
#else 
protected:
  virtual bool dispatchCall(::apache::thrift::protocol::TProtocol* in,
                            ::apache::thrift::protocol::TProtocol* out,
                            const std::string& fname,
                            int32_t seqid,
                            void* callContext) = 0;
#endif
};

// Specialize TDispatchProcessorT for TProtocol and TDummyProtocol just to use
// the generic TDispatchProcessor.
template <>
class TDispatchProcessorT<::apache::thrift::protocol::TDummyProtocol> : public TDispatchProcessor {};
template <>
class TDispatchProcessorT<::apache::thrift::protocol::TProtocol> : public TDispatchProcessor {};
}
} // apache::thrift

#endif // _THRIFT_TDISPATCHPROCESSOR_H_
