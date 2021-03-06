﻿
#include "ksp_plugin/interface.hpp"

#include "base/not_null.hpp"
#include "geometry/identity.hpp"
#include "geometry/named_quantities.hpp"
#include "geometry/orthogonal_map.hpp"
#include "geometry/rotation.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "integrators/embedded_explicit_runge_kutta_nyström_integrator.hpp"
#include "ksp_plugin/frames.hpp"
#include "ksp_plugin/identification.hpp"
#include "ksp_plugin_test/mock_flight_plan.hpp"
#include "ksp_plugin_test/mock_manœuvre.hpp"
#include "ksp_plugin_test/mock_plugin.hpp"
#include "ksp_plugin_test/mock_renderer.hpp"
#include "ksp_plugin_test/mock_vessel.hpp"
#include "physics/body_centred_non_rotating_dynamic_frame.hpp"
#include "physics/discrete_trajectory.hpp"
#include "physics/dynamic_frame.hpp"
#include "physics/massive_body.hpp"
#include "physics/mock_continuous_trajectory.hpp"
#include "physics/mock_dynamic_frame.hpp"
#include "physics/mock_ephemeris.hpp"
#include "physics/rigid_motion.hpp"
#include "quantities/constants.hpp"
#include "quantities/si.hpp"
#include "testing_utilities/actions.hpp"
#include "testing_utilities/almost_equals.hpp"

namespace principia {
namespace interface {

using base::check_not_null;
using base::make_not_null_unique;
using base::not_null;
using geometry::AngularVelocity;
using geometry::Identity;
using geometry::OrthogonalMap;
using geometry::RigidTransformation;
using geometry::Rotation;
using integrators::DormandElMikkawyPrince1986RKN434FM;
using ksp_plugin::Barycentric;
using ksp_plugin::Index;
using ksp_plugin::MockFlightPlan;
using ksp_plugin::MockManœuvre;
using ksp_plugin::MockPlugin;
using ksp_plugin::MockRenderer;
using ksp_plugin::MockVessel;
using ksp_plugin::Navigation;
using ksp_plugin::WorldSun;
using physics::BodyCentredNonRotatingDynamicFrame;
using physics::DiscreteTrajectory;
using physics::DynamicFrame;
using physics::MassiveBody;
using physics::MockContinuousTrajectory;
using physics::MockDynamicFrame;
using physics::MockEphemeris;
using physics::RigidMotion;
using quantities::constants::StandardGravity;
using quantities::si::Kilo;
using quantities::si::Kilogram;
using quantities::si::Newton;
using quantities::si::Second;
using quantities::si::Tonne;
using testing_utilities::AlmostEquals;
using testing_utilities::FillUniquePtr;
using ::testing::AllOf;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::Property;
using ::testing::Ref;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::SetArgReferee;
using ::testing::StrictMock;
using ::testing::_;

namespace {

char const vessel_guid[] = "123-456";
Index const celestial_index = 1;

}  // namespace

MATCHER_P4(BurnMatches, thrust, specific_impulse, initial_time, Δv, "") {
  return arg.thrust == thrust &&
         arg.specific_impulse == specific_impulse &&
         arg.initial_time == initial_time &&
         arg.Δv == Δv;
}

class InterfaceFlightPlanTest : public ::testing::Test {
 protected:
  InterfaceFlightPlanTest()
    : plugin_(make_not_null_unique<StrictMock<MockPlugin>>()),
      const_plugin_(plugin_.get()) {}

  not_null<std::unique_ptr<StrictMock<MockPlugin>>> const plugin_;
  StrictMock<MockPlugin> const* const const_plugin_;
  Instant const t0_;
};

TEST_F(InterfaceFlightPlanTest, FlightPlan) {
  Burn burn = {/*thrust_in_kilonewtons=*/1,
               /*specific_impulse_in_seconds_g0=*/2,
               /*frame=*/{/*extension=*/6000, /*centre=*/celestial_index},
               /*initial_time=*/3,
               /*delta_v=*/{4, 5, 6}};
  StrictMock<MockVessel> vessel;
  StrictMock<MockFlightPlan> flight_plan;

  EXPECT_CALL(*plugin_, HasVessel(vessel_guid))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*plugin_, GetVessel(vessel_guid))
      .WillRepeatedly(Return(&vessel));
  EXPECT_CALL(vessel, has_flight_plan())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(vessel, flight_plan())
      .WillRepeatedly(ReturnRef(flight_plan));

