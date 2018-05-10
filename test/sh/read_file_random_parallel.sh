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
#
# test case: random read file in parallel

set -o xtrace
set -o errexit

current_path=$(dirname "$0")
source "$current_path/utils.sh"

WORK_DIR_NAME="read_file_random_parallel"
mk_test_dir ${WORK_DIR_NAME}

FILE_NAME="${WORK_DIR_NAME}/random_read_file_parallel.txt"
MAX_NUM=20
THREADS=6

# creat a file at first
append_test_file $FILE_NAME $MAX_NUM

# random read and validate in parallel
(
  for i in $(seq 1 $THREADS); do
    rnum=$(shuf -n 1 "$QSFS_TEST_RUN_DIR/$FILE_NAME")
    if [ $rnum -lt 1 ] || [ $rnum -gt $MAX_NUM ]; then
      echo "Error: expected number belong to [1,$MAX_NUM], got $rnum"
      exit 1
    fi & true
  done
  wait
)

# clean up
rm_test_file $FILE_NAME
rm_test_dir ${WORK_DIR_NAME}