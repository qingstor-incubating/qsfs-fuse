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
# things to do after the tests run
#
# 1. stop qsfs
# 2. remove run dir

current_path=$(dirname "$0")
source "$current_path/common.sh"

# 1. stop qsfs
MOUNT_POINT=$(dirname "${QSFS_TEST_RUN_DIR}")
fusermount -u ${MOUNT_POINT}
if [ -n "$(df | grep ${MOUNT_POINT})" ]; then
  echo "Error: fail to unmount with command: fusermount -u ${MOUNT_POINT}"
  exit 1
fi

# 2. clean run dir
echo "remove qsfs run dir [path=$QSFS_TEST_RUN_DIR]"
rm -rf $QSFS_TEST_RUN_DIR