  EXPECT_TRUE(principia__FlightPlanExists(plugin_.get(), vessel_guid));

  EXPECT_CALL(*plugin_, CreateFlightPlan(vessel_guid,
                                         Instant() + 30 * Second,
                                         100 * Tonne));
  principia__FlightPlanCreate(plugin_.get(),
                              vessel_guid,
                              /*final_time=*/30,
                              /*mass_in_tonnes=*/100);

  EXPECT_CALL(flight_plan, SetDesiredFinalTime(Instant() + 60 * Second))
      .WillOnce(Return(true));
  EXPECT_TRUE(principia__FlightPlanSetDesiredFinalTime(plugin_.get(),
                                                       vessel_guid,
                                                       60));

  EXPECT_CALL(flight_plan, initial_time())
      .WillOnce(Return(Instant() + 3 * Second));
  EXPECT_EQ(3, principia__FlightPlanGetInitialTime(plugin_.get(), vessel_guid));

  EXPECT_CALL(flight_plan, desired_final_time())
      .WillOnce(Return(Instant() + 4 * Second));
  EXPECT_EQ(4, principia__FlightPlanGetDesiredFinalTime(plugin_.get(),
                                                        vessel_guid));

  EXPECT_CALL(
      flight_plan,
      SetAdaptiveStepParameters(AllOf(
          Property(&Ephemeris<Barycentric>::AdaptiveStepParameters::max_steps,
                   11),
          Property(&Ephemeris<Barycentric>::AdaptiveStepParameters::
                       length_integration_tolerance,
                   22 * Metre),
          Property(&Ephemeris<Barycentric>::AdaptiveStepParameters::
                       speed_integration_tolerance,
                   33 * Metre / Second))))
      .WillOnce(Return(true));
  EXPECT_TRUE(principia__FlightPlanSetAdaptiveStepParameters(
                  plugin_.get(),
                  vessel_guid,
                  {/*integrator_kind=*/1,
                   /*max_step=*/11,
                   /*length_integration_tolerance=*/22,
                   /*speed_integration_tolerance=*/33}));

  Ephemeris<Barycentric>::AdaptiveStepParameters adaptive_step_parameters(
      DormandElMikkawyPrince1986RKN434FM<Position<Barycentric>>(),
      /*max_steps=*/111,
      /*length_integration_tolerance=*/222 * Metre,
      /*speed_integration_tolerance=*/333 * Metre / Second);
  EXPECT_CALL(flight_plan, adaptive_step_parameters())
      .WillOnce(ReturnRef(adaptive_step_parameters));
  AdaptiveStepParameters expected_adaptive_step_parameters = {
      /*integrator_kind=*/1,
      /*max_step=*/111,
      /*length_integration_tolerance=*/222,
      /*speed_integration_tolerance=*/333};
  EXPECT_EQ(expected_adaptive_step_parameters,
            principia__FlightPlanGetAdaptiveStepParameters(
                plugin_.get(), vessel_guid));

  EXPECT_CALL(*plugin_,
              FillBodyCentredNonRotatingNavigationFrame(celestial_index, _))
      .WillOnce(FillUniquePtr<1>(
                    new StrictMock<MockDynamicFrame<Barycentric, Navigation>>));
  EXPECT_CALL(flight_plan,
              AppendConstRef(
                  BurnMatches(1 * Kilo(Newton),
                              2 * Second * StandardGravity,
                              Instant() + 3 * Second,
                              Velocity<Frenet<Navigation>>(
                                  {4 * (Metre / Second),
                                   5 * (Metre / Second),
                                   6 * (Metre / Second)}))))
      .WillOnce(Return(true));
  EXPECT_TRUE(principia__FlightPlanAppend(plugin_.get(), vessel_guid, burn));

  EXPECT_CALL(flight_plan, number_of_manœuvres())
      .WillOnce(Return(4));
  EXPECT_EQ(4, principia__FlightPlanNumberOfManoeuvres(plugin_.get(),
                                                       vessel_guid));

  auto const plotting_frame =
      make_not_null_unique<MockDynamicFrame<Barycentric, Navigation>>();

