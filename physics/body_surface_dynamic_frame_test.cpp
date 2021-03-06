﻿
#include "physics/body_surface_dynamic_frame.hpp"

#include <memory>

#include "astronomy/frames.hpp"
#include "geometry/frame.hpp"
#include "geometry/grassmann.hpp"
#include "geometry/named_quantities.hpp"
#include "geometry/rotation.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "integrators/symplectic_runge_kutta_nyström_integrator.hpp"
#include "physics/ephemeris.hpp"
#include "physics/mock_continuous_trajectory.hpp"
#include "physics/mock_ephemeris.hpp"
#include "physics/solar_system.hpp"
#include "quantities/constants.hpp"
#include "quantities/quantities.hpp"
#include "quantities/si.hpp"
#include "serialization/geometry.pb.h"
#include "serialization/physics.pb.h"
#include "testing_utilities/almost_equals.hpp"
#include "testing_utilities/componentwise.hpp"
#include "testing_utilities/numerics.hpp"
#include "testing_utilities/vanishes_before.hpp"

namespace principia {
namespace physics {
namespace internal_body_surface_dynamic_frame {

using astronomy::ICRFJ2000Equator;
using base::check_not_null;
using base::dynamic_cast_not_null;
using geometry::Bivector;
using geometry::Displacement;
using geometry::Frame;
using geometry::Instant;
using geometry::Rotation;
using geometry::Velocity;
using geometry::Vector;
using quantities::GravitationalParameter;
using quantities::Pow;
using quantities::Time;
using quantities::si::Kilo;
using quantities::si::Kilogram;
using quantities::si::Metre;
using quantities::si::Milli;
using quantities::si::Radian;
using quantities::si::Second;
using testing_utilities::AbsoluteError;
using testing_utilities::AlmostEquals;
using testing_utilities::Componentwise;
using testing_utilities::VanishesBefore;
using ::testing::Eq;
using ::testing::InSequence;
using ::testing::IsNull;
using ::testing::Lt;
using ::testing::Not;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::_;

namespace {

char constexpr big[] = "Big";
char constexpr small[] = "Small";

}  // namespace

class BodySurfaceDynamicFrameTest : public ::testing::Test {
 protected:
  // The rotating frame centred on the big body and directed to the small one.
  using BigSmallFrame = Frame<serialization::Frame::TestTag,
                              serialization::Frame::TEST, /*inertial=*/false>;
  using MockFrame = Frame<serialization::Frame::TestTag,
                          serialization::Frame::TEST1, /*inertial=*/false>;

  BodySurfaceDynamicFrameTest()
      : period_(10 * π * sqrt(5.0 / 7.0) * Second),
        solar_system_(SOLUTION_DIR / "astronomy" /
                          "test_gravity_model_two_bodies.proto.txt",
                      SOLUTION_DIR / "astronomy" /
                          "test_initial_state_two_bodies_circular.proto.txt"),
        t0_(solar_system_.epoch()),
        ephemeris_(solar_system_.MakeEphemeris(
            /*fitting_tolerance=*/1 * Milli(Metre),
            Ephemeris<ICRFJ2000Equator>::FixedStepParameters(
                integrators::McLachlanAtela1992Order4Optimal<
                    Position<ICRFJ2000Equator>>(),
                /*step=*/10 * Milli(Second)))),
        big_(dynamic_cast_not_null<RotatingBody<ICRFJ2000Equator> const*>(
            solar_system_.massive_body(*ephemeris_, big))),
        big_initial_state_(solar_system_.degrees_of_freedom(big)),
        big_gravitational_parameter_(
            solar_system_.gravitational_parameter(big)),
        small_initial_state_(solar_system_.degrees_of_freedom(small)),
        small_gravitational_parameter_(
            solar_system_.gravitational_parameter(small)),
        // A body that rotates at the same speed as the one in
        // BodyCentredBodyDirectionDynamicFrameTest, so it produces the same
        // fictitious forces.
        centre_(MassiveBody::Parameters(1 * Kilogram),
                RotatingBody<ICRFJ2000Equator>::Parameters(
                    /*mean_radius=*/1 * Metre,
                    /*reference_angle=*/0 * Radian,
                    /*reference_instant=*/t0_,
                    /*angular_frequency=*/10 * Radian / Second,
                    /*ascension_of_pole=*/0 * Radian,
                    /*declination_of_pole=*/π / 2 * Radian)),
        massive_centre_(&centre_) {
    EXPECT_CALL(mock_ephemeris_, trajectory(_))
        .WillOnce(Return(&mock_centre_trajectory_));
    mock_frame_ =
        std::make_unique<BodySurfaceDynamicFrame<ICRFJ2000Equator, MockFrame>>(
            &mock_ephemeris_, &centre_);

    ephemeris_->Prolong(t0_ + 2 * period_);
    big_frame_ = std::make_unique<
        BodySurfaceDynamicFrame<ICRFJ2000Equator, BigSmallFrame>>(
        ephemeris_.get(), big_);
  }

