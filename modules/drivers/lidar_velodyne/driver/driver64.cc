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

#include <time.h>
#include <cmath>
#include <string>

#include "ros/ros.h"

#include "modules/common/log.h"
#include "modules/drivers/lidar_velodyne/driver/driver.h"

namespace apollo {
namespace drivers {
namespace lidar_velodyne {

Velodyne64Driver::Velodyne64Driver(const VelodyneConf& conf) { config_ = conf; }

bool Velodyne64Driver::init() {
  double packet_rate = 0;  // packet frequency (Hz)
  if (config_.model() == V64E_S2 || config_.model() == V64E_S3S) {
    packet_rate = 3472.17;  // 1333312 / 384
  } else {                  // 64E_S3D etc.
    packet_rate = 5789;
  }
  double frequency = config_.rpm() / 60.0;  // expected Hz rate

  // default number of packets for each scan is a single revolution
  // (fractions rounded up)
  config_.set_npackets(static_cast<int>(ceil(packet_rate / frequency)));
  AINFO << "publishing " << config_.npackets() << " packets per scan";

  input_.reset(new SocketInput());
  if (!input_->init(config_.firing_data_port())) {
    AERROR << "init data input socket fail.";
    return false;
  }
  return true;
}

/** poll the device
 *
 *  @returns true unless end of file reached
 */
bool Velodyne64Driver::poll(velodyne_msgs::VelodyneScanUnifiedPtr scan) {
  int poll_result = poll_standard(scan);

  if (poll_result == SOCKET_TIMEOUT || poll_result == RECIEVE_FAIL) {
    return true;  // poll again
  }

  if (scan->packets.empty()) {
    AINFO << "Get a empty scan from port: " << config_.firing_data_port();
    return true;
  }

  // publish message using time of last packet read
  ADEBUG << "Publishing a full Velodyne scan.";
  scan->header.stamp = ros::Time().now();
  scan->header.frame_id = config_.frame_id();
  scan->basetime = basetime_;

  return true;
}

}  // namespace lidar_velodyne
}  // namespace drivers
}  // namespace apollo
