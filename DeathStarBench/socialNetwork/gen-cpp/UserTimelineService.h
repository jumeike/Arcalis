/**
 * Autogenerated by Thrift Compiler (0.22.0)
 *
 * DO NOT EDIT UNLESS YOU ARE SURE THAT YOU KNOW WHAT YOU ARE DOING
 *  @generated
 */
#ifndef UserTimelineService_H
#define UserTimelineService_H

#include <thrift/TDispatchProcessor.h>
#include <thrift/async/TConcurrentClientSyncInfo.h>
#include <memory>
#include "social_network_types.h"

namespace social_network {

#ifdef _MSC_VER
  #pragma warning( push )
  #pragma warning (disable : 4250 ) //inheriting methods via dominance 
#endif

class UserTimelineServiceIf {
 public:
  virtual ~UserTimelineServiceIf() {}
  virtual void WriteUserTimeline(const int64_t req_id, const int64_t post_id, const int64_t user_id, const int64_t timestamp, const std::map<std::string, std::string> & carrier) = 0;
  virtual void ReadUserTimeline(std::vector<Post> & _return, const int64_t req_id, const int64_t user_id, const int32_t start, const int32_t stop, const std::map<std::string, std::string> & carrier) = 0;
};

class UserTimelineServiceIfFactory {
 public:
  typedef UserTimelineServiceIf Handler;

  virtual ~UserTimelineServiceIfFactory() {}

  virtual UserTimelineServiceIf* getHandler(const ::apache::thrift::TConnectionInfo& connInfo) = 0;
  virtual void releaseHandler(UserTimelineServiceIf* /* handler */) = 0;
  };

class UserTimelineServiceIfSingletonFactory : virtual public UserTimelineServiceIfFactory {
 public:
  UserTimelineServiceIfSingletonFactory(const ::std::shared_ptr<UserTimelineServiceIf>& iface) : iface_(iface) {}
  virtual ~UserTimelineServiceIfSingletonFactory() {}

  virtual UserTimelineServiceIf* getHandler(const ::apache::thrift::TConnectionInfo&) override {
    return iface_.get();
  }
  virtual void releaseHandler(UserTimelineServiceIf* /* handler */) override {}

 protected:
  ::std::shared_ptr<UserTimelineServiceIf> iface_;
};

class UserTimelineServiceNull : virtual public UserTimelineServiceIf {
 public:
  virtual ~UserTimelineServiceNull() {}
  void WriteUserTimeline(const int64_t /* req_id */, const int64_t /* post_id */, const int64_t /* user_id */, const int64_t /* timestamp */, const std::map<std::string, std::string> & /* carrier */) override {
    return;
  }
  void ReadUserTimeline(std::vector<Post> & /* _return */, const int64_t /* req_id */, const int64_t /* user_id */, const int32_t /* start */, const int32_t /* stop */, const std::map<std::string, std::string> & /* carrier */) override {
    return;
  }
};

typedef struct _UserTimelineService_WriteUserTimeline_args__isset {
  _UserTimelineService_WriteUserTimeline_args__isset() : req_id(false), post_id(false), user_id(false), timestamp(false), carrier(false) {}
  bool req_id :1;
  bool post_id :1;
  bool user_id :1;
  bool timestamp :1;
  bool carrier :1;
} _UserTimelineService_WriteUserTimeline_args__isset;

class UserTimelineService_WriteUserTimeline_args {
 public:

  UserTimelineService_WriteUserTimeline_args(const UserTimelineService_WriteUserTimeline_args&);
  UserTimelineService_WriteUserTimeline_args& operator=(const UserTimelineService_WriteUserTimeline_args&);
  UserTimelineService_WriteUserTimeline_args() noexcept;

  virtual ~UserTimelineService_WriteUserTimeline_args() noexcept;
  int64_t req_id;
  int64_t post_id;
  int64_t user_id;
  int64_t timestamp;
  std::map<std::string, std::string>  carrier;

  _UserTimelineService_WriteUserTimeline_args__isset __isset;

  void __set_req_id(const int64_t val);

  void __set_post_id(const int64_t val);

