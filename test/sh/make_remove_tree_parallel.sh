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

cd $QSFS_TEST_RUN_DIR
DIR_TREE="make_remove_tree"
if [ -d $DIR_TREE ]; then
  rm -rf $DIR_TREE
fi
mkdir $DIR_TREE && cd $DIR_TREE

BASE=5
# create tree in parallel
(
  for i in $(seq 1 $BASE); do
    mkdir ${i}
    for j in $(seq 1 $BASE); do
      mkdir ${i}/${j}
      for k in $(seq 1 $BASE); do
        touch ${i}/${j}/${k} & true
      done
    done
  done
  wait
)

# validation
for i in $(seq 1 $BASE); do
  if [ ! -d ${i} ]; then
    echo "Error: expected dir $QSFS_TEST_RUN_DIR/${i} exist, but not exist"
    exit 1
  fi
  for j in $(seq 1 $BASE); do
    if [ ! -d "${i}/${j}" ]; then
      echo "Error: expected dir $QSFS_TEST_RUN_DIR/${i}/${j} exist, but not exist"
      exit 1
    fi
    for k in $(seq 1 $BASE); do
      if [ ! -f "${i}/${j}/${k}" ]; then
        echo "Error: expected file $QSFS_TEST_RUN_DIR/${i}/${j}/${k} exist, but not exist"
        exit 1
      fi
    done
  done
done

# rm tree
cd $QSFS_TEST_RUN_DIR
rm -rf "$DIR_TREE"
