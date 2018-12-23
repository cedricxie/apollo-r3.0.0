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

#include "modules/perception/traffic_light/onboard/hdmap_input.h"

#include "modules/map/hdmap/hdmap_util.h"
#include "modules/perception/common/perception_gflags.h"

namespace apollo {
namespace perception {
namespace traffic_light {

using apollo::hdmap::HDMapUtil;

// HDMapInput
HDMapInput::HDMapInput() {}

bool HDMapInput::GetSignals(const Eigen::Matrix4d &pointd,
                            std::vector<apollo::hdmap::Signal> *signals) {
  auto hdmap = HDMapUtil::BaseMapPtr();

  std::vector<hdmap::SignalInfoConstPtr> forward_signals;
  apollo::common::PointENU point;
  point.set_x(pointd(0, 3));
  point.set_y(pointd(1, 3));
  point.set_z(pointd(2, 3));
  int result = hdmap->GetForwardNearestSignalsOnLane(
      point, FLAGS_query_signal_range, &forward_signals);

  if (result != 0) {
    AERROR << "Failed to call HDMap::get_signal. point: "
           << point.ShortDebugString();
    return false;
  }

  signals->reserve(forward_signals.size());
  for (auto signal_info : forward_signals) {
    signals->push_back(signal_info->signal());
    ADEBUG << "Signal: " << signals->back().DebugString();
  }
  ADEBUG << "get_signal success. num_signals: " << signals->size()
         << " point: " << point.ShortDebugString();
  return true;
}
}  // namespace traffic_light
}  // namespace perception
}  // namespace apollo
