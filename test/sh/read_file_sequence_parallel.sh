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
# test case: sequence read in parallel

set -o xtrace
set -o errexit

current_path=$(dirname "$0")
source "$current_path/utils.sh"

WORK_DIR_NAME="read_file_sequence_parallel"
mk_test_dir ${WORK_DIR_NAME}

FILE_NAME="${WORK_DIR_NAME}/sequence_read_file_parallel.txt"
FILE_SIZE=15

# create a file at first
# append numbers (from 1 to file_size) to a file
append_test_file $FILE_NAME $FILE_SIZE

# read and validate it in parallel
(
  for i in $(seq 1 $FILE_SIZE); do
    read -r line
    if [ $line -ne $i ]; then
      echo "Error: expected ${i}, got ${line}"
      exit 1
    fi & true
  done
  wait
) < "$QSFS_TEST_RUN_DIR/$FILE_NAME"

# cleanup
rm_test_file $FILE_NAME
rm_test_dir ${WORK_DIR_NAME}