  void __set_user_id(const int64_t val);

  void __set_timestamp(const int64_t val);

  void __set_carrier(const std::map<std::string, std::string> & val);

  bool operator == (const UserTimelineService_WriteUserTimeline_args & rhs) const;
  bool operator != (const UserTimelineService_WriteUserTimeline_args &rhs) const {
    return !(*this == rhs);
  }

  bool operator < (const UserTimelineService_WriteUserTimeline_args & ) const;

  uint32_t read(::apache::thrift::protocol::TProtocol* iprot);
  uint32_t write(::apache::thrift::protocol::TProtocol* oprot) const;

};


class UserTimelineService_WriteUserTimeline_pargs {
 public:


  virtual ~UserTimelineService_WriteUserTimeline_pargs() noexcept;
  const int64_t* req_id;
  const int64_t* post_id;
  const int64_t* user_id;
  const int64_t* timestamp;
  const std::map<std::string, std::string> * carrier;

  uint32_t write(::apache::thrift::protocol::TProtocol* oprot) const;

};

typedef struct _UserTimelineService_WriteUserTimeline_result__isset {
  _UserTimelineService_WriteUserTimeline_result__isset() : se(false) {}
  bool se :1;
} _UserTimelineService_WriteUserTimeline_result__isset;

class UserTimelineService_WriteUserTimeline_result {
 public:

  UserTimelineService_WriteUserTimeline_result(const UserTimelineService_WriteUserTimeline_result&);
  UserTimelineService_WriteUserTimeline_result& operator=(const UserTimelineService_WriteUserTimeline_result&);
  UserTimelineService_WriteUserTimeline_result() noexcept;

  virtual ~UserTimelineService_WriteUserTimeline_result() noexcept;
  ServiceException se;

  _UserTimelineService_WriteUserTimeline_result__isset __isset;

  void __set_se(const ServiceException& val);

  bool operator == (const UserTimelineService_WriteUserTimeline_result & rhs) const;
  bool operator != (const UserTimelineService_WriteUserTimeline_result &rhs) const {
    return !(*this == rhs);
  }

  bool operator < (const UserTimelineService_WriteUserTimeline_result & ) const;

  uint32_t read(::apache::thrift::protocol::TProtocol* iprot);
  uint32_t write(::apache::thrift::protocol::TProtocol* oprot) const;

};

typedef struct _UserTimelineService_WriteUserTimeline_presult__isset {
  _UserTimelineService_WriteUserTimeline_presult__isset() : se(false) {}
  bool se :1;
} _UserTimelineService_WriteUserTimeline_presult__isset;

class UserTimelineService_WriteUserTimeline_presult {
 public:


  virtual ~UserTimelineService_WriteUserTimeline_presult() noexcept;
  ServiceException se;

  _UserTimelineService_WriteUserTimeline_presult__isset __isset;

  uint32_t read(::apache::thrift::protocol::TProtocol* iprot);

};

typedef struct _UserTimelineService_ReadUserTimeline_args__isset {
  _UserTimelineService_ReadUserTimeline_args__isset() : req_id(false), user_id(false), start(false), stop(false), carrier(false) {}
  bool req_id :1;
  bool user_id :1;
  bool start :1;
  bool stop :1;
  bool carrier :1;
} _UserTimelineService_ReadUserTimeline_args__isset;

class UserTimelineService_ReadUserTimeline_args {
 public:

  UserTimelineService_ReadUserTimeline_args(const UserTimelineService_ReadUserTimeline_args&);
  UserTimelineService_ReadUserTimeline_args& operator=(const UserTimelineService_ReadUserTimeline_args&);
  UserTimelineService_ReadUserTimeline_args() noexcept;

  virtual ~UserTimelineService_ReadUserTimeline_args() noexcept;
  int64_t req_id;
  int64_t user_id;
  int32_t start;
  int32_t stop;
  std::map<std::string, std::string>  carrier;

  _UserTimelineService_ReadUserTimeline_args__isset __isset;

  void __set_req_id(const int64_t val);

  void __set_user_id(const int64_t val);

