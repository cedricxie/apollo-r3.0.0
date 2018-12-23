/******************************************************************************
 * Copyright 2018 The Apollo Authors. All Rights Reserved.
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

#ifndef MODULES_DREAMVIEW_BACKEND_HMI_HMI_WORKER_H_
#define MODULES_DREAMVIEW_BACKEND_HMI_HMI_WORKER_H_

#include <string>
#include <vector>

#include "boost/thread/locks.hpp"
#include "boost/thread/shared_mutex.hpp"

#include "modules/canbus/proto/chassis.pb.h"
#include "modules/common/macro.h"
#include "modules/dreamview/proto/hmi_config.pb.h"
#include "modules/dreamview/proto/hmi_status.pb.h"
#include "modules/dreamview/proto/voice_detection.pb.h"
#include "modules/monitor/proto/system_status.pb.h"

/**
 * @namespace apollo::dreamview
 * @brief apollo::dreamview
 */
namespace apollo {
namespace dreamview {

using ChangeModeHandler = std::function<void(const std::string&)>;
using ChangeMapHandler = std::function<void(const std::string&)>;
using ChangeVehicleHandler = std::function<void(const std::string&)>;

// Singleton worker which does the actual work of HMI actions.
class HMIWorker {
 public:
  // High level HMI action trigger.
  bool Trigger(const HMIAction action);

  // Run a command on current system mode.
  void RunModeCommand(const std::string& command_name);
  // Run a command on given module.
  int RunModuleCommand(const std::string& module, const std::string& command);
  // Run a command on given hardware.
  int RunHardwareCommand(const std::string& hardware,
                         const std::string& command);
  // Run a command on given tool.
  int RunToolCommand(const std::string& tool, const std::string& command);

  // Update system status.
  void UpdateSystemStatus(const apollo::monitor::SystemStatus& system_status);

  // Register event handlers.
  void RegisterChangeModeHandler(ChangeModeHandler handler) {
    change_mode_handlers_.emplace_back(handler);
  }
  void RegisterChangeMapHandler(ChangeMapHandler handler) {
    change_map_handlers_.emplace_back(handler);
  }
  void RegisterChangeVehicleHandler(ChangeVehicleHandler handler) {
    change_vehicle_handlers_.emplace_back(handler);
  }
  // Change current mode, map, vehicle and driving mode.
  void ChangeToMode(const std::string& mode_name);
  void ChangeToMap(const std::string& map_name);
  void ChangeToVehicle(const std::string& vehicle_name);
  static bool ChangeToDrivingMode(
      const apollo::canbus::Chassis::DrivingMode mode);

  // Submit a DriveEvent.
  static void SubmitDriveEvent(const uint64_t event_time_ms,
                               const std::string& event_msg);

  // Get current config and status.
  inline const HMIConfig& GetConfig() const { return config_; }
  inline const HMIStatus& GetStatus() const { return status_; }
  // HMIStatus is updated frequently by multiple threads, including web workers
  // and ROS message callback. Please apply proper read/write lock when
  // accessing it.
  inline boost::shared_mutex& GetStatusMutex() { return status_mutex_; }

 private:
  HMIConfig config_;
  HMIStatus status_;
  mutable boost::shared_mutex status_mutex_;

  std::vector<ChangeModeHandler> change_mode_handlers_;
  std::vector<ChangeMapHandler> change_map_handlers_;
  std::vector<ChangeVehicleHandler> change_vehicle_handlers_;

  DECLARE_SINGLETON(HMIWorker);
};

}  // namespace dreamview
}  // namespace apollo

#endif  // MODULES_DREAMVIEW_BACKEND_HMI_HMI_WORKER_H_
