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

#ifndef QSFS_CLIENT_CLIENTFACTORY_H_
#define QSFS_CLIENT_CLIENTFACTORY_H_

#include "boost/shared_ptr.hpp"

#include "base/Singleton.hpp"

namespace QS {

namespace FileSystem {
class Drive;
}  // namespace FileSystem

namespace Client {

class Client;
class ClientImpl;

class ClientFactory : public Singleton<ClientFactory> {
 public:
  ~ClientFactory() {}

 private:
  ClientFactory() {}

  boost::shared_ptr<Client> MakeClient();
  boost::shared_ptr<ClientImpl> MakeClientImpl();

  friend class Singleton<ClientFactory>;
  friend class Client;
  friend class QS::FileSystem::Drive;
};

}  // namespace Client
}  // namespace QS

#endif  // QSFS_CLIENT_CLIENTFACTORY_H_