  Time const period_;
  SolarSystem<ICRFJ2000Equator> solar_system_;
  Instant const t0_;
  std::unique_ptr<Ephemeris<ICRFJ2000Equator>> const ephemeris_;
  RotatingBody<ICRFJ2000Equator> const* const big_;
  DegreesOfFreedom<ICRFJ2000Equator> big_initial_state_;
  GravitationalParameter big_gravitational_parameter_;
  DegreesOfFreedom<ICRFJ2000Equator> small_initial_state_;
  GravitationalParameter small_gravitational_parameter_;
  RotatingBody<ICRFJ2000Equator> const centre_;
  not_null<MassiveBody const*> const massive_centre_;
  StrictMock<MockEphemeris<ICRFJ2000Equator>> mock_ephemeris_;

  std::unique_ptr<BodySurfaceDynamicFrame<ICRFJ2000Equator, MockFrame>>
      mock_frame_;
  std::unique_ptr<
      BodySurfaceDynamicFrame<ICRFJ2000Equator, BigSmallFrame>> big_frame_;
  StrictMock<MockContinuousTrajectory<ICRFJ2000Equator>>
      mock_centre_trajectory_;
};


TEST_F(BodySurfaceDynamicFrameTest, ToBigSmallFrameAtTime) {
  int const steps = 100;

  for (Instant t = t0_; t < t0_ + 1 * period_; t += period_ / steps) {
    auto const to_big_frame_at_t = big_frame_->ToThisFrameAtTime(t);

    // Check that the big body is at the origin and doesn't move.  Check that
    // the small body is at a fixed position in the sky.
    DegreesOfFreedom<ICRFJ2000Equator> const big_in_inertial_frame_at_t =
        solar_system_.trajectory(*ephemeris_, big).
            EvaluateDegreesOfFreedom(t);
    DegreesOfFreedom<ICRFJ2000Equator> const small_in_inertial_frame_at_t =
        solar_system_.trajectory(*ephemeris_, small).
            EvaluateDegreesOfFreedom(t);

    DegreesOfFreedom<BigSmallFrame> const big_in_big_small_at_t =
        to_big_frame_at_t(big_in_inertial_frame_at_t);
    DegreesOfFreedom<BigSmallFrame> const small_in_big_small_at_t =
        to_big_frame_at_t(small_in_inertial_frame_at_t);
    EXPECT_THAT(AbsoluteError(big_in_big_small_at_t.position(),
                              BigSmallFrame::origin),
                Lt(1.0e-6 * Metre));
    EXPECT_THAT(AbsoluteError(big_in_big_small_at_t.velocity(),
                              Velocity<BigSmallFrame>()),
                Lt(1.0e-4 * Metre / Second));
    EXPECT_THAT(AbsoluteError(small_in_big_small_at_t.position(),
                              Displacement<BigSmallFrame>({
                                  0 * Kilo(Metre),
                                  5.0 * Kilo(Metre),
                                  0 * Kilo(Metre)}) + BigSmallFrame::origin),
                Lt(2.0e-4 * Metre));
    EXPECT_THAT(AbsoluteError(small_in_big_small_at_t.velocity(),
                              Velocity<BigSmallFrame>()),
                Lt(4.0e-3 * Metre / Second));
  }
}

TEST_F(BodySurfaceDynamicFrameTest, Inverse) {
  int const steps = 100;
  for (Instant t = t0_; t < t0_ + 1 * period_; t += period_ / steps) {
    auto const from_big_frame_at_t =
        big_frame_->FromThisFrameAtTime(t);
    auto const to_big_frame_at_t = big_frame_->ToThisFrameAtTime(t);
    auto const small_initial_state_transformed_and_back =
        from_big_frame_at_t(to_big_frame_at_t(
            small_initial_state_));
    EXPECT_THAT(
        AbsoluteError(small_initial_state_transformed_and_back.position(),
                      small_initial_state_.position()),
        Lt(1.0e-11 * Metre));
    EXPECT_THAT(
        AbsoluteError(small_initial_state_transformed_and_back.velocity(),
                      small_initial_state_.velocity()),
        Lt(1.0e-11 * Metre / Second));
  }
}

// The test point is at the origin and in motion.  The acceleration is purely
// due to Coriolis.
TEST_F(BodySurfaceDynamicFrameTest, CoriolisAcceleration) {
  Instant const t = t0_ + 0 * Second;
  // The velocity is opposed to the motion and away from the centre.
  DegreesOfFreedom<MockFrame> const point_dof =
      {Displacement<MockFrame>({0 * Metre, 0 * Metre, 0 * Metre}) +
           MockFrame::origin,
       Velocity<MockFrame>({10 * Metre / Second,
                            20 * Metre / Second,
                            30 * Metre / Second})};
  DegreesOfFreedom<ICRFJ2000Equator> const centre_dof =
      {Displacement<ICRFJ2000Equator>({0 * Metre, 0 * Metre, 0 * Metre}) +
           ICRFJ2000Equator::origin,
       Velocity<ICRFJ2000Equator>()};

  EXPECT_CALL(mock_centre_trajectory_, EvaluateDegreesOfFreedom(t))
      .Times(2)
      .WillRepeatedly(Return(centre_dof));
  {
    InSequence s;
    EXPECT_CALL(mock_ephemeris_,
                ComputeGravitationalAccelerationOnMassiveBody(
                    massive_centre_, t))
        .WillOnce(Return(Vector<Acceleration, ICRFJ2000Equator>({
                             0 * Metre / Pow<2>(Second),
                             0 * Metre / Pow<2>(Second),
                             0 * Metre / Pow<2>(Second)})));
    EXPECT_CALL(mock_ephemeris_,
                ComputeGravitationalAccelerationOnMasslessBody(_, t))
        .WillOnce(Return(Vector<Acceleration, ICRFJ2000Equator>()));
  }

  // The Coriolis acceleration is towards the centre and opposed to the motion.
  EXPECT_THAT(mock_frame_->GeometricAcceleration(t, point_dof).coordinates(),
              Componentwise(AlmostEquals(400 * Metre / Pow<2>(Second), 1),
                            AlmostEquals(-200 * Metre / Pow<2>(Second), 0),
                            VanishesBefore(1 * Metre / Pow<2>(Second), 110)));
}

// The test point doesn't move so the acceleration is purely centrifugal.
TEST_F(BodySurfaceDynamicFrameTest, CentrifugalAcceleration) {
  Instant const t = t0_ + 0 * Second;
  DegreesOfFreedom<MockFrame> const point_dof =
      {Displacement<MockFrame>({10 * Metre, 20 * Metre, 30 * Metre}) +
           MockFrame::origin,
       Velocity<MockFrame>({0 * Metre / Second,
                            0 * Metre / Second,
                            0 * Metre / Second})};
  DegreesOfFreedom<ICRFJ2000Equator> const centre_dof =
      {Displacement<ICRFJ2000Equator>({0 * Metre, 0 * Metre, 0 * Metre}) +
           ICRFJ2000Equator::origin,
       Velocity<ICRFJ2000Equator>({0 * Metre / Second,
                                   0 * Metre / Second,
                                   0 * Metre / Second})};
  EXPECT_CALL(mock_centre_trajectory_, EvaluateDegreesOfFreedom(t))
      .Times(2)
      .WillRepeatedly(Return(centre_dof));
  {
    InSequence s;
    EXPECT_CALL(mock_ephemeris_,
                ComputeGravitationalAccelerationOnMassiveBody(
                    massive_centre_, t))
        .WillOnce(Return(Vector<Acceleration, ICRFJ2000Equator>({
                             0 * Metre / Pow<2>(Second),
                             0 * Metre / Pow<2>(Second),
                             0 * Metre / Pow<2>(Second)})));
    EXPECT_CALL(mock_ephemeris_,
                ComputeGravitationalAccelerationOnMasslessBody(_, t))
        .WillOnce(Return(Vector<Acceleration, ICRFJ2000Equator>()));
  }

  EXPECT_THAT(mock_frame_->GeometricAcceleration(t, point_dof).coordinates(),
              Componentwise(AlmostEquals(1e3 * Metre / Pow<2>(Second), 0),
                            AlmostEquals(2e3 * Metre / Pow<2>(Second), 1),
                            VanishesBefore(1 * Metre / Pow<2>(Second), 552)));
}

// No Euler acceleration in this dynamic frame.

// A linear acceleration identical for both bodies.  The test point doesn't
// move.  The resulting acceleration combines centrifugal and linear.
TEST_F(BodySurfaceDynamicFrameTest, LinearAcceleration) {
  Instant const t = t0_ + 0 * Second;
  DegreesOfFreedom<MockFrame> const point_dof =
      {Displacement<MockFrame>({10 * Metre, 20 * Metre, 30 * Metre}) +
           MockFrame::origin,
       Velocity<MockFrame>({0 * Metre / Second,
                            0 * Metre / Second,
                            0 * Metre / Second})};
  DegreesOfFreedom<ICRFJ2000Equator> const centre_dof =
      {Displacement<ICRFJ2000Equator>({0 * Metre, 0 * Metre, 0 * Metre}) +
           ICRFJ2000Equator::origin,
       Velocity<ICRFJ2000Equator>({0 * Metre / Second,
                                   0 * Metre / Second,
                                   0 * Metre / Second})};

  EXPECT_CALL(mock_centre_trajectory_, EvaluateDegreesOfFreedom(t))
      .Times(2)
      .WillRepeatedly(Return(centre_dof));
  {
    // The acceleration is linear + centripetal.
    InSequence s;
    EXPECT_CALL(mock_ephemeris_,
                ComputeGravitationalAccelerationOnMassiveBody(
                    massive_centre_, t))
        .WillOnce(Return(Vector<Acceleration, ICRFJ2000Equator>({
                             -160 * Metre / Pow<2>(Second),
                             120 * Metre / Pow<2>(Second),
                             300 * Metre / Pow<2>(Second)})));
    EXPECT_CALL(mock_ephemeris_,
                ComputeGravitationalAccelerationOnMasslessBody(_, t))
        .WillOnce(Return(Vector<Acceleration, ICRFJ2000Equator>()));
  }

  // The acceleration is linear + centrifugal.
  EXPECT_THAT(mock_frame_->GeometricAcceleration(t, point_dof),
              AlmostEquals(Vector<Acceleration, MockFrame>({
                               (-120 + 1e3) * Metre / Pow<2>(Second),
                               (-160 + 2e3) * Metre / Pow<2>(Second),
                               -300 * Metre / Pow<2>(Second)}), 2));
}

TEST_F(BodySurfaceDynamicFrameTest, GeometricAcceleration) {
  Instant const t = t0_ + period_;
  DegreesOfFreedom<BigSmallFrame> const point_dof =
      {Displacement<BigSmallFrame>({10 * Metre, 20 * Metre, 30 * Metre}) +
           BigSmallFrame::origin,
       Velocity<BigSmallFrame>({3 * Metre / Second,
                                2 * Metre / Second,
                                1 * Metre / Second})};
  // We trust the functions to compute the values correctly, but this test
  // ensures that we don't get NaNs.
  EXPECT_THAT(big_frame_->GeometricAcceleration(t, point_dof),
              AlmostEquals(Vector<Acceleration, BigSmallFrame>({
                  -9.54504983899937710e5 * Metre / Pow<2>(Second),
                  -1.90900569196062256e6 * Metre / Pow<2>(Second),
                  -2.86351379198155506e6 * Metre / Pow<2>(Second)}), 0, 2));
}

TEST_F(BodySurfaceDynamicFrameTest, Serialization) {
  serialization::DynamicFrame message;
  big_frame_->WriteToMessage(&message);

  EXPECT_TRUE(message.HasExtension(
      serialization::BodySurfaceDynamicFrame::extension));
  auto const extension = message.GetExtension(
      serialization::BodySurfaceDynamicFrame::extension);
  EXPECT_TRUE(extension.has_centre());
  EXPECT_EQ(0, extension.centre());

  auto const read_big_frame =
      DynamicFrame<ICRFJ2000Equator, BigSmallFrame>::ReadFromMessage(
          message, ephemeris_.get());
  EXPECT_THAT(read_big_frame, Not(IsNull()));

  Instant const t = t0_ + period_;
  DegreesOfFreedom<BigSmallFrame> const point_dof =
      {Displacement<BigSmallFrame>({10 * Metre, 20 * Metre, 30 * Metre}) +
           BigSmallFrame::origin,
       Velocity<BigSmallFrame>({3 * Metre / Second,
                                2 * Metre / Second,
                                1 * Metre / Second})};
  EXPECT_EQ(big_frame_->GeometricAcceleration(t, point_dof),
            read_big_frame->GeometricAcceleration(t, point_dof));
}

}  // namespace internal_body_surface_dynamic_frame
}  // namespace physics
}  // namespace principia
