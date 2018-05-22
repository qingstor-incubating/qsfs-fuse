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
# test case: write_after_seek_file in parallel

set -o xtrace
set -o errexit

current_path=$(dirname "$0")
source "$current_path/utils.sh"

WORK_DIR_NAME="write_after_seek_file_parallel"
mk_test_dir ${WORK_DIR_NAME}

# write after seek ahead
FILE_NAME="${WORK_DIR_NAME}/write_after_seek_parallel.txt"
FILE_TEST="$QSFS_TEST_RUN_DIR/$FILE_NAME"
THREADS=6
NUM=$(( $THREADS - 1 ))

# touch file
if [ -f $FILE_TEST ]; then
  rm -f $FILE_TEST
fi
touch $FILE_TEST

# seek and write 
(
  for i in $(seq 0 $NUM); do
    echo $i | dd of=$FILE_TEST bs=1c count=1 seek=$i & true
  done
  wait
)

# wait & validation
TRY_COUNT=3
while true; do
  FILE_SIZE=$(stat -c %s ${FILE_TEST})
  # as write after seek will override the previous data
  if [ $FILE_SIZE -ge 1 ] && [ $FILE_SIZE -le $THREADS ]; then
    break;
  fi
  let TRY_COUNT--
  if [ $TRY_COUNT -le 0 ]; then
    echo "Error: expected ${FILE_TEST} has length belong to [1,$THREADS], got ${FILE_SIZE}"
    exit 1
  fi
  sleep 1
done

# cleanup
rm_test_file $FILE_NAME
rm_test_dir ${WORK_DIR_NAME}