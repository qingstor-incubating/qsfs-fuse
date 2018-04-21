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
# test case: list directory

set -o xtrace
set -o errexit

current_path=$(dirname "$0")
source "$current_path/utils.sh"

rm -rf ${QSFS_TEST_RUN_DIR}/*
mk_test_file
mk_test_dir

file_cnt=$(ls $QSFS_TEST_RUN_DIR -1 | wc -l)
if [ $file_cnt -ne 2 ]; then
  echo "Error: expected 2 files in ${QSFS_TEST_RUN_DIR}, got $file_cnt"
  exit 1
fi

rm_test_file
rm_test_dir
