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
 * @file: can_checker_factory.cc
 */

#include "modules/monitor/hardware/can/can_checker_factory.h"

#if USE_ESD_CAN
#include "modules/monitor/hardware/can/esdcan/esdcan_checker.h"
#endif
#include "modules/monitor/hardware/can/socketcan/socketcan_checker.h"

namespace apollo {
namespace monitor {

using apollo::drivers::canbus::CANCardParameter;

CanCheckerFactory::CanCheckerFactory() {}

void CanCheckerFactory::RegisterCanCheckers() {
#if USE_ESD_CAN
  Register(CANCardParameter::ESD_CAN,
           []() -> HwCheckerInterface* { return new hw::EsdCanChecker(); });
#endif
  Register(CANCardParameter::SOCKET_CAN_RAW,
           []() -> HwCheckerInterface* { return new hw::SocketCanChecker(); });
}

std::unique_ptr<HwCheckerInterface> CanCheckerFactory::CreateCanChecker(
    const CANCardParameter& parameter) {
  auto factory = CreateObjectOrNull(parameter.brand());
  if (!factory) {
    AERROR_EVERY(10) << "Failed to create CAN checker with parameter: "
                     << parameter.DebugString();
  }
  return factory;
}

}  // namespace monitor
}  // namespace apollo
