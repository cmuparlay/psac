// Utilities for PSAC unit tests

#ifndef TESTS_COMMON_H_
#define TESTS_COMMON_H_

#include <string>
#include <sstream>

#include <psac/psac.hpp>

#define INSTANTIATE_MT_TESTS(TestName) \
  using TestName = VariedWorkersTest; \
  INSTANTIATE_TEST_SUITE_P(MT, TestName, ::testing::Values(1, 2, 4, 8, 16), PrintNumWorkers());

// Used as the parameter name for value-parametrized tests
struct PrintNumWorkers {
  std::string operator()(const ::testing::TestParamInfo<size_t>& info) const {
    auto n_workers = info.param;
    std::stringstream ss;
    ss << n_workers;
    return ss.str();
  }
};

// Parametrized tests used to vary the number of worker threads
class VariedWorkersTest : public testing::TestWithParam<size_t> {
public:
  VariedWorkersTest() {
    auto n_workers = GetParam();
    parlay::set_num_workers(n_workers);
  }
  virtual void TestBody() override { }
};

#endif  // TESTS_COMMON_H_

