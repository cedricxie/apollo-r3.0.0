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

/**
 * @file offline_local_visualizer.h
 * @brief The offline tool for localization visualization.
 */
#ifndef MODULES_LOCALIZATION_MSF_LOCAL_TOOL_OFFLINE_LOCAL_VISUALIZER_H
#define MODULES_LOCALIZATION_MSF_LOCAL_TOOL_OFFLINE_LOCAL_VISUALIZER_H

#include <map>
#include <string>
#include <vector>

#include "modules/localization/msf/local_map/base_map/base_map_config.h"
#include "modules/localization/msf/local_tool/local_visualization/engine/visualization_engine.h"

namespace apollo {
namespace localization {
namespace msf {

/**
 * @class OfflineLocalVisualizer
 * @brief Offline localization visualization tool.
 */
class OfflineLocalVisualizer {
#define LOC_INFO_NUM 3

 public:
  OfflineLocalVisualizer();
  ~OfflineLocalVisualizer();

 public:
  bool Init(const std::string &map_folder, const std::string &map_visual_folder,
            const std::string &pcd_folder,
            const std::string &pcd_timestamp_file,
            const std::string &gnss_loc_file, const std::string &lidar_loc_file,
            const std::string &fusion_loc_file,
            const std::string &extrinsic_file);

  void Visualize();

 private:
  bool PCDTimestampFileHandler();
  bool LidarLocFileHandler(const std::vector<double> &pcd_timestamps);
  bool GnssLocFileHandler(const std::vector<double> &pcd_timestamps);
  bool FusionLocFileHandler(const std::vector<double> &pcd_timestamps);

  //   void PoseInterpolationByTime(
  //       const std::vector<Eigen::Affine3d> &in_poses,
  //       const std::vector<double> &in_timestamps,
  //       const std::vector<double> &ref_timestamps,
  //       std::map<unsigned int, Eigen::Affine3d> &out_poses);
 public:
  static void PoseAndStdInterpolationByTime(
      const std::vector<Eigen::Affine3d> &in_poses,
      const std::vector<Eigen::Vector3d> &in_stds,
      const std::vector<double> &in_timestamps,
      const std::vector<double> &ref_timestamps,
      std::map<unsigned int, Eigen::Affine3d> *out_poses,
      std::map<unsigned int, Eigen::Vector3d> *out_stds);

  bool GetZoneIdFromMapFolder(const std::string &map_folder,
                              const unsigned int resolution_id, int *zone_id);

 private:
  std::string map_folder_;
  std::string map_visual_folder_;
  std::string pcd_folder_;
  std::string pcd_timestamp_file_;
  std::string gnss_loc_file_;
  std::string lidar_loc_file_;
  std::string fusion_loc_file_;
  std::string extrinsic_file_;

  std::vector<double> pcd_timestamps_;
  std::map<unsigned int, Eigen::Affine3d> gnss_poses_;
  std::map<unsigned int, Eigen::Vector3d> gnss_stds_;
  std::map<unsigned int, Eigen::Affine3d> lidar_poses_;
  std::map<unsigned int, Eigen::Vector3d> lidar_stds_;
  std::map<unsigned int, Eigen::Affine3d> fusion_poses_;
  std::map<unsigned int, Eigen::Vector3d> fusion_stds_;

  BaseMapConfig map_config_;
  unsigned int resolution_id_;
  int zone_id_;

  Eigen::Affine3d velodyne_extrinsic_;
  VisualizationEngine visual_engine_;
};

}  // namespace msf
}  // namespace localization
}  // namespace apollo

#endif  // MODULES_LOCALIZATION_MSF_LOCAL_TOOL_OFFLINE_LOCAL_VISUALIZER_H
