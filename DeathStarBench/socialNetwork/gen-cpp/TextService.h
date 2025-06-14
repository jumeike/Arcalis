/**
 * Autogenerated by Thrift Compiler (0.22.0)
 *
 * DO NOT EDIT UNLESS YOU ARE SURE THAT YOU KNOW WHAT YOU ARE DOING
 *  @generated
 */
#ifndef TextService_H
#define TextService_H

#include <thrift/TDispatchProcessor.h>
#include <thrift/async/TConcurrentClientSyncInfo.h>
#include <memory>
#include "social_network_types.h"

namespace social_network {

#ifdef _MSC_VER
  #pragma warning( push )
  #pragma warning (disable : 4250 ) //inheriting methods via dominance 
#endif

class TextServiceIf {
 public:
  virtual ~TextServiceIf() {}
  virtual void ComposeText(TextServiceReturn& _return, const int64_t req_id, const std::string& text, const std::map<std::string, std::string> & carrier) = 0;
};

class TextServiceIfFactory {
 public:
  typedef TextServiceIf Handler;

  virtual ~TextServiceIfFactory() {}

  virtual TextServiceIf* getHandler(const ::apache::thrift::TConnectionInfo& connInfo) = 0;
  virtual void releaseHandler(TextServiceIf* /* handler */) = 0;
  };

class TextServiceIfSingletonFactory : virtual public TextServiceIfFactory {
 public:
  TextServiceIfSingletonFactory(const ::std::shared_ptr<TextServiceIf>& iface) : iface_(iface) {}
  virtual ~TextServiceIfSingletonFactory() {}

  virtual TextServiceIf* getHandler(const ::apache::thrift::TConnectionInfo&) override {
    return iface_.get();
  }
  virtual void releaseHandler(TextServiceIf* /* handler */) override {}

 protected:
  ::std::shared_ptr<TextServiceIf> iface_;
};

class TextServiceNull : virtual public TextServiceIf {
 public:
  virtual ~TextServiceNull() {}
  void ComposeText(TextServiceReturn& /* _return */, const int64_t /* req_id */, const std::string& /* text */, const std::map<std::string, std::string> & /* carrier */) override {
    return;
  }
};

typedef struct _TextService_ComposeText_args__isset {
  _TextService_ComposeText_args__isset() : req_id(false), text(false), carrier(false) {}
  bool req_id :1;
  bool text :1;
  bool carrier :1;
} _TextService_ComposeText_args__isset;

class TextService_ComposeText_args {
 public:

  TextService_ComposeText_args(const TextService_ComposeText_args&);
  TextService_ComposeText_args& operator=(const TextService_ComposeText_args&);
  TextService_ComposeText_args() noexcept;

  virtual ~TextService_ComposeText_args() noexcept;
  int64_t req_id;
  std::string text;
  std::map<std::string, std::string>  carrier;

  _TextService_ComposeText_args__isset __isset;

  void __set_req_id(const int64_t val);

  void __set_text(const std::string& val);

  void __set_carrier(const std::map<std::string, std::string> & val);

  bool operator == (const TextService_ComposeText_args & rhs) const;
  bool operator != (const TextService_ComposeText_args &rhs) const {
    return !(*this == rhs);
  }

  bool operator < (const TextService_ComposeText_args & ) const;

  uint32_t read(::apache::thrift::protocol::TProtocol* iprot);
  uint32_t write(::apache::thrift::protocol::TProtocol* oprot) const;

};


class TextService_ComposeText_pargs {
 public:


  virtual ~TextService_ComposeText_pargs() noexcept;
  const int64_t* req_id;
  const std::string* text;
  const std::map<std::string, std::string> * carrier;

  uint32_t write(::apache::thrift::protocol::TProtocol* oprot) const;

};

typedef struct _TextService_ComposeText_result__isset {
  _TextService_ComposeText_result__isset() : success(false), se(false) {}
  bool success :1;
  bool se :1;
} _TextService_ComposeText_result__isset;

class TextService_ComposeText_result {
 public:

  TextService_ComposeText_result(const TextService_ComposeText_result&);
  TextService_ComposeText_result& operator=(const TextService_ComposeText_result&);
  TextService_ComposeText_result() noexcept;

  virtual ~TextService_ComposeText_result() noexcept;
  TextServiceReturn success;
  ServiceException se;

  _TextService_ComposeText_result__isset __isset;

  void __set_success(const TextServiceReturn& val);

  void __set_se(const ServiceException& val);

  bool operator == (const TextService_ComposeText_result & rhs) const;
  bool operator != (const TextService_ComposeText_result &rhs) const {
    return !(*this == rhs);
  }

  bool operator < (const TextService_ComposeText_result & ) const;

  uint32_t read(::apache::thrift::protocol::TProtocol* iprot);
  uint32_t write(::apache::thrift::protocol::TProtocol* oprot) const;

};

typedef struct _TextService_ComposeText_presult__isset {
  _TextService_ComposeText_presult__isset() : success(false), se(false) {}
  bool success :1;
  bool se :1;
} _TextService_ComposeText_presult__isset;

class TextService_ComposeText_presult {
 public:


