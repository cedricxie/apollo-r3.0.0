#!/usr/bin/env bash

###############################################################################
# Copyright 2018 The Apollo Authors. All Rights Reserved.
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


DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

cd "${DIR}/.."

source "${DIR}/apollo_base.sh"

function start() {
    LOG_FILE="${APOLLO_ROOT_DIR}/data/log/voice_detector.out"
    nohup python modules/tools/voice_detection/snowboy_detector.py \
        > ${LOG_FILE} 2>&1 &
    echo "voice_detector: started"
}

function stop() {
    pkill -SIGKILL -f modules/tools/voice_detection/snowboy_detector.py
    echo "voice_detector: stopped"
}

case $1 in
  start)
    start
    ;;
  stop)
    stop
    ;;
  *)
    start
    ;;
esac