  void __set_start(const int32_t val);

  void __set_stop(const int32_t val);

  void __set_carrier(const std::map<std::string, std::string> & val);

  bool operator == (const UserTimelineService_ReadUserTimeline_args & rhs) const;
  bool operator != (const UserTimelineService_ReadUserTimeline_args &rhs) const {
    return !(*this == rhs);
  }

  bool operator < (const UserTimelineService_ReadUserTimeline_args & ) const;

  uint32_t read(::apache::thrift::protocol::TProtocol* iprot);
  uint32_t write(::apache::thrift::protocol::TProtocol* oprot) const;

};


class UserTimelineService_ReadUserTimeline_pargs {
 public:


  virtual ~UserTimelineService_ReadUserTimeline_pargs() noexcept;
  const int64_t* req_id;
  const int64_t* user_id;
  const int32_t* start;
  const int32_t* stop;
  const std::map<std::string, std::string> * carrier;

  uint32_t write(::apache::thrift::protocol::TProtocol* oprot) const;

};

typedef struct _UserTimelineService_ReadUserTimeline_result__isset {
  _UserTimelineService_ReadUserTimeline_result__isset() : success(false), se(false) {}
  bool success :1;
  bool se :1;
} _UserTimelineService_ReadUserTimeline_result__isset;

class UserTimelineService_ReadUserTimeline_result {
 public:

  UserTimelineService_ReadUserTimeline_result(const UserTimelineService_ReadUserTimeline_result&);
  UserTimelineService_ReadUserTimeline_result& operator=(const UserTimelineService_ReadUserTimeline_result&);
  UserTimelineService_ReadUserTimeline_result() noexcept;

  virtual ~UserTimelineService_ReadUserTimeline_result() noexcept;
  std::vector<Post>  success;
  ServiceException se;

  _UserTimelineService_ReadUserTimeline_result__isset __isset;

  void __set_success(const std::vector<Post> & val);

  void __set_se(const ServiceException& val);

  bool operator == (const UserTimelineService_ReadUserTimeline_result & rhs) const;
  bool operator != (const UserTimelineService_ReadUserTimeline_result &rhs) const {
    return !(*this == rhs);
  }

  bool operator < (const UserTimelineService_ReadUserTimeline_result & ) const;

  uint32_t read(::apache::thrift::protocol::TProtocol* iprot);
  uint32_t write(::apache::thrift::protocol::TProtocol* oprot) const;

};

typedef struct _UserTimelineService_ReadUserTimeline_presult__isset {
  _UserTimelineService_ReadUserTimeline_presult__isset() : success(false), se(false) {}
  bool success :1;
  bool se :1;
} _UserTimelineService_ReadUserTimeline_presult__isset;

class UserTimelineService_ReadUserTimeline_presult {
 public:


  virtual ~UserTimelineService_ReadUserTimeline_presult() noexcept;
  std::vector<Post> * success;
  ServiceException se;

  _UserTimelineService_ReadUserTimeline_presult__isset __isset;

