﻿
#pragma once

#include <list>
#include <type_traits>

#include "base/ranges.hpp"
#include "numerics/hermite3.hpp"

namespace principia {
namespace numerics {
namespace internal_fit_hermite_spline {

using base::Range;
using geometry::Normed;

template<typename Argument, typename Value, typename Samples>
std::list<typename Samples::const_iterator> FitHermiteSpline(
    Samples const& samples,
    std::function<Argument const&(typename Samples::value_type const&)> const&
        get_argument,
    std::function<Value const&(typename Samples::value_type const&)> const&
        get_value,
    std::function<Derivative<Value, Argument> const&(
        typename Samples::value_type const&)> const& get_derivative,
    typename Normed<Difference<Value>>::NormType const& tolerance) {
  using Iterator = typename Samples::const_iterator;

  if (samples.size() < 3) {
    // With 0 or 1 points there is nothing to interpolate, with 2 we cannot
    // estimate the error.
    return {};
  }

  auto interpolation_error = [get_argument, get_derivative, get_value](
                                 Iterator begin, Iterator last) {
    return Hermite3<Argument, Value>(
               {get_argument(*begin), get_argument(*last)},
               {get_value(*begin), get_value(*last)},
               {get_derivative(*begin), get_derivative(*last)})
        .LInfinityError(Range(begin, last + 1), get_argument, get_value);
  };

  auto const last = samples.end() - 1;
  if (interpolation_error(samples.begin(), last) < tolerance) {
    // A single polynomial fits the entire range, so we have no way of knowing
    // whether it is the largest polynomial that will fit the range.
    return {};
  } else {
    // Look for a cubic that fits the beginning within |tolerance| and
    // such the cubic fitting one more sample would not fit the samples within
    // |tolerance|.
    // Note that there may be more than one cubic satisfying this property;
    // ideally we would like to find the longest one, but this would be costly,
    // and we do not expect significant gains from this in practice.

    // Invariant: The Hermite interpolant on [samples.begin(), lower] is below
    // the tolerance, the Hermite interpolant on [samples.begin(), upper] is
    // above.
    auto lower = samples.begin() + 1;
    auto upper = last;
    for (;;) {
      auto const middle = lower + (upper - lower) / 2;
      // Note that lower ≤ middle ≤ upper.
      // If middle - lower > 0, upper - lower > 0,
      // therefore (upper - lower) / 2  < upper - lower, thus
      // middle < upper.  It follows that upper - lower strictly decreases in
      // this iteration, since we assign middle to either lower or upper.
      // We exit when middle == lower, so the algorithm terminates.
      if (middle == lower) {
        break;
      }
      if (interpolation_error(samples.begin(), middle) < tolerance) {
        lower = middle;
      } else {
        upper = middle;
      }
    }
    std::list<Iterator> tail = FitHermiteSpline(Range(lower, samples.end()),
                                                get_argument,
                                                get_value,
                                                get_derivative,
                                                tolerance);
    tail.push_front(lower);
    return tail;
  }
}

}  // namespace internal_fit_hermite_spline
}  // namespace numerics
}  // namespace principia
