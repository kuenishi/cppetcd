#include "cppetcd.h"
#include "etcd/etcdserver/etcdserverpb/rpc.pb.h"
#include "etcd/etcdserver/etcdserverpb/rpc.grpc.pb.h"
#include "etcd/etcdserver/api/v3lock/v3lockpb/v3lock.pb.h"
#include "etcd/etcdserver/api/v3lock/v3lockpb/v3lock.grpc.pb.h"

#include <ctime>
#include <thread>

#include <iostream>
#include <glog/logging.h>

#define UNINPLEMENTED_STATUS (grpc::Status(grpc::StatusCode::UNIMPLEMENTED, "Not Implemented Yet"))
#define UNAVAILABLE_STATUS (grpc::Status(grpc::StatusCode::UNAVAILABLE, "Server is unavailable"))

namespace etcd {
  Client::Client(const std::vector<std::string>& hosts) :
    lease_id_(0), hosts_(hosts), state_(DISCONNECTED)
  {
    if (hosts_.empty()) {
      throw 2;
    }
  }
  Client::~Client() {}

    // Connect to etcd
  grpc::Status Client::Connect(){
    if (Connected()) {
      // already connected
      return grpc::Status::OK;
    }

    grpc::ClientContext context;
    etcdserverpb::LeaseGrantRequest req;
    etcdserverpb::LeaseGrantResponse res;
    req.set_ttl(5); // hard coded
    req.set_id(0);
    grpc::Status status;
    for (auto host : hosts_) {
      channel_ = grpc::CreateChannel(host, grpc::InsecureChannelCredentials());
      if (not channel_) {
        //failed to connect
        continue;
      }
      std::unique_ptr<etcdserverpb::Lease::Stub> stub = etcdserverpb::Lease::NewStub(channel_);

      unsigned long lease_start = now();
      status = stub->LeaseGrant(&context, req, &res);
      if (not status.ok()) {
        continue;
      }

      lease_id_ = res.id();
      state_ = CONNECTED;
      lease_limit_ = lease_start + 5000; // hard coded
      std::cout << "connected: client id=" << res.id() << " ttl=" << res.ttl() << "sec" << std::endl;
      std::cout <<  "header: cluster_id=" << res.header().cluster_id() << " rev=" << res.header().revision() << std::endl;
      return status;
    }
    // no host available
    return UNAVAILABLE_STATUS;
  }
  grpc::Status Client::Disconnect(){
    state_ = DISCONNECTED;
    channel_ = std::shared_ptr<grpc::ChannelInterface>(nullptr);
    return grpc::Status::OK;
  }
  bool Client::Connected() const {
    long timeout_exceed = lease_limit_ - now();
    return (not channel_) && state_ == CONNECTED && (timeout_exceed < 0);
  }

    // put/get
  grpc::Status Client::Get(const std::string& key, std::string& value, long long * rev){
    if (not Connected()) {
      //failed to connect
      return UNAVAILABLE_STATUS;
    }
    std::shared_ptr<etcdserverpb::KV::Stub> stub = etcdserverpb::KV::NewStub(channel_);
    grpc::ClientContext context;
    etcdserverpb::RangeRequest req;
    etcdserverpb::RangeResponse res;
    req.set_key(key);
    req.set_limit(1);
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
    return grpc::Status(grpc::StatusCode::NOT_FOUND, "No key found");
  }
  grpc::Status Client::Put(const std::string& key, const std::string& value, long long rev,
                           bool ephemeral){
    if (not Connected()) {
      //failed to connect
      return UNAVAILABLE_STATUS;
    }
    std::shared_ptr<etcdserverpb::KV::Stub> stub = etcdserverpb::KV::NewStub(channel_);
    grpc::ClientContext context;
    etcdserverpb::PutRequest req;
    etcdserverpb::PutResponse res;
    req.set_key(key);
    req.set_value(value);
    if (ephemeral) {
      req.set_lease(lease_id_); // temporary...
    }
    grpc::Status status = stub->Put(&context, req, &res);
    return status;
  }
  grpc::Status Client::Delete(const std::string& key, long long rev){
    return UNINPLEMENTED_STATUS;
  }
  grpc::Status Client::List(const std::string& prefix,
                            std::vector<std::pair<std::string, std::string>>& out){
    if (not Connected()) {
      //failed to connect
      return UNAVAILABLE_STATUS;
    }
    
    std::shared_ptr<etcdserverpb::KV::Stub> stub = etcdserverpb::KV::NewStub(channel_);
    grpc::ClientContext context;
    etcdserverpb::RangeRequest req;
    etcdserverpb::RangeResponse res;
    req.set_key(prefix);
    std::string end = prefix;
    end.push_back(0xFF);
    req.set_range_end(end);

    grpc::Status status = stub->Range(&context, req, &res);
    if (not status.ok()) {
      std::cerr << status.error_message() << std::endl;
    }
    std::cerr << res.count() << std::endl;

    out.clear();
    for (auto kv : res.kvs()) {
      std::cerr << kv.key() << " " << kv.value() << std::endl;
      out.push_back(std::pair<std::string, std::string>(kv.key(), kv.value()));
    }
    if (res.more()) {
      //....
    }
    // no key found.
    return status;
  }

