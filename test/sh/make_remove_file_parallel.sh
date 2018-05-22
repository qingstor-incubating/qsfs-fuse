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
# test case: make file and remove file parallel

set -o xtrace
set -o errexit

current_path=$(dirname "$0")
source "$current_path/utils.sh"

WORK_DIR_NAME="make_remove_file_parallel"
mk_test_dir ${WORK_DIR_NAME}

THREADS=5
TEST_PREFIX="${WORK_DIR_NAME}/file"

mk_test_file_parallel ${THREADS} ${TEST_PREFIX}
rm_test_file_parallel ${THREADS} ${TEST_PREFIX}

rm_test_dir ${WORK_DIR_NAME}