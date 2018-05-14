#include <iostream>

#include <gtest/gtest.h>
#include <glog/logging.h>

#include "cppetcd.h"

using namespace std;

namespace {

  static const size_t CLUSTER_SIZE = 16;
  static const size_t PPN = 4;
  class NodeListTest : public ::testing::Test {
  protected:
    NodeListTest() {}
    virtual ~NodeListTest() {}
    virtual void SetUp() {}
    virtual void TearDown() {}

  };

  TEST_F(NodeListTest, Smoke) {
    std::string root("tcp://192.168.42.22:10000");
  }
}

int main(int argc, char** argv){
  google::InitGoogleLogging(argv[0]);
  ::testing::InitGoogleTest(&argc, argv);
  FLAGS_logtostderr = 0;
  return RUN_ALL_TESTS();
}
