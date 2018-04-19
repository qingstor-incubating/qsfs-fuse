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
# test case: redirect

set -o xtrace
set -o errexit

current_path=$(dirname "$0")
source "$current_path/utils.sh"

DATA_LINE0='abcdefghijk'
DATA_LINE1='ABCDEFGHIJK'
DATA_LINE2='12345678900'
FILE_NAME='test_redirects.txt'
FILE_="$RUN_DIR/$FILE_NAME"

mk_test_file $FILE_NAME $DATA_LINE0
CONTENT=$(cat ${FILE_})
if [ "${CONTENT}" != "${DATA_LINE0}" ]; then
  echo "Error: expected ${FILE_} contain ${DATA_LINE0}, got ${CONTENT}"
  exit 1
fi

echo ${DATA_LINE1} > $FILE_
CONTENT=$(cat ${FILE_})
if [ "${CONTENT}" != "${DATA_LINE1}" ]; then
  echo "Error: expected ${FILE_} contain ${DATA_LINE1}, got ${CONTENT}"
  exit 1
fi

echo ${DATA_LINE2} >> $FILE_
LINE1=$(sed -n '1,1p' $FILE_)
LINE2=$(sed -n '2,2p' $FILE_)
if [ ${LINE1} != ${DATA_LINE1} ]; then
  echo "Error: expected ${FILE_} first line ${DATA_LINE1}, got ${LINE1}"
  exit 1
fi

if [ ${LINE2} != ${DATA_LINE2} ]; then
  echo "Error: expected ${FILE_} second line ${DATA_LINE2}, got ${LINE2}"
  exit 1
fi

rm_test_file $FILE_NAME

