#include "cppetcd.h"
#include "etcd/etcdserver/etcdserverpb/rpc.pb.h"
#include "etcd/etcdserver/etcdserverpb/rpc.grpc.pb.h"
#include "etcd/etcdserver/api/v3lock/v3lockpb/v3lock.pb.h"
#include "etcd/etcdserver/api/v3lock/v3lockpb/v3lock.grpc.pb.h"
#include "etcd/mvcc/mvccpb/kv.pb.h"
#include "etcd/mvcc/mvccpb/kv.grpc.pb.h"

#include <ctime>
#include <thread>

#include <glog/logging.h>

#define UNINPLEMENTED_STATUS (grpc::Status(grpc::StatusCode::UNIMPLEMENTED, "Not Implemented Yet"))
#define UNAVAILABLE_STATUS (grpc::Status(grpc::StatusCode::UNAVAILABLE, "Server is unavailable"))

namespace etcd {
  Client::Client(const std::vector<std::string>& hosts) :
    lease_id_(0), hosts_(hosts), state_(DISCONNECTED)
  {
    if (hosts_.empty()) {
      LOG(FATAL) << "Empty host list given";
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
      LOG(INFO) << "cppetcd connected: client id=" << res.id() << " ttl=" << res.ttl() << "sec";
      DLOG(INFO) <<  "header: cluster_id=" << res.header().cluster_id() << " rev=" << res.header().revision();
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
    long timeout_exceed = now() - lease_limit_;
    //std::cerr << (!!channel_) << "|" << state_ << "?" << CONNECTED << "|" << timeout_exceed
    //          << "|" << lease_limit_ << "|" << now()  << "|" << timeout_exceed << std::endl;
    return (!!channel_) && state_ == CONNECTED && (timeout_exceed < 0);
  }

  long long Client::LeaseId() const {
    return lease_id_;
  };

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
      LOG(ERROR) << "Failed to get " << key << ": " << status.error_message();
    }

    for (auto kv : res.kvs()) {
      if (rev != nullptr) {
        *rev = kv.version();
      }
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
    if (not Connected()) {
      //failed to connect
      return UNAVAILABLE_STATUS;
    }
    std::shared_ptr<etcdserverpb::KV::Stub> stub = etcdserverpb::KV::NewStub(channel_);
    grpc::ClientContext context;
    etcdserverpb::DeleteRangeRequest req;
    etcdserverpb::DeleteRangeResponse res;
    req.set_key(key);

    grpc::Status status = stub->DeleteRange(&context, req, &res);
    if (not status.ok()) {
      LOG(ERROR) << "Failed to put: " << status.error_message();
    } else if (res.deleted() != 1) {
      LOG(ERROR) << "Number deleted keys != 0 actully:" << res.deleted();
    }
    return status;
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
      LOG(ERROR) << "Failed range request:" << status.error_message();
      return status;
    }

