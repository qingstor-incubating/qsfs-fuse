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

set -o xtrace
set -o errexit

current_path=$(dirname "$0")
source "$current_path/utils.sh"

mk_test_file
rm_test_file

TEXT_DATA='QingCloud©ⓐ➀Ⓐ♡㊊ㅙ=글ツマシン歓迎Oº°‘¨¨‘°ºOQingCloud'
FILE_NAME='青云文件ㅙ=글シン歓迎'
mk_test_file $TEXT_DATA $FILE_NAME
rm_test_file $FILE_NAME