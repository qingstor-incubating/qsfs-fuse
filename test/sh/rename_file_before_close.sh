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
# test case: rename file before close

set -o xtrace
set -o errexit

current_path=$(dirname "$0")
source "$current_path/utils.sh"

FILE_NAME='test_rename_before_close.txt'
FILE_NAME_NEW=${FILE_NAME}_new
FILE_="$QSFS_TEST_RUN_DIR/$FILE_NAME" # to avoid name overloading from utils
FILE_NEW="$QSFS_TEST_RUN_DIR/$FILE_NAME_NEW"
(
  echo foo
  mv ${FILE_} ${FILE_NEW}
) > ${FILE_}

if ! cmp <(echo foo) ${FILE_NEW}; then
    echo "Error: ${FILE} rename before close failed"
    exit 1
fi

rm_test_file "${FILE_NAME_NEW}"
rm -f ${FILE_}