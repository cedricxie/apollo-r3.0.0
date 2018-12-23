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

#include "modules/perception/obstacle/lidar/tracker/hm_tracker/hm_tracker.h"

#include <map>
#include <numeric>

#include "modules/common/log.h"
#include "modules/common/util/file.h"
#include "modules/perception/common/geometry_util.h"
#include "modules/perception/common/perception_gflags.h"
#include "modules/perception/obstacle/lidar/tracker/hm_tracker/feature_descriptor.h"
#include "modules/perception/obstacle/lidar/tracker/hm_tracker/hungarian_matcher.h"
#include "modules/perception/obstacle/lidar/tracker/hm_tracker/kalman_filter.h"
#include "modules/perception/obstacle/lidar/tracker/hm_tracker/track_object_distance.h"

namespace apollo {
namespace perception {

bool HmObjectTracker::Init() {
  // Initialize tracker's configs
  using apollo::common::util::GetProtoFromFile;

  if (!GetProtoFromFile(FLAGS_tracker_config, &config_)) {
    AERROR << "Cannot get config proto from file: " << FLAGS_tracker_config;
    return false;
  }

  // A. Basic tracker setup
  // load match method
  if (config_.matcher_method() !=
      tracker_config::ModelConfigs::HUNGARIAN_MATCHER) {
    config_.set_matcher_method(tracker_config::ModelConfigs::HUNGARIAN_MATCHER);
    AWARN << "invalid matcher method! default HungarianMatcher in use!";
  }
  matcher_.reset(new HungarianMatcher());

  // load filter method
  // load track cached history size maximum
  if (!ObjectTrack::SetTrackCachedHistorySizeMaximum(
          config_.track_cached_history_size_maximum())) {
    AERROR << "Failed to set track cached history size maximum! " << name();
    return false;
  }
  // load track consevutive invisible maximum
  if (!ObjectTrackSet::SetTrackConsecutiveInvisibleMaximum(
          config_.track_consecutive_invisible_maximum())) {
    AERROR << "Failed to set track consecutive invisible maximum! " << name();
    return false;
  }
  // load track visible ratio minimum
  if (!ObjectTrackSet::SetTrackVisibleRatioMinimum(
          config_.track_visible_ratio_minimum())) {
    AERROR << "Failed to set track visible ratio minimum! " << name();
    return false;
  }
  // check collect age minimum
  if (config_.collect_age_minimum() < 0) {
    AERROR << "invalid collect age minimum of " << name();
    return false;
  }

  // check collect consecutive invisible maximum
  if (config_.collect_consecutive_invisible_maximum() < 0) {
    AERROR << "invalid collect consecutive invisible maximum of " << name();
    return false;
  }

  // load acceleration maximum
  if (!ObjectTrack::SetAccelerationNoiseMaximum(
          config_.acceleration_noise_maximum())) {
    AERROR << "Failed to set acceleration noise maximum! " << name();
    return false;
  }
  // load speed noise maximum
  if (!ObjectTrack::SetSpeedNoiseMaximum(config_.speed_noise_maximum())) {
    AERROR << "Failed to set speed noise maximum! " << name();
    return false;
  }

  // B. Matcher setup
  // load match distance maximum
  if (config_.matcher_method() ==
      tracker_config::ModelConfigs::HUNGARIAN_MATCHER) {
    if (!HungarianMatcher::SetMatchDistanceMaximum(
            config_.match_distance_maximum())) {
      AERROR << "Failed to set match distance maximum! " << name();
      return false;
    }
  }
  // load location distance weight
  if (!TrackObjectDistance::SetLocationDistanceWeight(
          config_.location_distance_weight())) {
    AERROR << "Failed to set location distance weight! " << name();
    return false;
  }
  // load direction distance weight
  if (!TrackObjectDistance::SetDirectionDistanceWeight(
          config_.direction_distance_weight())) {
    AERROR << "Failed to set direction distance weight! " << name();
    return false;
  }
  // load bbox size distance weight
  if (!TrackObjectDistance::SetBboxSizeDistanceWeight(
          config_.bbox_size_distance_weight())) {
    AERROR << "Failed to set bbox size distance weight! " << name();
    return false;
  }
  // load point num distance weight
  if (!TrackObjectDistance::SetPointNumDistanceWeight(
          config_.point_num_distance_weight())) {
    AERROR << "Failed to set point num distance weight! " << name();
    return false;
  }
  // load histogram distance weight
  if (!TrackObjectDistance::SetHistogramDistanceWeight(
          config_.histogram_distance_weight())) {
    AERROR << "Failed to set histogram distance weight! " << name();
    return false;
  }
  use_histogram_for_match_ =
      config_.histogram_distance_weight() > FLT_EPSILON ? true : false;
  if (config_.histogram_bin_size() <= 0) {
    AERROR << "invalid histogram bin size of " << name();
    return false;
  }

  // C. Filter setup
  if (config_.filter_method() == tracker_config::ModelConfigs::KALMAN_FILTER) {
    double association_score_maximum = config_.match_distance_maximum();
    KalmanFilter::SetUseAdaptive(config_.use_adaptive());
    if (!KalmanFilter::SetAssociationScoreMaximum(association_score_maximum)) {
      AERROR << "Failed to set association score maximum! " << name();
      return false;
    }
    if (!KalmanFilter::InitParams(
            config_.measurement_noise(), config_.initial_velocity_noise(),
            config_.xy_propagation_noise(), config_.z_propagation_noise())) {
      AERROR << "Failed to set params for kalman filter! " << name();
      return false;
    }
    if (!KalmanFilter::SetBreakdownThresholdMaximum(
            config_.breakdown_threshold_maximum())) {
      AERROR << "Failed to set breakdown threshold maximum! " << name();
      return false;
    }
  }
  return true;
}

const std::vector<ObjectTrackPtr>& HmObjectTracker::GetObjectTracks() const {
  return object_tracks_.GetTracks();
}

bool HmObjectTracker::Track(
    const std::vector<std::shared_ptr<Object>>& objects, double timestamp,
    const TrackerOptions& options,
    std::vector<std::shared_ptr<Object>>* tracked_objects) {
  // A. track setup
  if (tracked_objects == nullptr) return false;
  if (!valid_) {
    valid_ = true;
    return InitializeTrack(objects, timestamp, options, tracked_objects);
  }
  Eigen::Matrix4d velo2world_pose = Eigen::Matrix4d::Identity();
  if (options.velodyne_trans != nullptr) {
    velo2world_pose = *(options.velodyne_trans);
  } else {
    AERROR << "Input velodyne_trans is null";
    return false;
  }
  double time_diff = timestamp - time_stamp_;
  time_stamp_ = timestamp;

  // B. preprocessing
  // B.1 transform given pose to local one
  TransformPoseGlobal2Local(&velo2world_pose);
  ADEBUG << "velo2local_pose\n" << velo2world_pose;
  // B.2 construct objects for tracking
  std::vector<std::shared_ptr<TrackedObject>> transformed_objects;
  ConstructTrackedObjects(objects, &transformed_objects, velo2world_pose,
                          options);

  // C. prediction
  std::vector<Eigen::VectorXf> tracks_predict;
  ComputeTracksPredict(&tracks_predict, time_diff);

  // D. match objects to tracks
  std::vector<std::pair<int, int>> assignments;
  std::vector<int> unassigned_objects;
  std::vector<int> unassigned_tracks;
  std::vector<ObjectTrackPtr>& tracks = object_tracks_.GetTracks();
  if (matcher_ != nullptr) {
    matcher_->Match(&transformed_objects, tracks, tracks_predict, &assignments,
                    &unassigned_tracks, &unassigned_objects);
  } else {
    AERROR << "matcher_ is not initiated. Please call Init() function before "
              "other functions.";
    return false;
  }
  ADEBUG << "multi-object-tracking: " << tracks.size() << "  "
         << assignments.size() << "  " << transformed_objects.size() << "  "
         << unassigned_objects.size() << "  " << time_diff;

  // E. update tracks
  // E.1 update tracks with associated objects
  UpdateAssignedTracks(&tracks_predict, &transformed_objects, assignments,
                       time_diff);
  // E.2 update tracks without associated objects
  UpdateUnassignedTracks(tracks_predict, unassigned_tracks, time_diff);
  DeleteLostTracks();
  // E.3 create new tracks for objects without associated tracks
  CreateNewTracks(transformed_objects, unassigned_objects);

  // F. collect tracked results
  CollectTrackedResults(tracked_objects);
  return true;
}

bool HmObjectTracker::InitializeTrack(
    const std::vector<std::shared_ptr<Object>>& objects,
    const double timestamp, const TrackerOptions& options,
    std::vector<std::shared_ptr<Object>>* tracked_objects) {
  // A. track setup
  Eigen::Matrix4d velo2world_pose = Eigen::Matrix4d::Identity();
  if (options.velodyne_trans != nullptr) {
    velo2world_pose = *(options.velodyne_trans);
  } else {
    AERROR << "Input velodyne_trans is null";
    return false;
  }
  global_to_local_offset_ = Eigen::Vector3d(
      -velo2world_pose(0, 3), -velo2world_pose(1, 3), -velo2world_pose(2, 3));

  // B. preprocessing
  // B.1 coordinate transformation
  TransformPoseGlobal2Local(&velo2world_pose);
  ADEBUG << "velo2local_pose\n" << velo2world_pose;
  // B.2 construct tracked objects
  std::vector<std::shared_ptr<TrackedObject>> transformed_objects;
  ConstructTrackedObjects(objects, &transformed_objects, velo2world_pose,
                          options);

  // C. create tracks
  std::vector<int> unassigned_objects;
  unassigned_objects.resize(transformed_objects.size());
  std::iota(unassigned_objects.begin(), unassigned_objects.end(), 0);
  CreateNewTracks(transformed_objects, unassigned_objects);
  time_stamp_ = timestamp;

  // D. collect tracked results
  CollectTrackedResults(tracked_objects);
  return true;
}

void HmObjectTracker::TransformPoseGlobal2Local(Eigen::Matrix4d* pose) {
  (*pose)(0, 3) += global_to_local_offset_(0);
  (*pose)(1, 3) += global_to_local_offset_(1);
  (*pose)(2, 3) += global_to_local_offset_(2);
}

void HmObjectTracker::ConstructTrackedObjects(
    const std::vector<std::shared_ptr<Object>>& objects,
    std::vector<std::shared_ptr<TrackedObject>>* tracked_objects,
    const Eigen::Matrix4d& pose, const TrackerOptions& options) {
  int num_objects = objects.size();
  tracked_objects->clear();
  tracked_objects->resize(num_objects);
  for (int i = 0; i < num_objects; ++i) {
    std::shared_ptr<Object> obj(new Object());
    obj->clone(*objects[i]);
    (*tracked_objects)[i].reset(new TrackedObject(obj));
    // Computing shape featrue
    if (use_histogram_for_match_) {
      ComputeShapeFeatures(&((*tracked_objects)[i]));
    }
    // Transforming all tracked objects
    TransformTrackedObject(&((*tracked_objects)[i]), pose);
    // Setting barycenter as anchor point of tracked objects
    Eigen::Vector3f anchor_point = (*tracked_objects)[i]->barycenter;
    (*tracked_objects)[i]->anchor_point = anchor_point;
    // Getting lane direction of tracked objects
    pcl_util::PointD query_pt;
    query_pt.x = anchor_point(0) - global_to_local_offset_(0);
    query_pt.y = anchor_point(1) - global_to_local_offset_(1);
    query_pt.z = anchor_point(2) - global_to_local_offset_(2);
    Eigen::Vector3d lane_dir;
    if (!options.hdmap_input->GetNearestLaneDirection(query_pt, &lane_dir)) {
      AERROR << "Failed to initialize the lane direction of tracked object!";
      // Set lane dir as host dir if query lane direction failed
      lane_dir = (pose * Eigen::Vector4d(1, 0, 0, 0)).head(3);
    }
    (*tracked_objects)[i]->lane_direction = lane_dir.cast<float>();
  }
}

void HmObjectTracker::ComputeShapeFeatures(
    std::shared_ptr<TrackedObject>* obj) {
  // Compute object's shape feature
  std::shared_ptr<Object>& temp_object = (*obj)->object_ptr;
  FeatureDescriptor fd(temp_object->cloud);
  fd.ComputeHistogram(config_.histogram_bin_size(),
                      &temp_object->shape_features);
}

void HmObjectTracker::TransformTrackedObject(
    std::shared_ptr<TrackedObject>* obj, const Eigen::Matrix4d& pose) {
  // Transform tracked object with given pose
  TransformObject(&((*obj)->object_ptr), pose);
  // transform direction
  Eigen::Vector3f& dir = (*obj)->direction;
  dir =
      (pose * Eigen::Vector4d(dir(0), dir(1), dir(2), 0)).head(3).cast<float>();
  // transform center
  Eigen::Vector3f& center = (*obj)->center;
  center = (pose * Eigen::Vector4d(center(0), center(1), center(2), 1))
               .head(3)
               .cast<float>();
  // transform barycenter
  Eigen::Vector3f& barycenter = (*obj)->barycenter;
  barycenter =
      (pose * Eigen::Vector4d(barycenter(0), barycenter(1), barycenter(2), 1))
          .head(3)
          .cast<float>();
}

void HmObjectTracker::TransformObject(std::shared_ptr<Object>* obj,
                                      const Eigen::Matrix4d& pose) {
  // Transform object with given pose
  Eigen::Vector3d& dir = (*obj)->direction;
  dir = (pose * Eigen::Vector4d(dir[0], dir[1], dir[2], 0)).head(3);
  // transform center
  Eigen::Vector3d& center = (*obj)->center;
  center = (pose * Eigen::Vector4d(center[0], center[1], center[2], 1)).head(3);
  // transform cloud & polygon
  TransformPointCloud<pcl_util::Point>(pose, (*obj)->cloud);
  TransformPointCloud<pcl_util::PointD>(pose, &((*obj)->polygon));
}

void HmObjectTracker::ComputeTracksPredict(
    std::vector<Eigen::VectorXf>* tracks_predict, const double time_diff) {
  // Compute tracks' predicted states
  int no_track = object_tracks_.Size();
  tracks_predict->resize(no_track);
  std::vector<ObjectTrackPtr>& tracks = object_tracks_.GetTracks();
  for (int i = 0; i < no_track; ++i) {
    (*tracks_predict)[i] = tracks[i]->Predict(time_diff);
  }
}

void HmObjectTracker::UpdateAssignedTracks(
    std::vector<Eigen::VectorXf>* tracks_predict,
    std::vector<std::shared_ptr<TrackedObject>>* new_objects,
    const std::vector<std::pair<int, int>>& assignments,
    const double time_diff) {
  // Update assigned tracks
  std::vector<ObjectTrackPtr>& tracks = object_tracks_.GetTracks();
  for (size_t i = 0; i < assignments.size(); ++i) {
    int track_id = assignments[i].first;
    int obj_id = assignments[i].second;
    tracks[track_id]->UpdateWithObject(&(*new_objects)[obj_id], time_diff);
  }
}

void HmObjectTracker::UpdateUnassignedTracks(
    const std::vector<Eigen::VectorXf>& tracks_predict,
    const std::vector<int>& unassigned_tracks, const double time_diff) {
  // Update tracks without matched objects
  std::vector<ObjectTrackPtr>& tracks = object_tracks_.GetTracks();
  for (size_t i = 0; i < unassigned_tracks.size(); ++i) {
    int track_id = unassigned_tracks[i];
    tracks[track_id]->UpdateWithoutObject(tracks_predict[track_id], time_diff);
  }
}

void HmObjectTracker::CreateNewTracks(
    const std::vector<std::shared_ptr<TrackedObject>>& new_objects,
    const std::vector<int>& unassigned_objects) {
  // Create new tracks for objects without matched tracks
  for (size_t i = 0; i < unassigned_objects.size(); ++i) {
    int obj_id = unassigned_objects[i];
    ObjectTrackPtr track(new ObjectTrack(new_objects[obj_id]));
    object_tracks_.AddTrack(track);
  }
}

void HmObjectTracker::DeleteLostTracks() {
  // Delete lost tracks
  object_tracks_.RemoveLostTracks();
}

void HmObjectTracker::CollectTrackedResults(
    std::vector<std::shared_ptr<Object>>* tracked_objects) {
  // Collect tracked results for reporting include objects may be occluded
  // temporaryly
  const std::vector<ObjectTrackPtr>& tracks = object_tracks_.GetTracks();
  tracked_objects->resize(tracks.size());

  int track_number = 0;
  for (size_t i = 0; i < tracks.size(); ++i) {
    if (tracks[i]->consecutive_invisible_count_ >
        config_.collect_consecutive_invisible_maximum())
      continue;
    if (tracks[i]->age_ < config_.collect_age_minimum()) {
      continue;
    }
    std::shared_ptr<Object> obj(new Object);
    std::shared_ptr<TrackedObject> result_obj = tracks[i]->current_object_;
    obj->clone(*(result_obj->object_ptr));
    // fill tracked information of object
    obj->direction = result_obj->direction.cast<double>();
    if (fabs(obj->direction[0]) < DBL_MIN) {
      obj->theta = obj->direction(1) > 0 ? M_PI / 2 : -M_PI / 2;
    } else {
      obj->theta = atan2(obj->direction[1], obj->direction[0]);
    }
    obj->length = result_obj->size[0];
    obj->width = result_obj->size[1];
    obj->height = result_obj->size[2];
    obj->velocity = result_obj->velocity.cast<double>();
    obj->velocity_uncertainty = result_obj->velocity_uncertainty.cast<double>();
    obj->track_id = tracks[i]->idx_;
    obj->tracking_time = tracks[i]->period_;
    obj->type = result_obj->type;
    obj->center = result_obj->center.cast<double>() - global_to_local_offset_;
    obj->anchor_point =
        result_obj->anchor_point.cast<double>() - global_to_local_offset_;
    // restore original world coordinates
    for (size_t j = 0; j < obj->cloud->size(); ++j) {
      obj->cloud->points[j].x -= global_to_local_offset_[0];
      obj->cloud->points[j].y -= global_to_local_offset_[1];
      obj->cloud->points[j].z -= global_to_local_offset_[2];
    }
    for (size_t j = 0; j < obj->polygon.size(); ++j) {
      obj->polygon.points[j].x -= global_to_local_offset_[0];
      obj->polygon.points[j].y -= global_to_local_offset_[1];
      obj->polygon.points[j].z -= global_to_local_offset_[2];
    }
    (*tracked_objects)[track_number] = obj;
    ++track_number;
  }
  tracked_objects->resize(track_number);
}

}  // namespace perception
}  // namespace apollo
