/******************************************************************************
 * Modification Copyright 2018 The Apollo Authors. All Rights Reserved.
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

#ifndef MODULES_DRIVERS_LIDAR_VELODYNE_COMMON_SOCKET_INPUT_H_
#define MODULES_DRIVERS_LIDAR_VELODYNE_COMMON_SOCKET_INPUT_H_

#include <stdio.h>
#include <unistd.h>

#include "ros/ros.h"

#include "modules/drivers/lidar_velodyne/common/input.h"

namespace apollo {
namespace drivers {
namespace lidar_velodyne {

static const int POLL_TIMEOUT = 1000;  // one second (in msec)

/** @brief Live Velodyne input from socket. */
class SocketInput : public Input {
 public:
  SocketInput();
  virtual ~SocketInput();
  bool init(int port) override;
  int get_firing_data_packet(velodyne_msgs::VelodynePacket *pkt) override;
  int get_positioning_data_packtet(const NMEATimePtr &nmea_time) override;

 private:
  int sockfd_;
  int port_;
  bool input_available(int timeout);
};

}  // namespace lidar_velodyne
}  // namespace drivers
}  // namespace apollo

#endif  // MODULES_DRIVERS_LIDAR_VELODYNE_COMMON_SOCKET_INPUT_H_