  MockEphemeris<Barycentric> ephemeris;
  MassiveBody const centre(MassiveBody::Parameters("centre", 1 * Kilogram));
  MockContinuousTrajectory<Barycentric> centre_trajectory;
  EXPECT_CALL(ephemeris, trajectory(check_not_null(&centre)))
      .WillOnce(Return(&centre_trajectory));
  // Cannot use a mock here since we use |dynamic_cast| to find the type of the
  // actual frame.
  BodyCentredNonRotatingDynamicFrame<Barycentric, Navigation> const* const
      navigation_manœuvre_frame =
          new BodyCentredNonRotatingDynamicFrame<Barycentric, Navigation>(
            &ephemeris,
            &centre);
  MockManœuvre<Barycentric, Navigation> navigation_manœuvre(
      10 * Kilo(Newton),
      20 * Tonne,
      30 * Second * StandardGravity,
      Vector<double, Frenet<Navigation>>({1, 1, 1}),
      std::unique_ptr<DynamicFrame<Barycentric, Navigation> const>(
          navigation_manœuvre_frame),
      /*is_inertially_fixed=*/true);
  navigation_manœuvre.set_initial_time(Instant());
  navigation_manœuvre.set_duration(7 * Second);
  auto const barycentric_to_plotting = RigidMotion<Barycentric, Navigation>(
      RigidTransformation<Barycentric, Navigation>(
          Barycentric::origin,
          Navigation::origin,
          OrthogonalMap<Barycentric, Navigation>::Identity()),
      AngularVelocity<Barycentric>(),
      Velocity<Barycentric>());
  MockRenderer renderer;
  auto const identity = Rotation<Barycentric, AliceSun>::Identity();
  EXPECT_CALL(*plugin_, renderer()).WillRepeatedly(ReturnRef(renderer));
  EXPECT_CALL(*const_plugin_, renderer()).WillRepeatedly(ReturnRef(renderer));
  EXPECT_CALL(*plugin_, PlanetariumRotation())
      .WillRepeatedly(ReturnRef(identity));
  EXPECT_CALL(flight_plan, GetManœuvre(3))
      .WillOnce(ReturnRef(navigation_manœuvre));
  EXPECT_CALL(navigation_manœuvre, InertialDirection())
      .WillOnce(Return(Vector<double, Barycentric>({40, 50, 60})));
  EXPECT_CALL(*plugin_, CelestialIndexOfBody(Ref(centre)))
      .WillOnce(Return(celestial_index));
  EXPECT_CALL(renderer, BarycentricToWorldSun(_))
      .WillOnce(Return(OrthogonalMap<Barycentric, WorldSun>::Identity()));
  auto const navigation_manoeuvre =
      principia__FlightPlanGetManoeuvre(plugin_.get(),
                                        vessel_guid,
                                        3);

  EXPECT_EQ(10, navigation_manoeuvre.burn.thrust_in_kilonewtons);
  EXPECT_EQ(6000, navigation_manoeuvre.burn.frame.extension);
  EXPECT_EQ(celestial_index, navigation_manoeuvre.burn.frame.centre_index);
  EXPECT_EQ(20, navigation_manoeuvre.initial_mass_in_tonnes);
  EXPECT_THAT(navigation_manoeuvre.burn.specific_impulse_in_seconds_g0,
              AlmostEquals(30, 1));
  EXPECT_EQ(40, navigation_manoeuvre.inertial_direction.x);
  EXPECT_EQ(50, navigation_manoeuvre.inertial_direction.y);
  EXPECT_EQ(60, navigation_manoeuvre.inertial_direction.z);

  EXPECT_CALL(flight_plan, GetManœuvre(3))
      .WillOnce(ReturnRef(navigation_manœuvre));

