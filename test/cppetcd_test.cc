#include <iostream>

#include <gtest/gtest.h>
#include <glog/logging.h>

#include "cppetcd.h"

using namespace std;

namespace {

  static const std::string TEST_PREFIX="cppetcd-test-";

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
    ASSERT_TRUE(client.Put(TEST_PREFIX+"hoge", "val", 0, true).ok());
    std::string value;
    long long rev = 0;
    ASSERT_TRUE(client.Get(TEST_PREFIX+"hoge", value, &rev).ok());
    ASSERT_EQ(std::string("val"), value);
    ASSERT_GT(rev, 0);

    value = "";
    ASSERT_TRUE(client.Get(TEST_PREFIX+"no=such=key", value, &rev).ok());
    ASSERT_EQ(std::string(""), value);
    ASSERT_TRUE(client.Disconnect().ok());
  }

  TEST_F(ClientTest, List) {
    std::vector<std::string> hosts;
    hosts.push_back("127.0.0.1:2379");

    etcd::Client client(hosts);
    grpc::Status status;
    status = client.Connect();
    ASSERT_TRUE(status.ok());
    ASSERT_TRUE(client.Put(TEST_PREFIX+"hog", "val", 0, true).ok());
    ASSERT_TRUE(client.Put(TEST_PREFIX+"hoge", "val", 0, true).ok());
    ASSERT_TRUE(client.Put(TEST_PREFIX+"hoge2", "val2", 0, true).ok());
    ASSERT_TRUE(client.Put(TEST_PREFIX+"hogf", "val2", 0, true).ok());

    std::vector<std::pair<std::string, std::string>> out;
    ASSERT_TRUE(client.List(TEST_PREFIX+"hoge", out).ok());
    ASSERT_EQ(2, out.size());
    ASSERT_EQ(std::string("val"), out.at(0).second);
    ASSERT_EQ(std::string("val2"), out.at(1).second);
    ASSERT_TRUE(client.Disconnect().ok());
  }
}

int main(int argc, char** argv){
  google::InitGoogleLogging(argv[0]);
  ::testing::InitGoogleTest(&argc, argv);
  FLAGS_logtostderr = 1;
  return RUN_ALL_TESTS();
}
