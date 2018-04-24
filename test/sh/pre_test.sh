#!/bin/bash
# +-------------------------------------------------------------------------
# | Copyright (C) 2018 Yunify, Inc.
# +-------------------------------------------------------------------------
# | Licensed under the Apache License, Version 2.0 (the "License");
# | You may not use this work except in compliance with the License.
# | You may obtain a copy of the License in the LICENSE file, or at:
# |
# | http://www.apache.org/licenses/LICENSE-2.0
# |
# | Unless required by applicable law or agreed to in writing, software
# | distributed under the License is distributed on an "AS IS" BASIS,
# | WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# | See the License for the specific language governing permissions and
# | limitations under the License.
# +-------------------------------------------------------------------------
#
# things to do before the tests run
#
# 0. check if user is root
# 1. start qsfs
# 2. make run dir

current_path=$(dirname "$0")
source "$current_path/common.sh"

# 0. check root
if [[ $EUID -ne 0 ]]; then
  echo "Error: qsfs integration test must be run as root" 1>&2
  exit 1
fi

# 1. start qsfs
MOUNT_POINT=$(dirname "${QSFS_TEST_RUN_DIR}")
echo "mount to ${MOUNT_POINT}"
# subshell
(
  set -x
  qsfs ${QSFS_TEST_BUCKET} ${MOUNT_POINT} \
    -l=/tmp/qsfs_integration_test_log \
    -L=INFO \
    -C \
    -d \
    -o allow_other &
)
if [ -z "$(df | grep ${MOUNT_POINT})" ]; then
  echo "Error: unable to mount with command: qsfs ${QSFS_TEST_BUCKET} ${MOUNT_POINT} -o allow_other"
  exit 1
fi

# 2. make run dir
if [ ! -d "$QSFS_TEST_RUN_DIR" ]; then
  echo "make qsfs run dir [path=$QSFS_TEST_RUN_DIR]"
  mkdir $QSFS_TEST_RUN_DIR

  if [ ! -d ${QSFS_TEST_RUN_DIR} ]; then
    echo "Error: Could not create directory ${QSFS_TEST_RUN_DIR}"
    exit 1
  fi
fi
