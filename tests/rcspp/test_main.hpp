#pragma once

#include "test_rcspp.hpp"

using namespace rcspp;

template <template <typename> class... AlgorithmTypes>
    std::pair<int,int> all_tests_rcspp() {
  int passed = 0;
  int total = 0;
  (void)std::initializer_list<int>{([&]() {
    if (test_rcspp<AlgorithmTypes>()) {
      ++passed;
    }
    ++total;
    return 0;
        }())...};
  return std::make_pair(passed, total);
}

template <template <typename> class... AlgorithmTypes>
    std::pair<int,int> all_tests_rcspp_non_integer_dual_row_coef() {
  int passed = 0;
  int total = 0;
  (void)std::initializer_list<int>{([&]() {
    if (test_rcspp_non_integer_dual_row_coef<AlgorithmTypes>()) {
      ++passed;
    }
    ++total;
    return 0;
        }())...};
  return std::make_pair(passed, total);
}