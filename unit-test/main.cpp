#include "gmock/gmock.h"
#include "gtest/gtest.h"

class GlobalEnv : public ::testing::Environment {
 public:
  virtual void SetUp() {
    //  Stub
  }
  virtual void TearDown() {
    //  Stub
  }
};

int main(int argc, char *argv[]) {
  AddGlobalTestEnvironment(new GlobalEnv);
  ::testing::InitGoogleMock(&argc, argv);
  return RUN_ALL_TESTS();
}
