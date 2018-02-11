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

#include "gtest/gtest.h"

#include "boost/bind.hpp"
#include "boost/make_shared.hpp"
#include "boost/shared_ptr.hpp"
#include "boost/thread/future.hpp"
#include "boost/thread/locks.hpp"
#include "boost/thread/mutex.hpp"
#include "boost/thread/thread_time.hpp"
#include "boost/typeof/typeof.hpp"

#include "base/ThreadPool.h"

namespace QS {

namespace Threading {

// In order to test ThreadPool private members, need to define
// test fixture and tests in the same namespace with ThreadPool,
// so they can be friends of class ThreadPool.

using boost::bind;
using boost::unique_future;
using boost::make_shared;
using boost::packaged_task;
using boost::shared_ptr;
using boost::type;
using ::testing::Test;

// Return n!. For negative, n! is defined to be 1;
int Factorial(int n) {
  int result = 1;
  for (int i = 1; i <= n; ++i) {
    result *= i;
  }
  return result;
}

int Add(int a, int b) { return a + b; }

static const int poolSize_ = 2;

class ThreadPoolTest : public Test {
 public:
  unique_future<int> FactorialCallable(int n) {
    shared_ptr<packaged_task<int> > pTask =
        make_shared<packaged_task<int> >(bind(type<int>(), Factorial, n));
    m_pThreadPool->SubmitToThread(boost::bind<void>(
        PackageFunctor1<BOOST_TYPEOF(&Factorial), int>(pTask)));
    return pTask->get_future();
  }

  unique_future<int> FactorialCallablePrioritized(int n) {
    shared_ptr<packaged_task<int> > pTask =
        make_shared<packaged_task<int> >(bind(type<int>(), Factorial, n));
    m_pThreadPool->SubmitToThread(
        boost::bind<void>(
            PackageFunctor1<BOOST_TYPEOF(&Factorial), int>(pTask)),
        true);
    return pTask->get_future();
  }

 protected:
  void SetUp() {
    m_pThreadPool = new ThreadPool(poolSize_);
    m_pThreadPool->Initialize();
  }

  void TearDown() { delete m_pThreadPool; }

  // test private member
  void TestInterruptThreadPool() {
    EXPECT_FALSE(m_pThreadPool->HasTasks());

    m_pThreadPool->StopProcessing();
    unique_future<int> f = m_pThreadPool->SubmitCallable(Factorial, 5);
    EXPECT_TRUE(m_pThreadPool->HasTasks());

    f.timed_wait(boost::posix_time::milliseconds(100));
    boost::future_state::state fStatus = f.get_state();
    ASSERT_EQ(fStatus, boost::future_state::waiting);

    Task *task = m_pThreadPool->PopTask();
    EXPECT_FALSE(m_pThreadPool->HasTasks());
    delete task;

    // Should never invoke f.get(), as after stoping thredpool, task will
    // neverget a chance to execute, so this will hang the program there.
    // f.get();
  }

 protected:
  ThreadPool *m_pThreadPool;
};

TEST_F(ThreadPoolTest, TestInterrupt) { TestInterruptThreadPool(); }

TEST_F(ThreadPoolTest, TestSubmitToThread) {
  int num = 5;
  unique_future<int> f = FactorialCallable(num);
  f.timed_wait(boost::posix_time::milliseconds(100));
  boost::future_state::state fStatus = f.get_state();
  ASSERT_EQ(fStatus, boost::future_state::ready);
  ASSERT_TRUE(f.is_ready());
  EXPECT_EQ(f.get(), 120);

  unique_future<int> f1 = FactorialCallablePrioritized(num);
  f1.timed_wait(boost::posix_time::milliseconds(100));
  boost::future_state::state fStatus1 = f1.get_state();
  ASSERT_EQ(fStatus1, boost::future_state::ready);
  ASSERT_TRUE(f1.is_ready());
  EXPECT_EQ(f1.get(), 120);
}

int result = 0;
boost::mutex lockResult;

void Add1(const int &value) {
  boost::lock_guard<boost::mutex> locker(lockResult);
  result = value;
}

void Add2(const int &v1, const int &v2) {
  boost::lock_guard<boost::mutex> locker(lockResult);
  result = v1 + v2;
}

void Add3(const int &v1, const int &v2, const int &v3) {
  boost::lock_guard<boost::mutex> locker(lockResult);
  result = v1 + v2 + v3;
}

TEST_F(ThreadPoolTest, TestSubmit) {
  m_pThreadPool->Submit(Add1, 1);
  boost::this_thread::sleep(boost::posix_time::milliseconds(30));
  EXPECT_EQ(result, 1);
  m_pThreadPool->Submit(Add2, 1, 10);
  boost::this_thread::sleep(boost::posix_time::milliseconds(30));
  EXPECT_EQ(result, 11);
  m_pThreadPool->Submit(Add3, 1, 10, 100);
  boost::this_thread::sleep(boost::posix_time::milliseconds(30));
  EXPECT_EQ(result, 111);
  m_pThreadPool->SubmitPrioritized(Add1, 1);
  boost::this_thread::sleep(boost::posix_time::milliseconds(30));
  EXPECT_EQ(result, 1);
  m_pThreadPool->SubmitPrioritized(Add2, 1, 10);
  boost::this_thread::sleep(boost::posix_time::milliseconds(30));
  EXPECT_EQ(result, 11);
  m_pThreadPool->SubmitPrioritized(Add3, 1, 10, 100);
  boost::this_thread::sleep(boost::posix_time::milliseconds(30));
  EXPECT_EQ(result, 111);
}

TEST_F(ThreadPoolTest, TestSubmitCallable) {
  int num = 5;
  unique_future<int> f1 = m_pThreadPool->SubmitCallable(Factorial, num);
  f1.timed_wait(boost::posix_time::milliseconds(100));
  boost::future_state::state fStatus1 = f1.get_state();
  ASSERT_EQ(fStatus1, boost::future_state::ready);
  EXPECT_EQ(f1.get(), 120);

  int a = 1;
  int b = 11;
  unique_future<int> f2 = m_pThreadPool->SubmitCallablePrioritized(Add, a, b);
  f2.timed_wait(boost::posix_time::milliseconds(100));
  boost::future_state::state fStatus2 = f2.get_state();
  ASSERT_EQ(fStatus2, boost::future_state::ready);
  EXPECT_EQ(f2.get(), 12);
}

struct callback0 {
  callback0() {}
  void operator()(int result) {
    EXPECT_EQ(result, 120);
  }
};

struct callback {
  callback() {}
  void operator()(int result, int num) {
    EXPECT_EQ(num, 5);
    EXPECT_EQ(result, 120);
  }
};

struct callback1 {
  callback1() {}
  void operator()(int result, int a, int b) {
    EXPECT_EQ(a, 1);
    EXPECT_EQ(b, 11);
    EXPECT_EQ(result, 12);
  }
};

TEST_F(ThreadPoolTest, TestSubmitAsync) {
  m_pThreadPool->SubmitAsync(
      boost::bind(boost::type<void>(), callback0(), _1), Factorial, 5);
  m_pThreadPool->SubmitAsync(
      boost::bind(boost::type<void>(), callback(), _1, _2), Factorial, 5);

  m_pThreadPool->SubmitAsyncPrioritized(
      boost::bind(boost::type<void>(), callback1(), _1, _2, _3), Add, 1, 11);
}

}  // namespace Threading
}  // namespace QS

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  int code = RUN_ALL_TESTS();
  return code;
}