  grpc::Status Client::Lock(const std::string& name, std::string& key){
    if (not Connected()) {
      //failed to connect
      return UNAVAILABLE_STATUS;
    }
    
    std::unique_ptr<v3lockpb::Lock::Stub> stub = v3lockpb::Lock::NewStub(channel_);
    grpc::ClientContext context;
    v3lockpb::LockRequest req;
    v3lockpb::LockResponse res;
    req.set_name(name);
    req.set_lease(lease_id_);

    /// Blocks until somebody holds lock, possibly?
    grpc::Status status = stub->Lock(&context, req, &res);
    if (status.ok()) {
      key = res.key();
    }
    return status;
  }
  grpc::Status Client::Unlock(const std::string& key){
    if (not Connected()) {
      //failed to connect
      return UNAVAILABLE_STATUS;
    }

    std::unique_ptr<v3lockpb::Lock::Stub> stub = v3lockpb::Lock::NewStub(channel_);
    grpc::ClientContext context;
    v3lockpb::UnlockRequest req;
    v3lockpb::UnlockResponse res;
    req.set_key(key);

    grpc::Status status = stub->Unlock(&context, req, &res);
    return status;
}

  
  // As this is a synchronous API and returns when keepalive failed
  grpc::Status Client::KeepAlive(bool forever) {
    if (not Connected()) {
      //failed to connect
      return UNAVAILABLE_STATUS;
    }
    std::shared_ptr<etcdserverpb::Lease::Stub> stub = etcdserverpb::Lease::NewStub(channel_);
    grpc::ClientContext context;
    std::shared_ptr<grpc::ClientReaderWriter<etcdserverpb::LeaseKeepAliveRequest, etcdserverpb::LeaseKeepAliveResponse> > stream(stub->LeaseKeepAlive(&context));
    etcdserverpb::LeaseKeepAliveRequest req;
    etcdserverpb::LeaseKeepAliveResponse res;
    req.set_id(lease_id_);

    do {
      unsigned long lease_start = now();
      if (not stream->Write(req)) {
        //write fail
        std::cerr << "write fail" << std::endl;
        state_ = DISCONNECTED;
        return stream->Finish();
      }
      if (not stream->Read(&res)) {
        //read fail
        std::cerr << "read fail" << std::endl;
        state_ = DISCONNECTED;
        return stream->Finish();
      }
      
      lease_limit_ = lease_start + res.ttl() * 1000; // hard coded
      // std::cerr << "ok" <<       res.id()         << std::endl;
      std::this_thread::sleep_for(std::chrono::milliseconds(res.ttl() * 1000 / 2));

    } while (forever && state_ == CONNECTED);
    return Disconnect();
  }

  // Wait is also needed, but can be replaced with periodic polling for my use case.
  // Someday someone wraps it.
  Client::Client() {}

  // Utilities
  unsigned long now() {
    struct timespec t;
    int r = ::clock_gettime(CLOCK_BOOTTIME, &t);
    if (r != 0) {
      char buf[1024];
      ::strerror_r(errno, buf, 1024);
      LOG(FATAL) << buf; // heh, it's really fatal and rare if this system call does not work.
      // See errno() and handle them well
      return 0;
    }
    unsigned long now = 0;
    now += (t.tv_sec * 1000);
    now += t.tv_nsec / 1000000;
    return now;
  }
  
} // namespace etcd
