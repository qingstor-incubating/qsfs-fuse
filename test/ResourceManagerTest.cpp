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

#include <vector>

#include "boost/bind.hpp"
#include "boost/foreach.hpp"
#include "boost/thread/future.hpp"
#include "boost/thread/thread_time.hpp"
#include "gtest/gtest.h"

#include "base/Logging.h"
#include "base/Utils.h"
#include "data/ResourceManager.h"

namespace QS {

namespace Data {

using boost::packaged_task;
using boost::unique_future;

using std::vector;
using ::testing::Test;

// default log dir
static const char *defaultLogDir = "/tmp/qsfs.test.logs/";
void InitLog() {
  QS::Utils::CreateDirectoryIfNotExists(defaultLogDir);
  QS::Logging::Log::Instance().Initialize(defaultLogDir);
}

class ResourceManagerTest : public Test {
 protected:
  static void SetUpTestCase() { InitLog(); }

  void TestDefaultCtor() {
    ResourceManager manager;
    EXPECT_FALSE(manager.ResourcesAvailable());
  }

  void TestPutResource() {
    ResourceManager manager;
    manager.PutResource(Resource(new vector<char>(10)));
    manager.PutResource(Resource(new vector<char>(10)));
    manager.PutResource(Resource(new vector<char>(10)));
    manager.PutResource(Resource(new vector<char>(10)));
    manager.PutResource(Resource(new vector<char>(10)));
    EXPECT_TRUE(manager.ResourcesAvailable());

    vector<Resource> resources = manager.ShutdownAndWait(5);
    BOOST_FOREACH(Resource &resource, resources) {
      if (resource) {
        resource.reset();
      }
    }

    EXPECT_FALSE(manager.ResourcesAvailable());
  }

  void TestAcquireReleaseResource() {
    ResourceManager manager;
    manager.PutResource(Resource(new vector<char>(10)));

    packaged_task<Resource> task = packaged_task<Resource>(boost::bind(
        boost::type<Resource>(), &ResourceManager::Acquire, &manager));

    unique_future<Resource> f = task.get_future();
    task();
    f.timed_wait(boost::posix_time::milliseconds(100));
    boost::future_state::state status = f.get_state();
    ASSERT_EQ(status, boost::future_state::ready);
    // resource is acquired, so no available resource now
    EXPECT_FALSE(manager.ResourcesAvailable());

    Resource resource = f.get();
    ASSERT_EQ(*(resource), vector<char>(10));

    manager.Release(resource);
    // resource is released, so resource is available now
    EXPECT_TRUE(manager.ResourcesAvailable());

    vector<Resource> resources = manager.ShutdownAndWait(1);
    BOOST_FOREACH(Resource &resource, resources) {
      if (resource) {
        resource.reset();
      }
    }
    EXPECT_FALSE(manager.ResourcesAvailable());
  }
};

TEST_F(ResourceManagerTest, Default) { TestDefaultCtor(); }

TEST_F(ResourceManagerTest, PutResource) { TestPutResource(); }

TEST_F(ResourceManagerTest, AcquireReleaseResource) {
  TestAcquireReleaseResource();
}

}  // namespace Data
}  // namespace QS

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  int code = RUN_ALL_TESTS();
  return code;
}
