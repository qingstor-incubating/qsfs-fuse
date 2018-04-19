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
# test case: symlink file

set -o xtrace
set -o errexit

current_path=$(dirname "$0")
source "$current_path/utils.sh"

FILE_NAME='origin_file'
FILE_NAME_TO='symlink_file'
FILE_="$RUN_DIR/$FILE_NAME"  # avoid name overloading from utils
FILE_TO="$RUN_DIR/$FILE_NAME_TO"
rm_test_file $FILE_NAME
rm_test_file $FILE_NAME_TO

echo foo > $FILE_

ln -s $FILE_ $FILE_TO
cmp $FILE_ $FILE_TO

if [ ! -L $FILE_TO ]; then
  echo "Error: expect ${FILE_TO} is symlink, but it's not"
  exit 1
fi

if [ ! -f $FILE_TO ]; then
  echo "Error: {FILE_TO} is not existing"
  exit 1
fi

rm_test_file $FILE_NAME_TO  # must remove symlink firstly
rm_test_file $FILE_NAME