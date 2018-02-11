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

#ifndef QSFS_FILESYSTEM_INITIALIZER_H_
#define QSFS_FILESYSTEM_INITIALIZER_H_

#include <queue>
#include <utility>
#include <vector>

#include "boost/function.hpp"
#include "boost/noncopyable.hpp"
#include "boost/thread/once.hpp"

// Declare main in global namespace before class Initializer, since friend
// declarations can only introduce names in the surrounding namespace.
extern int main(int argc, char **argv);

namespace QS {

namespace FileSystem {

struct Priority {
  enum Value {
    First = 1,
    Second = 2,
    Third = 3,
    Fourth = 4,
    Fifth = 5
    // Add others as you want
  };
};

typedef boost::function<void()> InitFunction;
typedef std::pair<Priority::Value, InitFunction> PriorityInitFuncPair;

class Initializer : private boost::noncopyable {
 public:
  explicit Initializer(const PriorityInitFuncPair &initFuncPair);

  ~Initializer() {}

 private:
  static void Init();
  static void RunInitializers();
  static void RemoveInitializers();
  void SetInitializer(const PriorityInitFuncPair &pair);
  friend int ::main(int argc, char **argv);

 private:
  Initializer() {}

  struct Greater {
    bool operator()(const PriorityInitFuncPair &left,
                    const PriorityInitFuncPair &right) const {
      return static_cast<int>(left.first) > static_cast<int>(right.first);
    }
  };

  typedef std::priority_queue<PriorityInitFuncPair,
                              std::vector<PriorityInitFuncPair>, Greater>
      InitFunctionQueue;
  static InitFunctionQueue *m_queue;
  static boost::once_flag m_initOnceFlag;
};

}  // namespace FileSystem
}  // namespace QS

#endif  // QSFS_FILESYSTEM_INITIALIZER_H_
