#include "gtest/gtest.h"
#include "adder.hpp"

class AdderTest : public ::testing::Test,
                  public ::testing::WithParamInterface<int> {
 public:
  Adder adder;
};

TEST_F(AdderTest, addTest) {
  EXPECT_EQ(adder(1, 2), 3);
}

TEST_P(AdderTest, addOne) {
  auto param = GetParam();
  EXPECT_EQ(adder(param, 1), param + 1);
}

TEST_P(AdderTest, addTwo) {
  auto param = GetParam();
  EXPECT_EQ(adder(param, 2), param + 2);
}

TEST_F(AdderTest, DISABLED_deathLoop) {
  while (true);
}

INSTANTIATE_TEST_CASE_P(AdderTestWithParam, AdderTest, ::testing::Range(0, 9));
