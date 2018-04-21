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
# common configure for interagtion function test cases
#

QSFS_TEST_RUN_DIR=$(printenv QSFS_INTEGRATION_TEST_RUN_DIR);
#QSFS_TEST_RUN_DIR=$(env | grep QSFS_INTEGRATION_TEST_RUN_DIR | grep -oe '[^=]*$');
export QSFS_TEST_RUN_DIR
