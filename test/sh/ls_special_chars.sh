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
# test case: special characters

set -o xtrace
set -o errexit

current_path=$(dirname "$0")
source "$current_path/common.sh"

cd $RUN_DIR
ls 'special' 2>&1 | grep -q 'No such file or directory'
ls 'special?' 2>&1 | grep -q 'No such file or directory'
ls 'special*' 2>&1 | grep -q 'No such file or directory'
ls 'special~' 2>&1 | grep -q 'No such file or directory'
ls 'specialµ' 2>&1 | grep -q 'No such file or directory'
ls 'special글' 2>&1 | grep -q 'No such file or directory'
ls 'specialシ' 2>&1 | grep -q 'No such file or directory'
ls 'special歓' 2>&1 | grep -q 'No such file or directory'