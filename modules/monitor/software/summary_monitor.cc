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

#include "modules/monitor/software/summary_monitor.h"

#include "modules/common/adapters/adapter_manager.h"
#include "modules/common/log.h"
#include "modules/common/util/string_util.h"
#include "modules/monitor/common/monitor_manager.h"

DEFINE_string(summary_monitor_name, "SummaryMonitor",
              "Name of the summary monitor.");

DEFINE_double(broadcast_max_interval, 8,
              "Max interval of broadcasting runtime status.");

DEFINE_bool(enable_safety_mode, true,
            "Whether to enable safety mode which may take over the vehicle on "
            "system failures.");

namespace apollo {
namespace monitor {
namespace {

using apollo::common::adapter::AdapterBase;
using apollo::common::adapter::AdapterConfig;
using apollo::common::adapter::AdapterManager;
using apollo::common::util::StrCat;
using apollo::common::util::StringPrintf;

// Status has a *summary* field which is apollo::monitor::Summary.
template <class Status>
void UpdateStatusSummary(const Summary new_summary, const std::string &new_msg,
                         Status *status) {
  // Overwrite priority: FATAL > ERROR > WARN > OK > UNKNOWN.
  if (new_summary > status->summary()) {
    status->set_summary(new_summary);
    if (!new_msg.empty()) {
      status->set_msg(new_msg);
    } else {
      status->clear_msg();
    }
  }
}

template <class Status>
void SummarizeOnTopicStatus(const TopicStatus &topic_status, Status *status) {
  if (!topic_status.has_message_delay()) {
    UpdateStatusSummary(Summary::OK, "", status);
    return;
  }

  if (topic_status.message_delay() < 0) {
    UpdateStatusSummary(Summary::ERROR, "No message", status);
  } else {
    UpdateStatusSummary(Summary::ERROR, "Notable delay", status);
  }
}

}  // namespace

// Set interval to 0, so it runs every time when ticking.
SummaryMonitor::SummaryMonitor()
    : RecurrentRunner(FLAGS_summary_monitor_name, 0) {
  CHECK(AdapterManager::GetSystemStatus())
      << "SystemStatusAdapter is not initialized.";
  if (FLAGS_enable_safety_mode) {
    safety_manager_.reset(new SafetyManager());
  }
}

void SummaryMonitor::RunOnce(const double current_time) {
  SummarizeModules();
  SummarizeHardware();
  if (safety_manager_ != nullptr) {
    safety_manager_->CheckSafety(current_time);
  }
  // Get fingerprint of current status.
  // Don't use DebugString() which has known bug on Map field. The string
  // doesn't change though the value has changed.
  static std::hash<std::string> hash_fn;
  std::string proto_bytes;
  auto *system_status = MonitorManager::GetStatus();
  system_status->clear_header();
  system_status->SerializeToString(&proto_bytes);
  const size_t new_fp = hash_fn(proto_bytes);

  if (system_status_fp_ != new_fp ||
      current_time - last_broadcast_ > FLAGS_broadcast_max_interval) {
    AdapterManager::FillSystemStatusHeader("SystemMonitor", system_status);
    AdapterManager::PublishSystemStatus(*system_status);
    ADEBUG << "Published system status: " << system_status->DebugString();
    system_status_fp_ = new_fp;
    last_broadcast_ = current_time;
  }

  // Print and publish all monitor logs.
  MonitorManager::LogBuffer().PrintLog();
  MonitorManager::LogBuffer().Publish();
}

void SummaryMonitor::SummarizeModules() {
  for (auto &module : *MonitorManager::GetStatus()->mutable_modules()) {
    ModuleStatus *status = &(module.second);

    if (status->has_process_status()) {
      if (status->process_status().running()) {
        UpdateStatusSummary(Summary::OK, "", status);
      } else {
        UpdateStatusSummary(Summary::FATAL, "No process", status);
        continue;
      }
    }

    if (status->has_topic_status()) {
      SummarizeOnTopicStatus(status->topic_status(), status);
    }
  }
}

void SummaryMonitor::SummarizeHardware() {
  for (auto &hardware : *MonitorManager::GetStatus()->mutable_hardware()) {
    HardwareStatus *status = &(hardware.second);

    // If we don't have the status, keeps as UNKNOWN.
    if (status->has_status()) {
      switch (status->status()) {
        case HardwareStatus::NOT_PRESENT:
          UpdateStatusSummary(Summary::FATAL, status->detailed_msg(), status);
          break;
        case HardwareStatus::NOT_READY:  // Fall through.
        case HardwareStatus::WARN:  // Fall through.
        case HardwareStatus::GPS_UNSTABLE_WARNING:  // Fall through.
        case HardwareStatus::GPS_UNSTABLE_ERROR:
          // GPS instability could be a fatal error if it's the only
          // localization source. As currently we have other sources like
          // PointCloud, we take it as a warning.
          //
          // TODO(xiaoxq & Localization team): Add stability metric in final
          // localization pose and trigger WARN or ERROR accordingly.
          UpdateStatusSummary(Summary::WARN, status->detailed_msg(), status);
          break;
        case HardwareStatus::OK:
          UpdateStatusSummary(Summary::OK, status->detailed_msg(), status);
          break;
        default:
          UpdateStatusSummary(Summary::ERROR, status->detailed_msg(), status);
          break;
      }
    }

    if (status->has_topic_status()) {
      SummarizeOnTopicStatus(status->topic_status(), status);
    }
  }
}

}  // namespace monitor
}  // namespace apollo
