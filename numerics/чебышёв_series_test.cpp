﻿
#include "numerics/чебышёв_series.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

#include "astronomy/frames.hpp"
#include "geometry/grassmann.hpp"
#include "geometry/named_quantities.hpp"
#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "quantities/named_quantities.hpp"
#include "quantities/si.hpp"
#include "testing_utilities/almost_equals.hpp"
#include "testing_utilities/numerics.hpp"

namespace principia {
namespace numerics {
namespace internal_чебышёв_series {

using astronomy::ICRFJ2000Ecliptic;
using geometry::Instant;
using geometry::Vector;
using quantities::Length;
using quantities::Speed;
using quantities::si::Metre;
using quantities::si::Second;
using testing_utilities::AbsoluteError;
using testing_utilities::AlmostEquals;
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Gt;
using ::testing::Lt;

class ЧебышёвSeriesTest : public ::testing::Test {
 protected:
  ЧебышёвSeriesTest()
      : t_min_(t0_ - 1 * Second),
        t_max_(t0_ + 3 * Second) {}

  void NewhallApproximationErrors(
      std::function<Length(Instant const)> length_function,
      std::function<Speed(Instant const)> speed_function,
      std::vector<Length>& length_absolute_errors,
      std::vector<Speed>& speed_absolute_errors) {
    std::vector<Length> lengths;
    std::vector<Speed> speeds;
    for (Instant t = t_min_; t <= t_max_; t += 0.5 * Second) {
      lengths.push_back(length_function(t));
      speeds.push_back(speed_function(t));
    }

    length_absolute_errors.clear();
    speed_absolute_errors.clear();
    for (int degree = 3; degree <= 17; ++degree) {
      ЧебышёвSeries<Length> const approximation =
          ЧебышёвSeries<Length>::NewhallApproximation(
              degree, lengths, speeds, t_min_, t_max_);

      // Compute the absolute error of both functions throughout the interval.
      Length length_absolute_error;
      Speed speed_absolute_error;
      for (Instant t = t_min_; t <= t_max_; t += 0.05 * Second) {
        Length const expected_length = length_function(t);
        Length const actual_length = approximation.Evaluate(t);
        Speed const expected_speed = speed_function(t);
        Speed const actual_speed = approximation.EvaluateDerivative(t);
        length_absolute_error =
            std::max(length_absolute_error,
                     AbsoluteError(expected_length, actual_length));
        speed_absolute_error =
            std::max(speed_absolute_error,
                     AbsoluteError(expected_speed, actual_speed));
      }
      length_absolute_errors.push_back(length_absolute_error);
      speed_absolute_errors.push_back(speed_absolute_error);

      // Check the conditions at the bounds.
      EXPECT_THAT(approximation.Evaluate(t_min_),
                  AlmostEquals(length_function(t_min_), 0, 248));
      EXPECT_THAT(approximation.Evaluate(t_max_),
                  AlmostEquals(length_function(t_max_), 0, 3));
      EXPECT_THAT(approximation.EvaluateDerivative(t_min_),
                  AlmostEquals(speed_function(t_min_), 1, 1185));
      EXPECT_THAT(approximation.EvaluateDerivative(t_max_),
                  AlmostEquals(speed_function(t_max_), 0, 339));
    }
  }

  // A helper that splits an array in two chunks and applies distinct matchers
  // to the chunks.  Necessary because ElementsAre only supports 10 elements and
  // ElementsAreArray does not support matchers as arguments.
  template<typename Vector, typename Matcher1, typename Matcher2>
  void ExpectMultipart(std::vector<Vector> const& v,
                       Matcher1 const& matcher1,
                       Matcher2 const& matcher2) {
    std::vector<Vector> v_0_9(10);
    std::copy(v.begin(), v.begin() + 10, v_0_9.begin());
    EXPECT_THAT(v_0_9, matcher1);
    std::vector<Vector> v_10_end(v.size() - 10);
    std::copy(v.begin() + 10, v.end(), v_10_end.begin());
    EXPECT_THAT(v_10_end, matcher2);
  }

