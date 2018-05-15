#include "cppetcd.h"
#include "etcd/etcdserver/etcdserverpb/rpc.pb.h"
#include "etcd/etcdserver/etcdserverpb/rpc.grpc.pb.h"

#include <iostream>

#define UNINPLEMENTED_STATUS (grpc::Status(grpc::StatusCode::UNIMPLEMENTED, "Not Implemented Yet"))
#define UNAVAILABLE_STATUS (grpc::Status(grpc::StatusCode::UNAVAILABLE, "Server is unavailable"))

namespace etcd {
  Client::Client(const std::vector<std::string>& hosts) :
    lease_id_(0), hosts_(hosts)
  {
    if (hosts_.empty()) {
      throw 2;
    }
  }
  Client::~Client() {}

    // Connect to etcd
  grpc::Status Client::Connect(){
    if (channel_ && state_ == CONNECTED) {
      // already connected
      return grpc::Status::OK;
    }
    std::string host = hosts_.at(0);
    //std::cerr << __LINE__ << std::endl;
    channel_ = grpc::CreateChannel(host, grpc::InsecureChannelCredentials());
    if (not channel_) {
      //failed to connect
      return UNAVAILABLE_STATUS;
    }
    std::unique_ptr<etcdserverpb::Lease::Stub> stub = etcdserverpb::Lease::NewStub(channel_);

    grpc::ClientContext context;
    etcdserverpb::LeaseGrantRequest req;
    etcdserverpb::LeaseGrantResponse res;
    req.set_ttl(5); // hard coded
    req.set_id(0);

    grpc::Status status = stub->LeaseGrant(&context, req, &res);
    if (not status.ok()) {
      return status;
    }

    lease_id_ = res.id();
    state_ = CONNECTED;
    std::cout << "connected: client id=" << res.id() << " ttl=" << res.ttl() << "sec" << std::endl;
    std::cout <<  "header: cluster_id=" << res.header().cluster_id() << " rev=" << res.header().revision() << std::endl;

    return status;
  }
  grpc::Status Client::Disconnect(){
    return grpc::Status::OK;
  }

    // put/get
  grpc::Status Client::Get(const std::string& key, std::string& value, long long * rev){
    if (not channel_) {
      //failed to connect
      return UNAVAILABLE_STATUS;
    }
    std::shared_ptr<etcdserverpb::KV::Stub> stub = etcdserverpb::KV::NewStub(channel_);
    grpc::ClientContext context;
    etcdserverpb::RangeRequest req;
    etcdserverpb::RangeResponse res;
    req.set_key(key);
    req.set_limit(1);
    //req.set_range_end(key);
    //req.set_lease(lease_id_); // temporary...
    grpc::Status status = stub->Range(&context, req, &res);
    if (not status.ok()) {
      std::cerr << status.error_message() << std::endl;
    }
    std::cerr << res.count() << std::endl;

    for (auto kv : res.kvs()) {
      *rev = kv.version();
      value = kv.value();
      return status;
    }
    // no key found.
    return status;
  }
  grpc::Status Client::Put(const std::string& key, const std::string& value, long long rev){
    if (not channel_) {
      //failed to connect
      return UNAVAILABLE_STATUS;
    }
    std::shared_ptr<etcdserverpb::KV::Stub> stub = etcdserverpb::KV::NewStub(channel_);
    grpc::ClientContext context;
    etcdserverpb::PutRequest req;
    etcdserverpb::PutResponse res;
    req.set_key(key);
    req.set_value(value);
    //req.set_lease(lease_id_); // temporary...
    grpc::Status status = stub->Put(&context, req, &res);
    return status;
  }
  grpc::Status Client::Delete(const std::string& key, long long rev){
    return UNINPLEMENTED_STATUS;
  }
  grpc::Status Client::List(const std::string& prefix, std::vector<std::string>& out){
    return UNINPLEMENTED_STATUS;
    
  }
  // As this is a synchronous API and returns when keepalive failed
  grpc::Status Client::KeepAlive() {
    if (not channel_) {
      //failed to connect
      return UNAVAILABLE_STATUS;
    }
    std::shared_ptr<etcdserverpb::Lease::Stub> stub = etcdserverpb::Lease::NewStub(channel_);
    grpc::ClientContext context;
    std::shared_ptr<grpc::ClientReaderWriter<etcdserverpb::LeaseKeepAliveRequest, etcdserverpb::LeaseKeepAliveResponse> > stream(stub->LeaseKeepAlive(&context));
    etcdserverpb::LeaseKeepAliveRequest req;
    etcdserverpb::LeaseKeepAliveResponse res;
    req.set_id(lease_id_);

    while (true) {
      if (not stream->Write(req)) {
        //write fail
        std::cerr << "write fail" << std::endl;
        return stream->Finish();
      }
      if (not stream->Read(&res)) {
        //read fail
        std::cerr << "read fail" << std::endl;
        return stream->Finish();
      }

      // std::cerr << "ok" <<       res.id()         << std::endl;
      return grpc::Status::OK;
    }
  }

    // Wait is also needed, but can be replaced with periodic polling for my use case.
    // Someday someone wraps it.
  Client::Client() {}
  
}
/*
	
	--proto_path=. \
	-o ru \
	--cpp_out=src 
*/
