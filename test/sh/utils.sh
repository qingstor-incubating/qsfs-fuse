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

set -o errexit

current_path=$(dirname "$0")
source "$current_path/common.sh"

# configuration
TEST_TEXT="HELLO 你好"
TEST_TEXT_FILE="$RUN_DIR/test-file-qsfs.txt"
TEST_DIR="$RUN_DIR/testdir"


function mk_test_file {
  if [ $# == 0 ]; then
    TEXT=$TEST_TEXT
    FILE=$TEST_TEXT_FILE
  else
    TEXT=$1
    if [ $# > 2 ]; then
      FILE="$RUN_DIR/$2"
    else
      FILE=$TEST_TEXT_FILE
    fi
  fi

  echo $TEXT > $FILE
  if [ ! -e $FILE ]; then
    echo "Could not create file ${FILE}, it does not exist"
    exit 1
  fi

  # wait & check
  TEST_TEXT_LEN=$(echo $TEXT | wc -c | awk '{print $1}')
  TRY_COUNT=3
  while true; do
    TEXT_FILE_LEN=$(wc -c $FILE | awk '{print $1}')
    if [ $TEST_TEXT_LEN -eq $TEXT_FILE_LEN ]; then
      break;
    fi
    let TRY_COUNT--
    if [ $TRY_COUNT -le 0 ]; then
      echo "Cound not create file ${TEST_TEXT_FILE}, that file size is something wrong"
    fi
    sleep 1
  done
}

function rm_test_file {
  if [ $# == 0 ]; then
    FILE=$TEST_TEXT_FILE
  else
    FILE="$RUN_DIR/$1"
  fi
  rm -f $FILE

  if [ -e $FILE ]; then
    echo "Could not cleanup file ${FILE}"
    exit 1
  fi
}

function mk_test_dir {
  if [ $# == 0 ]; then
    DIR=$TEST_DIR
  else
    DIR="$RUN_DIR/$1"
  fi

  if [ ! -e $DIR ]; then
    mkdir $DIR
  else
    echo "Warning: directory ${DIR} already exists"
  fi
  if [ ! -d $DIR ]; then
    echo "Could not create directory ${DIR}"
    exit 1
  fi
}

function rm_test_dir {
  if [ $# == 0 ]; then
    DIR=$TEST_DIR
  else
    DIR="$RUN_DIR/$1"
  fi

  rmdir $DIR
  if [ -e $DIR ]; then
    echo "Could not remove directory ${DIR}, it still exists"
    exit 1
  fi
}