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

# write after seek ahead
FILE_NAME="write_after_seek_parallel.txt"
FILE_TEST="$QSFS_TEST_RUN_DIR/$FILE_NAME"
THREADS=10
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

# validation
FILE_SIZE=$(stat -c %s ${FILE_TEST})
if [ $FILE_SIZE -lt 1 ] || [ $FILE_SIZE -gt $THREADS ]; then
  echo "Error: expected ${FILE_TEST} has length belong to [1,$THREADS], got ${FILE_SIZE}"
  exit 1
fi

# cleanup
rm_test_file $FILE_NAME