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
"""
This program can dump a rosbag into separate text files that contains the pb messages
"""

import argparse
import os
import shutil

import rosbag
import std_msgs
from std_msgs.msg import String

g_args = None

g_delta_t = 0.5  # 1 second approximate time match region.

def write_to_file(file_path, topic_pb):
    """write pb message to file"""
    f = file(file_path, 'w')
    f.write(str(topic_pb))
    f.close()


def dump_bag(in_bag, out_dir):
    """out_bag = in_bag + routing_bag"""
    bag = rosbag.Bag(in_bag, 'r')
    seq = 0
    global g_args
    topic_name_map = {
        "/apollo/localization/pose" : ["localization", None],
        "/apollo/canbus/chassis" : ["chassis", None],
        "/apollo/routing_response" : ["routing", None],
        "/apollo/routing_resquest" : ["routing_request", None],
        "/apollo/perception/obstacles" : ["perception", None],
        "/apollo/prediction" : ["prediction", None],
        "/apollo/planning" : ["planning", None],
        "/apollo/control" : ["control", None]
    }
    first_time = None
    record_num = 0
    for topic, msg, t in bag.read_messages():
        record_num += 1
        if record_num % 1000 == 0:
            print "Processing record_num:", record_num
        if first_time is None:
            first_time = t
        if topic not in topic_name_map:
            continue
        relative_time = (t - first_time).secs - g_args.start_time
        if ((g_args.time_duration > 0) and
            (relative_time < 0 or relative_time > g_args.time_duration)):
            continue
        if topic == "/apollo/planning":
            seq += 1
            topic_name_map[topic][1] = msg
            print "Generating seq:", seq
            for t, name_pb in topic_name_map.iteritems():
                if name_pb[1] is None:
                    continue
                file_path = os.path.join(out_dir,
                                        str(seq) + "_" + name_pb[0] + ".pb.txt")
                write_to_file(file_path, name_pb[1])
        topic_name_map[topic][1] = msg


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description=
        "A tool to dump the protobuf messages according to the planning message ")
    parser.add_argument(
        "in_rosbag", action="store", type=str, help="the input ros bag")
    parser.add_argument(
        "out_dir",
        action="store",
        help="the output directory for the dumped file")
    parser.add_argument(
        "--start_time",
        type=float,
        action="store",
        default=0.0,
        help="""The time range to extract in second""")
    parser.add_argument(
        "--time_duration",
        type=float,
        action="store",
        default=-1,
        help="""time duration to extract in second, negative to extract all""")

    g_args = parser.parse_args()

    if os.path.exists(g_args.out_dir):
        shutil.rmtree(g_args.out_dir)
    os.makedirs(g_args.out_dir)
    dump_bag(g_args.in_rosbag, g_args.out_dir)
