/******************************************************************************
 * Copyright 2017 The Apollo Authors. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *****************************************************************************/

#include "modules/control/controller/lat_controller.h"

#include <memory>
#include <string>
#include <utility>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "modules/common/log.h"
#include "modules/common/time/time.h"
#include "modules/common/util/file.h"
#include "modules/common/vehicle_state/vehicle_state_provider.h"
#include "modules/control/common/control_gflags.h"
#include "modules/control/proto/control_conf.pb.h"
#include "modules/localization/common/localization_gflags.h"
#include "modules/planning/proto/planning.pb.h"

namespace apollo {
namespace control {

using apollo::common::time::Clock;
using PlanningTrajectoryPb = planning::ADCTrajectory;
using LocalizationPb = localization::LocalizationEstimate;
using ChassisPb = canbus::Chassis;
using apollo::common::VehicleStateProvider;

class LatControllerTest : public ::testing::Test, LatController {
 public:
  virtual void SetUp() {
    FLAGS_v = 3;
    std::string control_conf_file =
        "modules/control/testdata/conf/lincoln.pb.txt";
    ControlConf control_conf;
    CHECK(apollo::common::util::GetProtoFromFile(control_conf_file,
                                                 &control_conf));
    lateral_conf_ = control_conf.lat_controller_conf();

    timestamp_ = Clock::NowInSeconds();
  }

  void ComputeLateralErrors(const double x, const double y, const double theta,
                            const double linear_v, const double angular_v,
                            const TrajectoryAnalyzer &trajectory_analyzer,
                            SimpleLateralDebug *debug) {
    LatController::ComputeLateralErrors(x, y, theta, linear_v, angular_v,
                                        trajectory_analyzer, debug);
  }

 protected:
  LocalizationPb LoadLocalizaionPb(const std::string &filename) {
    LocalizationPb localization_pb;
    CHECK(apollo::common::util::GetProtoFromFile(filename, &localization_pb))
        << "Failed to open file " << filename;
    localization_pb.mutable_header()->set_timestamp_sec(timestamp_);
    return localization_pb;
  }

  ChassisPb LoadChassisPb(const std::string &filename) {
    ChassisPb chassis_pb;
    CHECK(apollo::common::util::GetProtoFromFile(filename, &chassis_pb))
        << "Failed to open file " << filename;
    chassis_pb.mutable_header()->set_timestamp_sec(timestamp_);
    return chassis_pb;
  }

  PlanningTrajectoryPb LoadPlanningTrajectoryPb(const std::string &filename) {
    PlanningTrajectoryPb planning_trajectory_pb;
    CHECK(apollo::common::util::GetProtoFromFile(filename,
                                                 &planning_trajectory_pb))
        << "Failed to open file " << filename;
    planning_trajectory_pb.mutable_header()->set_timestamp_sec(timestamp_);
    return planning_trajectory_pb;
  }

  LatControllerConf lateral_conf_;

  double timestamp_ = 0.0;
};

TEST_F(LatControllerTest, ComputeLateralErrors) {
  auto localization_pb = LoadLocalizaionPb(
      "modules/control/testdata/lateral_controller_test/"
      "1_localization.pb.txt");
  auto chassis_pb = LoadChassisPb(
      "modules/control/testdata/lateral_controller_test/1_chassis.pb.txt");
  FLAGS_enable_map_reference_unify = false;
  auto *vehicle_state = VehicleStateProvider::instance();
  vehicle_state->Update(localization_pb, chassis_pb);

  auto planning_trajectory_pb = LoadPlanningTrajectoryPb(
      "modules/control/testdata/lateral_controller_test/"
      "1_planning.pb.txt");
  TrajectoryAnalyzer trajectory_analyzer(&planning_trajectory_pb);

  ControlCommand cmd;
  SimpleLateralDebug *debug = cmd.mutable_debug()->mutable_simple_lat_debug();

  ComputeLateralErrors(
      vehicle_state->x(), vehicle_state->y(), vehicle_state->heading(),
      vehicle_state->linear_velocity(), vehicle_state->angular_velocity(),
      trajectory_analyzer, debug);

  double theta_error_expected = -0.03549;
  double theta_error_dot_expected = 0.0044552856731;
  double d_error_expected = 1.30917375441;
  double d_error_dot_expected = 0.0;
  double matched_theta_expected = -1.81266;
  double matched_kappa_expected = -0.00237307;

  EXPECT_NEAR(debug->heading_error(), theta_error_expected, 0.001);
  EXPECT_NEAR(debug->heading_error_rate(), theta_error_dot_expected, 0.001);
  EXPECT_NEAR(debug->lateral_error(), d_error_expected, 0.001);
  EXPECT_NEAR(debug->lateral_error_rate(), d_error_dot_expected, 0.001);
  EXPECT_NEAR(debug->ref_heading(), matched_theta_expected, 0.001);
  EXPECT_NEAR(debug->curvature(), matched_kappa_expected, 0.001);
}

}  // namespace control
}  // namespace apollo
