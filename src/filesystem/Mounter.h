// +-------------------------------------------------------------------------
// | Copyright (C) 2017 Yunify, Inc.
// +-------------------------------------------------------------------------
// | Licensed under the Apache License, Version 2.0 (the "License");
// | You may not use this work except in compliance with the License.
// | You may obtain a copy of the License in the LICENSE file, or at:
// |
// | http://www.apache.org/licenses/LICENSE-2.0
// |
// | Unless required by applicable law or agreed to in writing, software
// | distributed under the License is distributed on an "AS IS" BASIS,
// | WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// | See the License for the specific language governing permissions and
// | limitations under the License.
// +-------------------------------------------------------------------------

#ifndef QSFS_FILESYSTEM_MOUNTER_H_
#define QSFS_FILESYSTEM_MOUNTER_H_

#include <string>
#include <utility>

#include "base/Singleton.hpp"

namespace QS {

namespace Configure {

class Options;
}

namespace FileSystem {

class Mounter : public Singleton<Mounter> {
 public:
  ~Mounter() {}

 public:
  std::pair<bool, std::string> IsMountable(const std::string &mountPoint,
                                           bool logOn) const;
  bool Mount(const QS::Configure::Options &options, bool logOn) const;
  bool DoMount(const QS::Configure::Options &options, bool logOn,
               void *user_data) const;

 private:
  Mounter() {}

  friend class Singleton<Mounter>;
};

}  // namespace FileSystem
}  // namespace QS

#endif  // QSFS_FILESYSTEM_MOUNTER_H_
