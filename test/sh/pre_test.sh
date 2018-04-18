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
# things to do before the tests run
#
# 1. start qsfs
# 2. make run dir

current_path=$(dirname "$0")
source "$current_path/common.sh"

# 1. start qsfs
# TODO(jim)

# 2. make run dir
if [ ! -d "$RUN_DIR" ]; then
  echo "make qsfs run dir [path=$RUN_DIR]"
  mkdir $RUN_DIR
fi
