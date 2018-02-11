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

#ifndef QSFS_CLIENT_CLIENTCONFIGURATION_H_
#define QSFS_CLIENT_CLIENTCONFIGURATION_H_

#include <stdint.h>

#include <string>

#include "boost/shared_ptr.hpp"

#include "client/Credentials.h"
#include "client/Protocol.h"
#include "client/URI.h"

// Declare in global namespace before class ClientConfiguration, since friend
// declarations can only introduce names in the surrounding namespace.
extern void ClientConfigurationInitializer();

namespace QS {

namespace Client {

class QSClient;

struct ClientLogLevel {  // SDK log level
  enum Value {
    Verbose = -2,
    Debug = -1,
    Info = 0,
    Warn = 1,
    Error = 2,
    Fatal = 3
  };
};

std::string GetClientLogLevelName(ClientLogLevel::Value level);
ClientLogLevel::Value GetClientLogLevelByName(const std::string& name);

class ClientConfiguration;
void InitializeClientConfiguration(
    const boost::shared_ptr<ClientConfiguration>& config);

class ClientConfiguration {
 public:
  static ClientConfiguration& Instance();

 public:
  explicit ClientConfiguration(
      const Credentials& credentials =
          GetCredentialsProviderInstance().GetCredentials());
  explicit ClientConfiguration(const CredentialsProvider& provider);

 public:
  // accessor
  const std::string& GetBucket() const { return m_bucket; }
  const std::string& GetZone() const { return m_zone; }
  Http::Host::Value GetHost() const { return m_host; }
  Http::Protocol::Value GetProtocol() const { return m_protocol; }
  uint16_t GetPort() const { return m_port; }
  bool IsDebugCurl() const { return m_debugCurl; }
  const std::string& GetAdditionalAgent() const {
    return m_additionalUserAgent;
  }
  ClientLogLevel::Value GetClientLogLevel() const { return m_logLevel; }
  const std::string& GetClientLogDirectory() const { return m_sdkLogDirectory; }
  uint16_t GetTransactionRetries() const { return m_transactionRetries; }
  uint32_t GetTransactionTimeDuration() const {
    return m_transactionTimeDuration;
  }
  int32_t GetMaxListCount() const { return m_maxListCount; }
  uint16_t GetPoolSize() const { return m_clientPoolSize; }
  uint16_t GetParallelTransfers() const { return m_parallelTransfers; }
  uint32_t GetTransferBufferSizeInMB() const {
    return m_transferBufferSizeInMB;
  }

 private:
  const std::string& GetAccessKeyId() const { return m_accessKeyId; }
  const std::string& GetSecretKey() const { return m_secretKey; }
  void InitializeByOptions();
  friend void ::ClientConfigurationInitializer();
  friend class QSClient;  // for QSClient::StartQSService

 private:
  std::string m_accessKeyId;
  std::string m_secretKey;
  std::string m_bucket;
  std::string m_zone;  // zone or region
  Http::Host::Value m_host;
  Http::Protocol::Value m_protocol;
  uint16_t m_port;
  bool m_debugCurl;
  std::string m_additionalUserAgent;
  ClientLogLevel::Value m_logLevel;
  std::string m_sdkLogDirectory;  // log directory

  uint16_t m_transactionRetries;       // retry times for one transaction
  uint32_t m_transactionTimeDuration;  // default time duration for one
                                       // transaction in milliseconds
  int32_t m_maxListCount;              // max obj count for ls
  uint16_t m_clientPoolSize;           // pool size of client
  uint16_t m_parallelTransfers;        // number of file transfers in parallel
  uint32_t m_transferBufferSizeInMB;   // file transfer buffer size in MB
};

}  // namespace Client
}  // namespace QS


#endif  // QSFS_CLIENT_CLIENTCONFIGURATION_H_
