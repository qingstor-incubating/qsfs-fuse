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
# test case: make and remove tree parallel

set -o xtrace
set -o errexit

current_path=$(dirname "$0")
source "$current_path/common.sh"

cd /tmp
DIR_TREE="rsync-tree-inplace"
if [ -d $DIR_TREE ]; then
  rm -rf $DIR_TREE
fi
mkdir "$DIR_TREE" && cd $DIR_TREE
BASE=3

# create tree in local
for i in $(seq 1 $BASE); do
  mkdir ${i}
  for j in $(seq 1 $BASE); do
    mkdir ${i}/${j}
    for k in $(seq 1 $BASE); do
      echo "${i} ${j} ${k}" > "${i}/${j}/${k}.txt"
    done
  done
done

# rsync to qingstor
# by default rsync will call mkstemp makes a temporary file and sets its permissions to 0600
# as for now, qsfs not support chmod/chgrp, so we do the test in following two ways
# 1st: add inplace option
# 2nd: add --no-perms --no-owner --no-group options

# 1st way --inplace option
cd /tmp
rsync -a --inplace $DIR_TREE $QSFS_TEST_RUN_DIR

# validation
cd "$QSFS_TEST_RUN_DIR/$DIR_TREE"
for i in $(seq 1 $BASE); do
  if [ ! -d ${i} ]; then
    echo "Error: expected dir $QSFS_TEST_RUN_DIR/$DIR_TREE/${i} exist, but not exist"
    exit 1
  fi
  for j in $(seq 1 $BASE); do
    if [ ! -d "${i}/${j}" ]; then
      echo "Error: expected dir $QSFS_TEST_RUN_DIR/$DIR_TREE/${i}/${j} exist, but not exist"
      exit 1
    fi
    for k in $(seq 1 $BASE); do
      if [ ! -f "${i}/${j}/${k}.txt" ]; then
        echo "Error: expected file $QSFS_TEST_RUN_DIR/$DIR_TREE/${i}/${j}/${k} exist, but not exist"
        exit 1
      fi
    done
  done
done

# cleanup
cd $QSFS_TEST_RUN_DIR
rm -rf "$DIR_TREE"
# cleanup local
cd /tmp
rm -rf "$DIR_TREE"