    out.clear();
    for (auto kv : res.kvs()) {
      DLOG(INFO) << kv.key() << " " << kv.value();
      out.push_back(std::pair<std::string, std::string>(kv.key(), kv.value()));
    }
    if (res.more()) {
      //....
    }
    // no key found.
    return status;
  }

  grpc::Status Client::Watch(const std::string& prefix, EventWatcher& watcher) {
    if (not Connected()) {
      //failed to connect
      return UNAVAILABLE_STATUS;
    }
    std::unique_ptr<etcdserverpb::Watch::Stub> stub = etcdserverpb::Watch::NewStub(channel_);

    grpc::ClientContext context;
    std::shared_ptr<grpc::ClientReaderWriter<etcdserverpb::WatchRequest, etcdserverpb::WatchResponse>> stream(stub->Watch(&context));
    etcdserverpb::WatchRequest wreq;
    std::string key_end = prefix;
    key_end.push_back(0xFF);
    grpc::Status status;
    wreq.mutable_create_request()->set_key(prefix);
    wreq.mutable_create_request()->set_range_end(key_end);

    if (not stream->Write(wreq)) {
      stream->Finish();
      LOG(FATAL) << "Can't write to stream";
    }
    do {
      etcdserverpb::WatchResponse res;
      if (not stream->Read(&res)) {
        stream->Finish();
        LOG(FATAL) << "Can't read from stream";
      }

      // Handle data read!
      std::vector<KeyValueEvent> events;
      for (mvccpb::Event event : res.events()) {
        enum KeyValueEvent::EventType et;
        if (event.type() == mvccpb::Event::PUT) {
          et = KeyValueEvent::PUT;
        } else if (event.type() == mvccpb::Event::DELETE) {
          et = KeyValueEvent::DELETE;
        }
        struct KeyValueEvent e(event.kv().key(),
                               event.kv().value(),
                               event.kv().version(),
                               event.kv().lease(),
                               et);
        events.push_back(e);
      }
      watcher.HandleEvents(events);
      if (watcher.StopHandling()) {
        LOG(INFO) << "Watch stopped by application";
        return grpc::Status::OK;
      }

      // wreq.mutable_create_request()->set_watch_id(res.watch_id());
    } while(true);
    return grpc::Status::OK;
  }
  grpc::Status Client::Lock(const std::string& name, unsigned int timeout_ms){
    if (not Connected()) {
      //failed to connect
      return UNAVAILABLE_STATUS;
    }

    std::unique_ptr<v3lockpb::Lock::Stub> stub = v3lockpb::Lock::NewStub(channel_);
    grpc::ClientContext context;
    // https://grpc.io/blog/deadlines
    std::chrono::system_clock::time_point deadline = std::chrono::system_clock::now() + std::chrono::milliseconds(timeout_ms);
    context.set_deadline(deadline);
    v3lockpb::LockRequest req;
    v3lockpb::LockResponse res;
    req.set_name(name);
    req.set_lease(lease_id_);

    /// Blocks until somebody holds lock, possibly?
    grpc::Status status = stub->Lock(&context, req, &res);
    if (status.ok()) {
      lock_keys_[name] = res.key();
      //} else if (status.error_code() == StatusCode::DEADLINE_EXCEEDED) { // OK but lock not acquired!
      // status = Status(StatusCode::OK, "Timeout: lock might be acquired by other");
    }
    return status;
  }
  grpc::Status Client::Unlock(const std::string& name){
    if (not Connected()) {
      //failed to connect
      return UNAVAILABLE_STATUS;
    }
    auto pair = lock_keys_.find(name);
    if (pair == lock_keys_.end()) {
      return grpc::Status(grpc::StatusCode::FAILED_PRECONDITION, "No lock acquired");
    }
    lock_keys_.erase(name);

    std::unique_ptr<v3lockpb::Lock::Stub> stub = v3lockpb::Lock::NewStub(channel_);
    grpc::ClientContext context;
    v3lockpb::UnlockRequest req;
    req.set_key(pair->second);
    v3lockpb::UnlockResponse res;

    grpc::Status status = stub->Unlock(&context, req, &res);
    return status;
  }
  bool Client::HasLock(const std::string& name) {
    return lock_keys_.find(name) != lock_keys_.end();
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
        LOG(ERROR) << "write fail";
        state_ = DISCONNECTED;
        return stream->Finish();
      }
      if (not stream->Read(&res)) {
        //read fail
        LOG(ERROR) << "read fail";
        state_ = DISCONNECTED;
        return stream->Finish();
      }

      lease_limit_ = lease_start + res.ttl() * 1000; // hard coded
      // std::cerr << "ok" <<       res.id()         << std::endl;
      std::this_thread::sleep_for(std::chrono::milliseconds(res.ttl() * 1000 / 2));

    } while (forever && state_ == CONNECTED);
    // TODO: do we need to disconnect here?
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