  uint32_t read(::apache::thrift::protocol::TProtocol* iprot);

};

class UserTimelineServiceClient : virtual public UserTimelineServiceIf {
 public:
  UserTimelineServiceClient(std::shared_ptr< ::apache::thrift::protocol::TProtocol> prot) {
    setProtocol(prot);
  }
  UserTimelineServiceClient(std::shared_ptr< ::apache::thrift::protocol::TProtocol> iprot, std::shared_ptr< ::apache::thrift::protocol::TProtocol> oprot) {
    setProtocol(iprot,oprot);
  }
 private:
  void setProtocol(std::shared_ptr< ::apache::thrift::protocol::TProtocol> prot) {
  setProtocol(prot,prot);
  }
  void setProtocol(std::shared_ptr< ::apache::thrift::protocol::TProtocol> iprot, std::shared_ptr< ::apache::thrift::protocol::TProtocol> oprot) {
    piprot_=iprot;
    poprot_=oprot;
    iprot_ = iprot.get();
    oprot_ = oprot.get();
  }
 public:
  std::shared_ptr< ::apache::thrift::protocol::TProtocol> getInputProtocol() {
    return piprot_;
  }
  std::shared_ptr< ::apache::thrift::protocol::TProtocol> getOutputProtocol() {
    return poprot_;
  }
  void WriteUserTimeline(const int64_t req_id, const int64_t post_id, const int64_t user_id, const int64_t timestamp, const std::map<std::string, std::string> & carrier) override;
  void send_WriteUserTimeline(const int64_t req_id, const int64_t post_id, const int64_t user_id, const int64_t timestamp, const std::map<std::string, std::string> & carrier);
  void recv_WriteUserTimeline();
  void ReadUserTimeline(std::vector<Post> & _return, const int64_t req_id, const int64_t user_id, const int32_t start, const int32_t stop, const std::map<std::string, std::string> & carrier) override;
  void send_ReadUserTimeline(const int64_t req_id, const int64_t user_id, const int32_t start, const int32_t stop, const std::map<std::string, std::string> & carrier);
  void recv_ReadUserTimeline(std::vector<Post> & _return);
 protected:
  std::shared_ptr< ::apache::thrift::protocol::TProtocol> piprot_;
  std::shared_ptr< ::apache::thrift::protocol::TProtocol> poprot_;
  ::apache::thrift::protocol::TProtocol* iprot_;
  ::apache::thrift::protocol::TProtocol* oprot_;
};

class UserTimelineServiceProcessor : public ::apache::thrift::TDispatchProcessor {
 protected:
  ::std::shared_ptr<UserTimelineServiceIf> iface_;
  virtual bool dispatchCall(::apache::thrift::protocol::TProtocol* iprot, ::apache::thrift::protocol::TProtocol* oprot, const std::string& fname, int32_t seqid, void* callContext) override;
 private:
  typedef  void (UserTimelineServiceProcessor::*ProcessFunction)(int32_t, ::apache::thrift::protocol::TProtocol*, ::apache::thrift::protocol::TProtocol*, void*);
  typedef std::map<std::string, ProcessFunction> ProcessMap;
  ProcessMap processMap_;
  void process_WriteUserTimeline(int32_t seqid, ::apache::thrift::protocol::TProtocol* iprot, ::apache::thrift::protocol::TProtocol* oprot, void* callContext);
  void process_ReadUserTimeline(int32_t seqid, ::apache::thrift::protocol::TProtocol* iprot, ::apache::thrift::protocol::TProtocol* oprot, void* callContext);
 public:
  UserTimelineServiceProcessor(::std::shared_ptr<UserTimelineServiceIf> iface) :
    iface_(iface) {
    processMap_["WriteUserTimeline"] = &UserTimelineServiceProcessor::process_WriteUserTimeline;
    processMap_["ReadUserTimeline"] = &UserTimelineServiceProcessor::process_ReadUserTimeline;
  }

  virtual ~UserTimelineServiceProcessor() {}
};

class UserTimelineServiceProcessorFactory : public ::apache::thrift::TProcessorFactory {
 public:
  UserTimelineServiceProcessorFactory(const ::std::shared_ptr< UserTimelineServiceIfFactory >& handlerFactory) noexcept :
      handlerFactory_(handlerFactory) {}

  ::std::shared_ptr< ::apache::thrift::TProcessor > getProcessor(const ::apache::thrift::TConnectionInfo& connInfo) override;

 protected:
  ::std::shared_ptr< UserTimelineServiceIfFactory > handlerFactory_;
};

class UserTimelineServiceMultiface : virtual public UserTimelineServiceIf {
 public:
  UserTimelineServiceMultiface(std::vector<std::shared_ptr<UserTimelineServiceIf> >& ifaces) : ifaces_(ifaces) {
  }
  virtual ~UserTimelineServiceMultiface() {}
 protected:
  std::vector<std::shared_ptr<UserTimelineServiceIf> > ifaces_;
  UserTimelineServiceMultiface() {}
  void add(::std::shared_ptr<UserTimelineServiceIf> iface) {
    ifaces_.push_back(iface);
  }
 public:
  void WriteUserTimeline(const int64_t req_id, const int64_t post_id, const int64_t user_id, const int64_t timestamp, const std::map<std::string, std::string> & carrier) override {
    size_t sz = ifaces_.size();
    size_t i = 0;
    for (; i < (sz - 1); ++i) {
      ifaces_[i]->WriteUserTimeline(req_id, post_id, user_id, timestamp, carrier);
    }
    ifaces_[i]->WriteUserTimeline(req_id, post_id, user_id, timestamp, carrier);
  }

