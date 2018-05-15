#pragma once

#include <string>
#include <vector>
#include <grpc++/grpc++.h>

namespace etcd {

  //class etcdserverpb::KV::Stub;
  class Client final {
  public:
    Client(const std::vector<std::string>&);
    ~Client();

    // Connect to etcd
    grpc::Status Connect();
    grpc::Status Disconnect();

    // only single key with exact match, returning version
    grpc::Status Get(const std::string& key, std::string& value, long long * rev);
    // overwrite any key
    grpc::Status Put(const std::string& key, const std::string& value, long long rev);
    grpc::Status Delete(const std::string& key, long long rev);
    grpc::Status List(const std::string& prefix, std::vector<std::string>&);

    // Wait is also needed, but can be replaced with periodic polling for my use case.
    // Someday someone wraps it.
    grpc::Status KeepAlive();
  private:
    Client();

    long long lease_id_;
    std::vector<std::string> hosts_;
    std::shared_ptr<grpc::ChannelInterface> channel_;
    enum state { NONE, CONNECTED } state_;;
  };

};