  virtual ~TextService_ComposeText_presult() noexcept;
  TextServiceReturn* success;
  ServiceException se;

  _TextService_ComposeText_presult__isset __isset;

  uint32_t read(::apache::thrift::protocol::TProtocol* iprot);

};

class TextServiceClient : virtual public TextServiceIf {
 public:
  TextServiceClient(std::shared_ptr< ::apache::thrift::protocol::TProtocol> prot) {
    setProtocol(prot);
  }
  TextServiceClient(std::shared_ptr< ::apache::thrift::protocol::TProtocol> iprot, std::shared_ptr< ::apache::thrift::protocol::TProtocol> oprot) {
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
  void ComposeText(TextServiceReturn& _return, const int64_t req_id, const std::string& text, const std::map<std::string, std::string> & carrier) override;
  void send_ComposeText(const int64_t req_id, const std::string& text, const std::map<std::string, std::string> & carrier);
  void recv_ComposeText(TextServiceReturn& _return);
 protected:
  std::shared_ptr< ::apache::thrift::protocol::TProtocol> piprot_;
  std::shared_ptr< ::apache::thrift::protocol::TProtocol> poprot_;
  ::apache::thrift::protocol::TProtocol* iprot_;
  ::apache::thrift::protocol::TProtocol* oprot_;
};

class TextServiceProcessor : public ::apache::thrift::TDispatchProcessor {
 protected:
  ::std::shared_ptr<TextServiceIf> iface_;
  virtual bool dispatchCall(::apache::thrift::protocol::TProtocol* iprot, ::apache::thrift::protocol::TProtocol* oprot, const std::string& fname, int32_t seqid, void* callContext) override;
 private:
  typedef  void (TextServiceProcessor::*ProcessFunction)(int32_t, ::apache::thrift::protocol::TProtocol*, ::apache::thrift::protocol::TProtocol*, void*);
  typedef std::map<std::string, ProcessFunction> ProcessMap;
  ProcessMap processMap_;
  void process_ComposeText(int32_t seqid, ::apache::thrift::protocol::TProtocol* iprot, ::apache::thrift::protocol::TProtocol* oprot, void* callContext);
 public:
  TextServiceProcessor(::std::shared_ptr<TextServiceIf> iface) :
    iface_(iface) {
    processMap_["ComposeText"] = &TextServiceProcessor::process_ComposeText;
  }

  virtual ~TextServiceProcessor() {}
};

class TextServiceProcessorFactory : public ::apache::thrift::TProcessorFactory {
 public:
  TextServiceProcessorFactory(const ::std::shared_ptr< TextServiceIfFactory >& handlerFactory) noexcept :
      handlerFactory_(handlerFactory) {}

  ::std::shared_ptr< ::apache::thrift::TProcessor > getProcessor(const ::apache::thrift::TConnectionInfo& connInfo) override;

 protected:
  ::std::shared_ptr< TextServiceIfFactory > handlerFactory_;
};

class TextServiceMultiface : virtual public TextServiceIf {
 public:
  TextServiceMultiface(std::vector<std::shared_ptr<TextServiceIf> >& ifaces) : ifaces_(ifaces) {
  }
  virtual ~TextServiceMultiface() {}
 protected:
  std::vector<std::shared_ptr<TextServiceIf> > ifaces_;
  TextServiceMultiface() {}
  void add(::std::shared_ptr<TextServiceIf> iface) {
    ifaces_.push_back(iface);
  }
 public:
  void ComposeText(TextServiceReturn& _return, const int64_t req_id, const std::string& text, const std::map<std::string, std::string> & carrier) override {
    size_t sz = ifaces_.size();
    size_t i = 0;
    for (; i < (sz - 1); ++i) {
      ifaces_[i]->ComposeText(_return, req_id, text, carrier);
    }
    ifaces_[i]->ComposeText(_return, req_id, text, carrier);
    return;
  }

};

// The 'concurrent' client is a thread safe client that correctly handles
// out of order responses.  It is slower than the regular client, so should
// only be used when you need to share a connection among multiple threads
class TextServiceConcurrentClient : virtual public TextServiceIf {
 public:
  TextServiceConcurrentClient(std::shared_ptr< ::apache::thrift::protocol::TProtocol> prot, std::shared_ptr< ::apache::thrift::async::TConcurrentClientSyncInfo> sync) : sync_(sync)
{
    setProtocol(prot);
  }
  TextServiceConcurrentClient(std::shared_ptr< ::apache::thrift::protocol::TProtocol> iprot, std::shared_ptr< ::apache::thrift::protocol::TProtocol> oprot, std::shared_ptr< ::apache::thrift::async::TConcurrentClientSyncInfo> sync) : sync_(sync)
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
  void ComposeText(TextServiceReturn& _return, const int64_t req_id, const std::string& text, const std::map<std::string, std::string> & carrier) override;
  int32_t send_ComposeText(const int64_t req_id, const std::string& text, const std::map<std::string, std::string> & carrier);
  void recv_ComposeText(TextServiceReturn& _return, const int32_t seqid);
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
