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
# util functions
#
# when call mk_test_file, you probably need to call mv_test_file to cleanup
# same for mk_test_dir

set -o errexit

current_path=$(dirname "$0")
source "$current_path/common.sh"

# configuration
TEST_TEXT="HELLO 你好"
TEST_TEXT_FILE="$QSFS_TEST_RUN_DIR/test-file-qsfs.txt"
TEST_TEXT_FILENAME="test-file-qsfs.txt"
TEST_DIR="$QSFS_TEST_RUN_DIR/testdir"
TEST_DIRNAME="testdir"
TEST_APPEND_FILE_LEN=10
TEST_TEXT_FILE_PARALLEL_CNT=10
TEST_TEXT_FILE_PARALLEL_PREFIX="test_make_file_parallel"


#
# Make a text file
#
# Arguments:
#   $1 file name (optional)
#   $2 file data (optional)
# Returns:
#   None
function mk_test_file {
  if [ $# -eq 0 ]; then
    FILE=$TEST_TEXT_FILE
    TEXT=$TEST_TEXT
  else
    FILE="$QSFS_TEST_RUN_DIR/$1"
    if [ $# -gt 1 ]; then
      TEXT=$2
    else
      TEXT=$TEST_TEXT
    fi
  fi

  echo $TEXT > $FILE
  if [ ! -e $FILE ]; then
    echo "Error: Could not create file ${FILE}, it does not exist"
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
      echo "Error: Cound not create file ${FILE}, that file size is something wrong"
      exit 1
    fi
    sleep 1
  done
}

#
# Make text files parallel
#
# Arguments:
#   $1 number of files (optional)
#   $2 file prefix (optional)
# Returns:
#   None
function mk_test_file_parallel {
  if [ $# -eq 0 ]; then
    COUNT=$TEST_TEXT_FILE_PARALLEL_CNT
    PREFIX=$TEST_TEXT_FILE_PARALLEL_PREFIX
  else
    re='^[0-9]+$'
    if [[ $1 =~ $re ]]; then
      if [ $1 -lt 1 ]; then
        echo "Warning: file count ${1} is less than 1, use $TEST_TEXT_FILE_PARALLEL_CNT"
        COUNT=$TEST_TEXT_FILE_PARALLEL_CNT
      else
        COUNT=$1
      fi
    else
      echo "Warning: files count ${1} is not a integer, use $TEST_TEXT_FILE_PARALLEL_CNT"
      COUNT=$TEST_TEXT_FILE_PARALLEL_CNT
    fi

    PREFIX=$TEST_TEXT_FILE_PARALLEL_PREFIX
    if [ $# -gt 1 ]; then
      PREFIX=$2
    fi
  fi

  (
    for i in $(seq 1 $COUNT); do
      echo $i > "$QSFS_TEST_RUN_DIR/$PREFIX$i" & true
    done
    wait
  )

  # validation
  for i in $(seq 1 $COUNT); do
    fileName="$QSFS_TEST_RUN_DIR/$PREFIX$i"
    dat=$(cat "$fileName")
    if [ ! $dat -eq $i ]; then
      echo "Error: expected $i in $fileName, got $dat"
      exit 1
    fi
  done
}

#
# Remove a file
#
# Arguments:
#   $1 file name (optional)
# Returns:
#   None
function rm_test_file {
  if [ $# -eq 0 ]; then
    FILE=$TEST_TEXT_FILE
  else
    FILE="$QSFS_TEST_RUN_DIR/$1"
  fi

  if [ -e $FILE ]; then
    rm -f $FILE
  fi

  if [ -e $FILE ]; then
    echo "Error: Could not cleanup file ${FILE}"
    exit 1
  fi
}

#
# Remove text files parallel
#
# Arguments:
#   $1 number of files (optional)
#   $2 file prefix (optional)
# Returns:
#   None
function rm_test_file_parallel {
  if [ $# -eq 0 ]; then
    COUNT=$TEST_TEXT_FILE_PARALLEL_CNT
    PREFIX=$TEST_TEXT_FILE_PARALLEL_PREFIX
  else
    re='^[0-9]+$'
    if [[ $1 =~ $re ]]; then
      if [[ $1 -lt 1 ]]; then
        echo "Warning: file count ${1} is less than 1, use $TEST_TEXT_FILE_PARALLEL_CNT"
        COUNT=$TEST_TEXT_FILE_PARALLEL_CNT
      else
        COUNT=$1
      fi
    else
      echo "Warning: files count ${1} is not a integer, use $TEST_TEXT_FILE_PARALLEL_CNT"
      COUNT=$TEST_TEXT_FILE_PARALLEL_CNT
    fi

    PREFIX=$TEST_TEXT_FILE_PARALLEL_PREFIX
    if [ $# -gt 1 ]; then
      PREFIX=$2;
    fi
  fi

  (
    for i in $(seq 1 $COUNT); do
      fileName="$QSFS_TEST_RUN_DIR/$PREFIX$i" 
      rm $fileName & true
    done
    wait
  )

  # validation
  for i in $(seq 1 $COUNT); do
    fileName="$QSFS_TEST_RUN_DIR/$PREFIX$i" 
    if [ -e $fileName ]; then
      echo "Error: could not cleanup file $fileName"
      exit 1
    fi
  done
}


#
# Make a directory
#
# Arguments:
#   $1 dir(folder) name (optional)
# Returns:
#   None
function mk_test_dir {
  if [ $# -eq 0 ]; then
    DIR=$TEST_DIR
  else
    DIR="$QSFS_TEST_RUN_DIR/$1"
  fi

  if [ ! -e $DIR ]; then
    mkdir $DIR
  else
    echo "Warning: directory ${DIR} already exists"
  fi
  if [ ! -d $DIR ]; then
    echo "Error: Could not create directory ${DIR}"
    exit 1
  fi
}

#
# Remove a directory
#
# Arguments:
#   $1 dir(folder) name (optional)
# Returns:
#   None
function rm_test_dir {
  if [ $# -eq 0 ]; then
    DIR=$TEST_DIR
  else
    DIR="$QSFS_TEST_RUN_DIR/$1"
  fi

  files=$(ls -A $DIR)
  if [ -n "${files}" ]; then
    rm -rf $DIR
  else
    rmdir $DIR
  fi
  # fix integration test regression
  # QingStor change its logic to cache directory for performance issue,
  # this cache result in latency for dir deleting ops, the latency is
  # about 60s, so we choose to remove the validation for now.

  # if [ -e $DIR ]; then
  #   echo "Error: Could not remove directory ${DIR}, it still exists"
  #   exit 1
  # fi
}

#
# Append data to a file
# This will append numbers (from 1 to file_size) to a file
#
# Arguments:
#   $1 file name (optional)
#   $2 file size (optional)
# Returns:
#   None
function append_test_file {
  if [ $# -eq 0 ]; then
    FILE="$TEST_TEXT_FILE"
    SIZE=$TEST_APPEND_FILE_LEN
  else
    FILE="$QSFS_TEST_RUN_DIR/$1"
    if [ $# -gt 1 ]; then
      re='^[0-9]+$'
      if [[ $2 =~ $re ]]; then
        SIZE=$2
        if [ $SIZE -lt 1 ]; then
          echo "Warning: file size ${2} is less than 1, use $TEST_APPEND_FILE_LEN"
          SIZE=$TEST_APPEND_FILE_LEN
        fi
      else
        echo "Warning: file size ${2} is not a integer, use $TEST_APPEND_FILE_LEN"
        SIZE=$TEST_APPEND_FILE_LEN
      fi
    fi
  fi

  if [ -e $FILE ]; then
    rm -f $FILE
  fi

  for x in $(seq 1 $SIZE); do
    echo $x >> $FILE
  done

  # wait & verify contents of file
  TRY_COUNT=3
  while true; do
    FILE_SIZE=$(wc -l $FILE | awk '{print $1}')
    if [ $SIZE -eq $FILE_SIZE ]; then
      break;
    fi
    let TRY_COUNT--
    if [ $TRY_COUNT -le 0 ]; then
      echo "Error: expected file size ${SIZE}, got ${FILE_SIZE} [path=$FILE]"
      FILEDATA=$(cat "$FILE")
      echo "DATA: ${FILEDATA}"
      exit 1
    fi
    sleep 1
  done
}

#
# Append data to a file in parallel
# This will append numbers (from 1 to file_size) to a file
#
# Arguments:
#   $1 file name (optional)
#   $2 file size (optional)
# Returns:
#   None
function append_test_file_parallel {
  if [ $# -eq 0 ]; then
    FILE="$TEST_TEXT_FILE"
    SIZE=$TEST_APPEND_FILE_LEN
  else
    FILE="$QSFS_TEST_RUN_DIR/$1"
    if [ $# -gt 1 ]; then
      re='^[0-9]+$'
      if [[ $2 =~ $re ]]; then
        SIZE=$2
        if [ $SIZE -lt 1 ]; then
          echo "Warning: file size ${2} is less than 1, use $TEST_APPEND_FILE_LEN"
          SIZE=$TEST_APPEND_FILE_LEN
        fi
      else
        echo "Warning: file size ${2} is not a integer, use $TEST_APPEND_FILE_LEN"
        SIZE=$TEST_APPEND_FILE_LEN
      fi
    fi
  fi

  if [ -e $FILE ]; then
    rm -f $FILE
  fi

  # write in parallel
  (
    for i in $(seq 1 $SIZE); do
      echo $i >> $FILE & true
    done
    wait
  )

  # As http request has latency, we need to wait for a while when validate
  # wait & validation
  TRY_COUNT=3
  while true; do
    LINE_NO=$(wc -l $FILE | awk '{print $1}')
    if [ $LINE_NO -eq ${SIZE} ]; then
      break;
    fi
    let TRY_COUNT--
    if [ $TRY_COUNT -le 0 ]; then
      echo "Error: expected ${SIZE} lines in ${FILE}, got ${LINE_NO} lines"
      FILEDATA=$(cat "$FILE")
      echo "DATA: ${FILEDATA}"
      exit 1
    fi
    sleep 1
  done
}


#
# Truncate a file
# If given file not exists, an empty file will be created
#
# Arguments:
#   $1 file name (optional)
#   $2 target size (optional)
# Returns:
#   None
function truncate_test_file {
  if [ $# -eq 0 ]; then
    FILE="$TEST_TEXT_FILE"
    TARGET_SIZE=0
  elif [ $# -eq 1 ]; then
    FILE="$QSFS_TEST_RUN_DIR/$1"
    TARGET_SIZE=0
  else
    FILE="$QSFS_TEST_RUN_DIR/$1"
    TARGET_SIZE=$2
    if [ $TARGET_SIZE -lt 0 ]; then
      echo "Warning: truncate size $TARGET_SIZE is less than 0, use 0"
      TARGET_SIZE=0
    fi
  fi

  if [ ! -e ${FILE} ]; then
    touch ${FILE}
  else
    if [ ! -f ${FILE} ]; then
      echo "Error: ${FILE} exists, but is not a regular file"
      exit 1
    fi
  fi

  if [ $TARGET_SIZE -eq 0 ]; then
    # This should trigger open(path, O_RDWR | O_TRUNC...)
    : > ${FILE}
  else
    truncate ${FILE} -s $TARGET_SIZE
  fi

  # wait & verify file size
  TRY_COUNT=3
  while true; do
    FILE_SIZE=$(stat -c %s ${FILE})
    if [ $TARGET_SIZE -eq $FILE_SIZE ]; then
      break;
    fi
    let TRY_COUNT--
    if [ $TRY_COUNT -le 0 ]; then
      echo "Error: expected ${FILE} to be $TARGET_SIZE length, got $FILE_SIZE"
      exit 1
    fi
    sleep 1
  done
}


#
# Rename a file
# If file not exist, make a file with data of 'TEST_TEXT'
# The renamed file will have a new name of 'OLD_FILE_NAME_renamed'
#
# Arguments:
#   $1 file name (optional)
# Returns:
#   None
function mv_test_file {
  if [ $# -eq 0 ]; then
    FILENAME=$TEST_TEXT_FILENAME
  else
    FILENAME=$1
  fi
  FILENAME_RENAMED="${FILENAME}_renamed"
  FILE="$QSFS_TEST_RUN_DIR/$FILENAME"
  FILE_RENAMED="$QSFS_TEST_RUN_DIR/$FILENAME_RENAMED"

  mk_test_file $FILENAME
  LEN=$(wc -c $FILE | awk '{print $1}')

  mv $FILE $FILE_RENAMED
  if [ ! -f $FILE_RENAMED ]; then
    echo "Error: could not rename file, ${FILE_RENAMED} not exists"
    exit 1
  fi

  LEN_RENAMED=$(wc -c $FILE_RENAMED | awk '{print $1}')
  if [ $LEN -ne $LEN_RENAMED ]; then
    echo "Error: file ${FILE_RENAMED} expected length ${LEN}, got ${LEN_RENAMED}"
    exit 1
  fi

  rm_test_file $FILENAME_RENAMED
}


#
# Rename a dir
# If dir not exist, make a dir with data of 'TEST_TEXT'
# The renamed dir will have a new name of 'OLD_DIR_NAME_renamed'
#
# Arguments:
#   $1 dir name (optional)
# Returns:
#   None
function mv_test_dir {
  if [ $# -eq 0 ]; then
    DIRNAME=$TEST_DIRNAME
  else 
    DIRNAME=$1
  fi
  DIRNAME_RENAMED="${DIRNAME}_renamed"
  DIR="$QSFS_TEST_RUN_DIR/$DIRNAME"
  DIR_RENAMED="$QSFS_TEST_RUN_DIR/$DIRNAME_RENAMED"

  if [ -e ${DIR} ]; then
    echo "Error: unexpected, the file/dir ${DIR} exists"
    exit 1
  fi

  mk_test_dir $DIRNAME
  mv ${DIR} ${DIR_RENAMED}

  if [ ! -d ${DIR_RENAMED} ]; then
    echo "Error: dir {DIR} was not renamed"
    exit 1
  fi

  rm_test_dir $DIRNAME_RENAMED
}