  Instant const t0_;
  Instant t_min_;
  Instant t_max_;
};

using ЧебышёвSeriesDeathTest = ЧебышёвSeriesTest;

TEST_F(ЧебышёвSeriesDeathTest, ConstructionErrors) {
  EXPECT_DEATH({
    ЧебышёвSeries<double> p({}, t_min_, t_max_);
  }, "at least 0");
  EXPECT_DEATH({
    ЧебышёвSeries<double> p({1}, t_max_, t_min_);
  }, "not be empty");
}

TEST_F(ЧебышёвSeriesDeathTest, EvaluationErrors) {
#ifdef _DEBUG
  EXPECT_DEATH({
    ЧебышёвSeries<double> p({1}, t_min_, t_max_);
    p.Evaluate(t_min_ - 10 * Second);
  }, ">= -1.1");
  EXPECT_DEATH({
    ЧебышёвSeries<double> p({1}, t_min_, t_max_);
    p.Evaluate(t_max_ + 10 * Second);
  }, "<= 1.1");
#endif
}

TEST_F(ЧебышёвSeriesTest, T0) {
  ЧебышёвSeries<double> t0({1}, t_min_, t_max_);
  EXPECT_EQ(1, t0.Evaluate(t0_ + 1 * Second));
  EXPECT_EQ(1, t0.Evaluate(t0_ + 3 * Second));
}

TEST_F(ЧебышёвSeriesTest, T1) {
  ЧебышёвSeries<double> t1({0, 1}, t_min_, t_max_);
  EXPECT_EQ(0, t1.Evaluate(t0_ + 1 * Second));
  EXPECT_EQ(1, t1.Evaluate(t0_ + 3 * Second));
}

TEST_F(ЧебышёвSeriesTest, T2) {
  ЧебышёвSeries<double> t2({0, 0, 1}, t_min_, t_max_);
  EXPECT_EQ(1, t2.Evaluate(t0_ + -1 * Second));
  EXPECT_EQ(-1, t2.Evaluate(t0_ + 1 * Second));
  EXPECT_EQ(1, t2.Evaluate(t0_ + 3 * Second));
}

TEST_F(ЧебышёвSeriesTest, T3) {
  ЧебышёвSeries<double> t3({0, 0, 0, 1}, t_min_, t_max_);
  EXPECT_EQ(-1, t3.Evaluate(t0_ + -1 * Second));
  EXPECT_EQ(0, t3.Evaluate(t0_ + 1 * Second));
  EXPECT_EQ(-1, t3.Evaluate(t0_ + 2 * Second));
  EXPECT_EQ(1, t3.Evaluate(t0_ + 3 * Second));
}

TEST_F(ЧебышёвSeriesTest, X5) {
  ЧебышёвSeries<double> x5({0.0, 10.0 / 16.0, 0, 5.0 / 16.0, 0, 1.0 / 16.0},
                           t_min_, t_max_);
  EXPECT_EQ(-1, x5.Evaluate(t0_ + -1 * Second));
  EXPECT_EQ(0, x5.Evaluate(t0_ + 1 * Second));
  EXPECT_EQ(1.0 / 1024.0, x5.Evaluate(t0_ + 1.5 * Second));
  EXPECT_EQ(1.0 / 32.0, x5.Evaluate(t0_ + 2 * Second));
  EXPECT_EQ(1, x5.Evaluate(t0_ + 3 * Second));
}

TEST_F(ЧебышёвSeriesTest, X6) {
  ЧебышёвSeries<double> x6(
      {10.0 / 32.0, 0, 15.0 / 32.0, 0, 6.0 / 32.0, 0, 1.0 / 32.0},
      t_min_, t_max_);
  EXPECT_EQ(1, x6.Evaluate(t0_ + -1 * Second));
  EXPECT_EQ(0, x6.Evaluate(t0_ + 1 * Second));
  EXPECT_EQ(1.0 / 4096.0, x6.Evaluate(t0_ + 1.5 * Second));
  EXPECT_EQ(1.0 / 64.0, x6.Evaluate(t0_ + 2 * Second));
  EXPECT_EQ(1, x6.Evaluate(t0_ + 3 * Second));
}

TEST_F(ЧебышёвSeriesTest, T2Dimension) {
  ЧебышёвSeries<Length> t2({0 * Metre, 0 * Metre, 1 * Metre}, t_min_, t_max_);
  EXPECT_EQ(1 * Metre, t2.Evaluate(t0_ + -1 * Second));
  EXPECT_EQ(-1 * Metre, t2.Evaluate(t0_ + 1 * Second));
  EXPECT_EQ(1 * Metre, t2.Evaluate(t0_ + 3 * Second));
}

TEST_F(ЧебышёвSeriesTest, X6Vector) {
  using V = Vector<Length, ICRFJ2000Ecliptic>;
  // {T3, X5, X6}
  V const c0 = V({0.0 * Metre, 0.0 * Metre, 10.0 / 32.0 * Metre});
  V const c1 = V({0.0 * Metre, 10.0 / 16.0 * Metre, 0.0 * Metre});
  V const c2 = V({0.0 * Metre, 0.0 * Metre, 15.0 / 32.0 * Metre});
  V const c3 = V({1.0 * Metre, 5.0 / 16.0 * Metre, 0.0 * Metre});
  V const c4 = V({0.0 * Metre, 0.0 * Metre, 6.0 / 32.0 * Metre});
  V const c5 = V({0.0 * Metre, 1.0 / 16.0 * Metre, 0 * Metre});
  V const c6 = V({0.0 * Metre, 0.0 * Metre, 1.0 / 32.0 * Metre});
  ЧебышёвSeries<Vector<Length, ICRFJ2000Ecliptic>> x6(
      {c0, c1, c2, c3, c4, c5, c6},
      t_min_, t_max_);
  EXPECT_EQ(V({-1 * Metre, -1 * Metre, 1 * Metre}),
            x6.Evaluate(t0_ + -1 * Second));
  EXPECT_EQ(V({0 * Metre, 0 * Metre, 0 * Metre}),
            x6.Evaluate(t0_ + 1 * Second));
  EXPECT_EQ(V({-1 * Metre, 1.0 / 32.0 * Metre, 1 / 64.0 * Metre}),
            x6.Evaluate(t0_ + 2 * Second));
  EXPECT_EQ(V({1 * Metre, 1 * Metre, 1 * Metre}),
            x6.Evaluate(t0_ + 3 * Second));
}

TEST_F(ЧебышёвSeriesDeathTest, SerializationError) {
  ЧебышёвSeries<Speed> v({1 * Metre / Second,
                          -2 * Metre / Second,
                          5 * Metre / Second},
                         t_min_, t_max_);
  ЧебышёвSeries<double> d({7, 8, -1}, t_min_, t_max_);

  EXPECT_DEATH({
    serialization::ЧебышёвSeries message;
    v.WriteToMessage(&message);
    ЧебышёвSeries<double>::ReadFromMessage(message);
  }, "has_double");
  EXPECT_DEATH({
    serialization::ЧебышёвSeries message;
    d.WriteToMessage(&message);
    ЧебышёвSeries<Speed>::ReadFromMessage(message);
  }, "has_quantity");
}

TEST_F(ЧебышёвSeriesTest, SerializationSuccess) {
  {
    serialization::ЧебышёвSeries message;
    ЧебышёвSeries<Speed> const v1({1 * Metre / Second,
                                   -2 * Metre / Second,
                                   5 * Metre / Second},
                                  t_min_, t_max_);
    v1.WriteToMessage(&message);
    EXPECT_EQ(3, message.coefficient_size());
    EXPECT_FALSE(message.coefficient(0).has_double_());
    EXPECT_TRUE(message.coefficient(0).has_quantity());
    EXPECT_EQ(0x7C01, message.coefficient(0).quantity().dimensions());
    EXPECT_EQ(1.0, message.coefficient(0).quantity().magnitude());
    EXPECT_TRUE(message.has_t_min());
    EXPECT_TRUE(message.t_min().has_scalar());
    EXPECT_TRUE(message.t_min().scalar().has_dimensions());
    EXPECT_TRUE(message.t_min().scalar().has_magnitude());
    EXPECT_EQ(-1.0, message.t_min().scalar().magnitude());
    EXPECT_TRUE(message.has_t_max());
    EXPECT_TRUE(message.t_max().has_scalar());
    EXPECT_TRUE(message.t_max().scalar().has_dimensions());
    EXPECT_TRUE(message.t_max().scalar().has_magnitude());
    EXPECT_EQ(3.0, message.t_max().scalar().magnitude());
    ЧебышёвSeries<Speed> const v2 =
        ЧебышёвSeries<Speed>::ReadFromMessage(message);
    EXPECT_EQ(v1, v2);
  }
  {
    serialization::ЧебышёвSeries message;
    ЧебышёвSeries<double> const d1({-1, 2, 5}, t_min_, t_max_);
    d1.WriteToMessage(&message);
    EXPECT_EQ(3, message.coefficient_size());
    EXPECT_TRUE(message.coefficient(0).has_double_());
    EXPECT_FALSE(message.coefficient(0).has_quantity());
    EXPECT_EQ(-1.0, message.coefficient(0).double_());
    EXPECT_TRUE(message.has_t_min());
    EXPECT_TRUE(message.t_min().has_scalar());
    EXPECT_TRUE(message.t_min().scalar().has_dimensions());
    EXPECT_TRUE(message.t_min().scalar().has_magnitude());
    EXPECT_EQ(-1.0, message.t_min().scalar().magnitude());
    EXPECT_TRUE(message.has_t_max());
    EXPECT_TRUE(message.t_max().has_scalar());
    EXPECT_TRUE(message.t_max().scalar().has_dimensions());
    EXPECT_TRUE(message.t_max().scalar().has_magnitude());
    EXPECT_EQ(3.0, message.t_max().scalar().magnitude());
    ЧебышёвSeries<double> const d2 =
        ЧебышёвSeries<double>::ReadFromMessage(message);
    EXPECT_EQ(d1, d2);
  }
}

TEST_F(ЧебышёвSeriesTest, NewhallApproximation) {
  std::vector<Length> length_absolute_errors;
  std::vector<Speed> speed_absolute_errors;

  auto near_length = [](Length const& length) {
    return AllOf(Gt(0.9 * length), Lt(length));
  };
  auto near_speed = [](Speed const& speed) {
    return AllOf(Gt(0.9 * speed), Lt(speed));
  };

  {
    auto length_function = [this](Instant const t) -> Length {
      return 0.5 * Metre + 2 * Metre *
             std::sin((t - t_min_) / (0.3 * Second)) *
             std::exp((t - t_min_) / (1 * Second));
    };
    auto speed_function = [this](Instant const t) -> Speed {
      return ((2 * Metre) / (0.3 * Second) *
                   std::cos((t - t_min_) / (0.3 * Second)) +
              (2 * Metre / Second) *
                   std::sin((t - t_min_) / (0.3 * Second))) *
                       std::exp((t - t_min_) / (1 * Second));
    };

    NewhallApproximationErrors(length_function,
                               speed_function,
                               length_absolute_errors,
                               speed_absolute_errors);
  }

  ExpectMultipart(length_absolute_errors,
                  ElementsAre(near_length(1.7e2 * Metre),
                              near_length(4.7e1 * Metre),
                              near_length(4.3e1 * Metre),
                              near_length(3.8e1 * Metre),
                              near_length(1.5e1 * Metre),
                              near_length(6.3 * Metre),
                              near_length(4.9 * Metre),
                              near_length(6.5e-1 * Metre),
                              near_length(2.0e-1 * Metre),
                              near_length(7.9e-2 * Metre)),
                  ElementsAre(near_length(1.3e-2 * Metre),
                              near_length(1.6e-2 * Metre),
                              near_length(4.3e-3 * Metre),
                              near_length(1.7e-3 * Metre),
                              near_length(7.6e-4 * Metre)));
  ExpectMultipart(speed_absolute_errors,
                  ElementsAre(near_speed(2.3e2 * Metre / Second),
                              near_speed(1.3e2 * Metre / Second),
                              near_speed(1.2e2 * Metre / Second),
                              near_speed(1.1e2 * Metre / Second),
                              near_speed(4.5e1 * Metre / Second),
                              near_speed(2.8e1 * Metre / Second),
                              near_speed(2.2e1 * Metre / Second),
                              near_speed(3.6 * Metre / Second),
                              near_speed(1.6 * Metre / Second),
                              near_speed(7.3e-1 * Metre / Second)),
                  ElementsAre(near_speed(1.3e-1 * Metre / Second),
                              near_speed(1.5e-1 * Metre / Second),
                              near_speed(4.4e-2 * Metre / Second),
                              near_speed(1.8e-2 * Metre / Second),
                              near_speed(8.2e-3 * Metre / Second)));

  {
    auto length_function = [this](Instant const t) -> Length {
      return 5 * Metre * (1 + (t - t_min_) / (0.3 * Second) +
                          std::pow((t - t_min_) / (4 * Second), 7));
    };
    auto speed_function = [this](Instant const t) -> Speed {
      return 5 * Metre * (1 / (0.3 * Second) +
                          (7 / (4 * Second)) *
                              std::pow((t - t_min_) / (4 * Second), 6));
    };

    NewhallApproximationErrors(length_function,
                               speed_function,
                               length_absolute_errors,
                               speed_absolute_errors);
  }

  ExpectMultipart(length_absolute_errors,
                  ElementsAre(near_length(2.0 * Metre),
                              near_length(2.9e-1 * Metre),
                              near_length(3.6e-2 * Metre),
                              near_length(2.3e-3 * Metre),
                              near_length(2.9e-14 * Metre),
                              near_length(2.9e-14 * Metre),
                              near_length(2.9e-14 * Metre),
                              near_length(4.3e-14 * Metre),
                              near_length(3.6e-14 * Metre),
                              near_length(2.9e-14 * Metre)),
                  ElementsAre(near_length(1.5e-14 * Metre),
                              near_length(1.5e-14 * Metre),
                              near_length(2.9e-14 * Metre),
                              near_length(2.9e-14 * Metre),
                              near_length(7.2e-14 * Metre)));
  ExpectMultipart(speed_absolute_errors,
                  ElementsAre(near_speed(1.8 * Metre / Second),
                              near_speed(4.6e-1 * Metre / Second),
                              near_speed(7.4e-2 * Metre / Second),
                              near_speed(6.0e-3 * Metre / Second),
                              near_speed(2.5e-14 * Metre / Second),
                              near_speed(2.2e-14 * Metre / Second),
                              near_speed(2.2e-14 * Metre / Second),
                              near_speed(2.9e-14 * Metre / Second),
                              near_speed(2.2e-14 * Metre / Second),
                              near_speed(2.9e-14 * Metre / Second)),
                  ElementsAre(AllOf(Gt(5.3e-14 * Metre / Second),
                                    Lt(6.1e-14 * Metre / Second)),
                              near_speed(6.8e-14 * Metre / Second),
                              near_speed(3.5e-13 * Metre / Second),
                              near_speed(8.5e-13 * Metre / Second),
                              near_speed(1.3e-12 * Metre / Second)));
}

}  // namespace internal_чебышёв_series
}  // namespace numerics
}  // namespace principia
