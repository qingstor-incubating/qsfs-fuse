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
# test case: random read file

set -o xtrace
set -o errexit

current_path=$(dirname "$0")
source "$current_path/utils.sh"

WORK_DIR_NAME="read_file_random"
mk_test_dir ${WORK_DIR_NAME}

FILE_NAME="${WORK_DIR_NAME}/random_read_file.txt"
MAX_NUM=10
HEAD_COUNT=5

# creat a file at first
append_test_file $FILE_NAME $MAX_NUM

# random read
random_numbers=( $(shuf -n $HEAD_COUNT "$QSFS_TEST_RUN_DIR/$FILE_NAME") )

# validation
count_=${#random_numbers[@]}
if [ $count_ -ne $HEAD_COUNT ]; then
  echo "Error: expected random read count ${HEAD_COUNT}, got ${count_}"
  exit 1
fi
for i in ${random_numbers[@]}; do
  if [ $i -lt 1 ] || [ $i -gt $MAX_NUM ]; then
    echo "Error: expected number belong to [1,$MAX_NUM], got $i"
    exit 1
  fi
done

# clean up
rm_test_file $FILE_NAME
rm_test_dir ${WORK_DIR_NAME}