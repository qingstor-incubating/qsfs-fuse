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
#ifndef QSFS_BASE_SINGLETON_HPP_
#define QSFS_BASE_SINGLETON_HPP_

#include "boost/noncopyable.hpp"
#include "boost/scoped_ptr.hpp"
#include "boost/thread/once.hpp"

//
// Use:
// class YourClass : public Singleton<YourClass> {
// public:
//   void YourMethod() { }
// private:
//   YourClass() { /* init */ }
//   friend class Singleton<YourClass>;
// };
//
// Using Your Singleton:
// YourClass::Instance().YourMethod();
//
//
template <typename T>
class Singleton : private boost::noncopyable {
 public:
  static T& Instance() {
    boost::call_once(m_flag, Init);
    return *m_instance;
  }

 protected:
  static void Init() { m_instance.reset(new T); }

 protected:
  Singleton() {}
  virtual ~Singleton() {}

 private:
  static boost::scoped_ptr<T> m_instance;
  static boost::once_flag m_flag;
};

template <typename T>
boost::scoped_ptr<T> Singleton<T>::m_instance(0);

template <typename T>
boost::once_flag Singleton<T>::m_flag = BOOST_ONCE_INIT;


#endif  // QSFS_BASE_SINGLETON_HPP_
