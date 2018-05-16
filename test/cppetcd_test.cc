#include <iostream>

#include <gtest/gtest.h>
#include <glog/logging.h>

#include "cppetcd.h"

using namespace std;

namespace {

  static const size_t CLUSTER_SIZE = 16;
  static const size_t PPN = 4;
  class ClientTest : public ::testing::Test {
  protected:
    ClientTest() {}
    virtual ~ClientTest() {}
    virtual void SetUp() {}
    virtual void TearDown() {}

  };

  TEST_F(ClientTest, Smoke) {
    std::vector<std::string> hosts;
    hosts.push_back("127.0.0.1:2379");

    ASSERT_GE(1, hosts.size());
    etcd::Client client(hosts);
    grpc::Status status;
    status = client.Connect();
    ASSERT_TRUE(status.ok());
    ASSERT_TRUE(client.KeepAlive(false).ok());
    ASSERT_TRUE(client.Put("hoge", "val", 0, true).ok());
    std::string value;
    long long rev = 0;
    ASSERT_TRUE(client.Get("hoge", value, &rev).ok());
    ASSERT_EQ(std::string("val"), value);
    ASSERT_GT(rev, 0);

    value = "";
    ASSERT_TRUE(client.Get("no=such=key", value, &rev).ok());
    ASSERT_EQ(std::string(""), value);
    ASSERT_TRUE(client.Disconnect().ok());
  }  
}

int main(int argc, char** argv){
  google::InitGoogleLogging(argv[0]);
  ::testing::InitGoogleTest(&argc, argv);
  FLAGS_logtostderr = 1;
  return RUN_ALL_TESTS();
}