  void ReadUserTimeline(std::vector<Post> & _return, const int64_t req_id, const int64_t user_id, const int32_t start, const int32_t stop, const std::map<std::string, std::string> & carrier) override {
    size_t sz = ifaces_.size();
    size_t i = 0;
    for (; i < (sz - 1); ++i) {
      ifaces_[i]->ReadUserTimeline(_return, req_id, user_id, start, stop, carrier);
    }
    ifaces_[i]->ReadUserTimeline(_return, req_id, user_id, start, stop, carrier);
    return;
  }

};

// The 'concurrent' client is a thread safe client that correctly handles
// out of order responses.  It is slower than the regular client, so should
// only be used when you need to share a connection among multiple threads
class UserTimelineServiceConcurrentClient : virtual public UserTimelineServiceIf {
 public:
  UserTimelineServiceConcurrentClient(std::shared_ptr< ::apache::thrift::protocol::TProtocol> prot, std::shared_ptr< ::apache::thrift::async::TConcurrentClientSyncInfo> sync) : sync_(sync)
{
    setProtocol(prot);
  }
  UserTimelineServiceConcurrentClient(std::shared_ptr< ::apache::thrift::protocol::TProtocol> iprot, std::shared_ptr< ::apache::thrift::protocol::TProtocol> oprot, std::shared_ptr< ::apache::thrift::async::TConcurrentClientSyncInfo> sync) : sync_(sync)
{
    setProtocol(iprot,oprot);
  }
 private:
  void setProtocol(std::shared_ptr< ::apache::thrift::protocol::TProtocol> prot) {
  setProtocol(prot,prot);
  }
  void setProtocol(std::shared_ptr< ::apache::thrift::protocol::TProtocol> iprot, std::shared_ptr< ::apache::thrift::protocol::TProtocol> oprot) {
    piprot_=iprot;
    poprot_=oprot;
    iprot_ = iprot.get();
    oprot_ = oprot.get();
  }
 public:
  std::shared_ptr< ::apache::thrift::protocol::TProtocol> getInputProtocol() {
    return piprot_;
  }
  std::shared_ptr< ::apache::thrift::protocol::TProtocol> getOutputProtocol() {
    return poprot_;
  }
  void WriteUserTimeline(const int64_t req_id, const int64_t post_id, const int64_t user_id, const int64_t timestamp, const std::map<std::string, std::string> & carrier) override;
  int32_t send_WriteUserTimeline(const int64_t req_id, const int64_t post_id, const int64_t user_id, const int64_t timestamp, const std::map<std::string, std::string> & carrier);
  void recv_WriteUserTimeline(const int32_t seqid);
  void ReadUserTimeline(std::vector<Post> & _return, const int64_t req_id, const int64_t user_id, const int32_t start, const int32_t stop, const std::map<std::string, std::string> & carrier) override;
  int32_t send_ReadUserTimeline(const int64_t req_id, const int64_t user_id, const int32_t start, const int32_t stop, const std::map<std::string, std::string> & carrier);
  void recv_ReadUserTimeline(std::vector<Post> & _return, const int32_t seqid);
 protected:
  std::shared_ptr< ::apache::thrift::protocol::TProtocol> piprot_;
  std::shared_ptr< ::apache::thrift::protocol::TProtocol> poprot_;
  ::apache::thrift::protocol::TProtocol* iprot_;
  ::apache::thrift::protocol::TProtocol* oprot_;
  std::shared_ptr< ::apache::thrift::async::TConcurrentClientSyncInfo> sync_;
};

#ifdef _MSC_VER
  #pragma warning( pop )
#endif

} // namespace

#endif
