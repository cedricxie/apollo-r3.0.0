#!/usr/bin/env python

###############################################################################
# Copyright 2017 The Apollo Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
###############################################################################

import sys
import rosbag
from modules.localization.proto import localization_pb2

if len(sys.argv) < 3:
    print("Usage: %s <filename> <fbags>" % sys.argv[0])
    sys.exit(1)

filename = sys.argv[1]
fbags = sys.argv[2:]

with open(filename, 'w') as f:
    for fbag in fbags:
        bag = rosbag.Bag(fbag)
        for topic, localization_pb, t in bag.read_messages(
                topics=['/apollo/localization/pose']):
            x = localization_pb.pose.position.x
            y = localization_pb.pose.position.y
            f.write(str(x) + "," + str(y) + "\n")
print("File written to: %s" % filename)
