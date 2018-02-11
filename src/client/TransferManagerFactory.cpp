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

#include "client/TransferManagerFactory.h"

#include "boost/shared_ptr.hpp"

#include "client/ClientConfiguration.h"
#include "client/NullTransferManager.h"
#include "client/QSTransferManager.h"
#include "client/TransferManager.h"
#include "client/URI.h"

namespace QS {

namespace Client {

using boost::shared_ptr;

shared_ptr<TransferManager> TransferManagerFactory::Create(
    const TransferManagerConfigure &config) {
  shared_ptr<TransferManager> transferManager = shared_ptr<TransferManager>();
  Http::Host::Value host = ClientConfiguration::Instance().GetHost();
  switch (host) {
    case Http::Host::QingStor: {
    transferManager =
        shared_ptr<QSTransferManager>(new QSTransferManager(config));
      break;
    }
    // Add other cases here
    case Http::Host::Null:  // Bypass
    default: {
      TransferManagerConfigure nullConfig(0, 0, 0);
      transferManager =
          shared_ptr<NullTransferManager>(new NullTransferManager(nullConfig));
      break;
    }
  }
  return transferManager;
}

}  // namespace Client
}  // namespace QS
