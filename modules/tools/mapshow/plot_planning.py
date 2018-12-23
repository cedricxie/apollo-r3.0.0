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

import rospy
import argparse
import matplotlib.pyplot as plt
import matplotlib.animation as animation

from modules.planning.proto import planning_pb2
from subplot_st_main import StMainSubplot
from subplot_path import PathSubplot
from subplot_sl_main import SlMainSubplot
from subplot_st_speed import StSpeedSubplot
from subplot_speed import SpeedSubplot
from localization import Localization
from planning import Planning

planning = Planning()
localization = Localization()


def update(frame_number):
    #st_main_subplot.show(planning)
    #st_speed_subplot.show(planning)
    map_path_subplot.show(planning, localization)
    dp_st_main_subplot.show(planning)
    qp_st_main_subplot.show(planning)
    speed_subplot.show(planning)
    sl_main_subplot.show(planning)
    st_speed_subplot.show(planning)


def planning_callback(planning_pb):
    planning.update_planning_pb(planning_pb)
    localization.update_localization_pb(
        planning_pb.debug.planning_data.adc_position)

    planning.compute_st_data()
    planning.compute_sl_data()
    planning.compute_path_data()
    planning.compute_speed_data()
    planning.compute_init_point()


def add_listener():
    rospy.init_node('st_plot', anonymous=True)
    rospy.Subscriber('/apollo/planning', planning_pb2.ADCTrajectory,
                     planning_callback)


def press_key(event):
    if event.key == '+' or event.key == '=':
        map_path_subplot.zoom_in()
    if event.key == '-' or event.key == '_':
        map_path_subplot.zoom_out()


if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        description="plot_planning is a tool to display "
        "planning trajs on a map.",
        prog="plot_planning_old.py")
    parser.add_argument(
        "-m",
        "--map",
        action="store",
        type=str,
        required=False,
        default=None,
        help="Specify the map file in txt or binary format")
    args = parser.parse_args()

    add_listener()
    fig = plt.figure()
    fig.canvas.mpl_connect('key_press_event', press_key)

    ax = plt.subplot2grid((3, 3), (0, 0), rowspan=2, colspan=2)
    map_path_subplot = PathSubplot(ax, args.map)

    ax1 = plt.subplot2grid((3, 3), (0, 2))
    speed_subplot = SpeedSubplot(ax1)

    ax2 = plt.subplot2grid((3, 3), (2, 2))
    dp_st_main_subplot = StMainSubplot(ax2, 'QpSplineStSpeedOptimizer')

    ax3 = plt.subplot2grid((3, 3), (1, 2))
    qp_st_main_subplot = StMainSubplot(ax3, 'DpStSpeedOptimizer')

    ax4 = plt.subplot2grid((3, 3), (2, 0), colspan=1)
    sl_main_subplot = SlMainSubplot(ax4)

    ax5 = plt.subplot2grid((3, 3), (2, 1), colspan=1)
    st_speed_subplot = StSpeedSubplot(ax5, 'QpSplineStSpeedOptimizer')

    ani = animation.FuncAnimation(fig, update, interval=100)

    ax.axis('equal')
    plt.show()
