#include <iostream>
#include <thread>

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
    ASSERT_FALSE(client.Connected());
    status = client.Connect();
    ASSERT_TRUE(status.ok());
    ASSERT_TRUE(client.Connected());
    status = client.KeepAlive(false);
    ASSERT_TRUE(status.ok());
    ASSERT_FALSE(client.Connected());
    status = client.Connect();
    ASSERT_TRUE(status.ok());
    ASSERT_TRUE(client.Connected());
    status = client.Put(TEST_PREFIX+"hoge", "val", 0, true);
    ASSERT_TRUE(status.ok());
    std::string value;
    long long rev = 0;
    ASSERT_TRUE(client.Get(TEST_PREFIX+"hoge", value, &rev).ok());
    ASSERT_EQ(std::string("val"), value);
    ASSERT_GT(rev, 0);

    value = "";
    status = client.Get(TEST_PREFIX+"no=such=key", value, &rev);
    // cerr << "Error reason: " << status.error_message() << " " << status.error_details() << endl;
    ASSERT_EQ(grpc::StatusCode::NOT_FOUND, status.error_code());
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

  TEST_F(ClientTest, Lock) {
    std::vector<std::string> hosts;
    hosts.push_back("127.0.0.1:2379");

    etcd::Client client(hosts);
    grpc::Status status;
    status = client.Connect();
    ASSERT_TRUE(status.ok());

    std::string l1 = "Lock1";
    std::string l2 = "Lock2";
    for (size_t s = 0; s < 10; s++) {
      status = client.Lock(l1, 1000u);
      ASSERT_TRUE(status.ok());
      status = client.Lock(l2, 1000u);
      ASSERT_TRUE(status.ok());
      ASSERT_TRUE(client.HasLock(l1));
      ASSERT_TRUE(client.HasLock(l2));
      status = client.Unlock(l1);
      ASSERT_TRUE(status.ok());
      status = client.Unlock(l2);
      ASSERT_TRUE(status.ok());
    }

    status = client.Unlock(l1);
    // ASSERT_EQ(grpc::StatusCode::FAILED_PRECONDITION, status.error_code());
    ASSERT_FALSE(status.ok());
    ASSERT_TRUE(client.Disconnect().ok());
  }

  class TestWatcher : public etcd::EventWatcher {
  public:
    size_t count;
    TestWatcher() : EventWatcher(), count(0) {}
    virtual ~TestWatcher() {}
    virtual void HandleEvents(const std::vector<etcd::KeyValueEvent>& events) {
      count += events.size();
      for (etcd::KeyValueEvent e : events) {
        cerr << e.key << " => " << e.value << endl;
      }
    }
    virtual bool StopHandling() const {
      return count == 2;
    }
  };

  TEST_F(ClientTest, Watch) {
    std::vector<std::string> hosts;
    hosts.push_back("127.0.0.1:2379");

    etcd::Client c(hosts);
    grpc::Status status;
    status = c.Connect();
    ASSERT_TRUE(status.ok());

    TestWatcher w;
    std::thread th = std::thread([&c]{
                                   ::sleep(1);
                                   ASSERT_TRUE(c.Put("foobar/baz", "val", 0).ok());
                                 });
    ASSERT_TRUE(c.Watch("foobar", w).ok());
    th.join();
  }
}

int main(int argc, char** argv){
  google::InitGoogleLogging(argv[0]);
  ::testing::InitGoogleTest(&argc, argv);
  FLAGS_logtostderr = 1;
  return RUN_ALL_TESTS();
}
