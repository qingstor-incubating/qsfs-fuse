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
# test case: make directory and remove directory

set -o xtrace
set -o errexit

current_path=$(dirname "$0")
source "$current_path/utils.sh"

mk_test_dir
rm_test_dir

DIR_NAME='QingCloudㅙ=글シン歓迎'
mk_test_dir $DIR_NAME
rm_test_dir $DIR_NAME

# remove non-empty dir
DIR_NAME='dir_nonempty'
FILE_IN_DIR="$DIR_NAME/file"
mk_test_dir $DIR_NAME
mk_test_file $FILE_IN_DIR '0123456789'
rm_test_file $FILE_IN_DIR
rm_test_dir $DIR_NAME

# remove non-empty dir with recursive rm
FILE1_IN_DIR="$DIR_NAME/file1"
FILE2_IN_DIR="$DIR_NAME/file2"
mk_test_dir $DIR_NAME
mk_test_file $FILE1_IN_DIR
mk_test_file $FILE2_IN_DIR
rm_test_dir $DIR_NAME