  EXPECT_CALL(renderer, BarycentricToWorldSun(_))
      .WillOnce(Return(OrthogonalMap<Barycentric, WorldSun>::Identity()));
  EXPECT_CALL(navigation_manœuvre, FrenetFrame())
      .WillOnce(
          Return(OrthogonalMap<Frenet<Navigation>, Barycentric>::Identity()));
  EXPECT_CALL(*plugin_, CurrentTime())
      .WillRepeatedly(Return(Instant() - 4 * Second));
  EXPECT_CALL(renderer, GetPlottingFrame())
      .WillRepeatedly(Return(plotting_frame.get()));
  EXPECT_CALL(*plotting_frame, ToThisFrameAtTime(Instant()))
      .WillOnce(Return(barycentric_to_plotting));
  EXPECT_CALL(*plotting_frame, FromThisFrameAtTime(Instant() - 4 * Second))
      .WillOnce(Return(barycentric_to_plotting.Inverse()));
  principia__FlightPlanGetManoeuvreFrenetTrihedron(plugin_.get(),
                                                   vessel_guid,
                                                   3);

  EXPECT_CALL(flight_plan, number_of_segments())
      .WillOnce(Return(12));
  EXPECT_EQ(12, principia__FlightPlanNumberOfSegments(plugin_.get(),
                                                      vessel_guid));

  auto rendered_trajectory = make_not_null_unique<DiscreteTrajectory<World>>();
  rendered_trajectory->Append(
      t0_, DegreesOfFreedom<World>(World::origin, Velocity<World>()));
  rendered_trajectory->Append(
      t0_ + 1 * Second,
      DegreesOfFreedom<World>(
          World::origin +
              Displacement<World>({0 * Metre, 1 * Metre, 2 * Metre}),
          Velocity<World>()));
  rendered_trajectory->Append(
      t0_ + 2 * Second,
      DegreesOfFreedom<World>(
          World::origin +
              Displacement<World>({0 * Metre, 2 * Metre, 4 * Metre}),
          Velocity<World>()));
  auto segment = make_not_null_unique<DiscreteTrajectory<Barycentric>>();
  DegreesOfFreedom<Barycentric> immobile_origin{Barycentric::origin,
                                                Velocity<Barycentric>{}};
  segment->Append(t0_, immobile_origin);
  segment->Append(t0_ + 1 * Second, immobile_origin);
  segment->Append(t0_ + 2 * Second, immobile_origin);
  EXPECT_CALL(flight_plan, GetSegment(3, _, _))
      .WillOnce(DoAll(SetArgReferee<1>(segment->Begin()),
                      SetArgReferee<2>(segment->End())));
  EXPECT_CALL(renderer,
              FillRenderedBarycentricTrajectoryInWorld(_, _, _, _, _, _))
      .WillOnce(FillUniquePtr<5>(rendered_trajectory.release()));
  auto* const iterator =
      principia__FlightPlanRenderedSegment(plugin_.get(),
                                           vessel_guid,
                                           {0, 1, 2},
                                           3);
  EXPECT_EQ(XYZ({0, 0, 0}),
            principia__IteratorGetDiscreteTrajectoryXYZ(iterator));
  principia__IteratorIncrement(iterator);
  EXPECT_EQ(XYZ({0, 1, 2}),
            principia__IteratorGetDiscreteTrajectoryXYZ(iterator));
  principia__IteratorIncrement(iterator);
  EXPECT_EQ(XYZ({0, 2, 4}),
            principia__IteratorGetDiscreteTrajectoryXYZ(iterator));

  burn.thrust_in_kilonewtons = 10;
  EXPECT_CALL(*plugin_,
              FillBodyCentredNonRotatingNavigationFrame(celestial_index, _))
      .WillOnce(FillUniquePtr<1>(
                    new StrictMock<MockDynamicFrame<Barycentric, Navigation>>));
  EXPECT_CALL(flight_plan,
              ReplaceLastConstRef(
                  BurnMatches(10 * Kilo(Newton),
                              2 * Second * StandardGravity,
                              Instant() + 3 * Second,
                              Velocity<Frenet<Navigation>>(
                                  {4 * (Metre / Second),
                                   5 * (Metre / Second),
                                   6 * (Metre / Second)}))))
      .WillOnce(Return(true));
  EXPECT_TRUE(principia__FlightPlanReplaceLast(plugin_.get(),
                                               vessel_guid,
                                               burn));

  EXPECT_CALL(flight_plan, RemoveLast());
  principia__FlightPlanRemoveLast(plugin_.get(), vessel_guid);

  EXPECT_CALL(vessel, DeleteFlightPlan());
  principia__FlightPlanDelete(plugin_.get(), vessel_guid);
}

}  // namespace interface
}  // namespace principia
