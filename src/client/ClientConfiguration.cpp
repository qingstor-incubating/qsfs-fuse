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

#include "client/ClientConfiguration.h"

#include <assert.h>
#include <errno.h>
#include <string.h>  // for strerr

#include <string>
#include <utility>

#include "boost/bind.hpp"
#include "boost/make_shared.hpp"
#include "boost/shared_ptr.hpp"
#include "boost/thread/once.hpp"

#include "base/Exception.h"
#include "base/Size.h"
#include "base/StringUtils.h"
#include "base/Utils.h"
#include "client/Credentials.h"
#include "client/Protocol.h"
#include "client/URI.h"
#include "configure/Default.h"
#include "configure/Options.h"

namespace QS {

namespace Client {

using boost::call_once;
using boost::shared_ptr;
using QS::Exception::QSException;
using QS::Configure::Default::GetClientDefaultPoolSize;
using QS::Configure::Default::GetDefaultLogDirectory;
using QS::Configure::Default::GetDefaultTransactionRetries;
using QS::Configure::Default::GetDefaultParallelTransfers;
using QS::Configure::Default::GetDefaultTransferBufSize;
using QS::Configure::Default::GetDefaultHostName;
using QS::Configure::Default::GetDefaultPort;
using QS::Configure::Default::GetDefaultProtocolName;
using QS::Configure::Default::GetDefaultZone;
using QS::Configure::Default::GetDefineFileMode;
using QS::Configure::Default::GetMaxListObjectsCount;
using QS::Configure::Default::GetSDKLogFolderBaseName;
using QS::Configure::Default::GetDefaultTransactionTimeDuration;
using QS::StringUtils::FormatPath;
using QS::Utils::AppendPathDelim;
using std::string;

// --------------------------------------------------------------------------
string GetClientLogLevelName(ClientLogLevel::Value level) {
  string name;
  switch (level) {
    case ClientLogLevel::Debug:
      name = "debug";
      break;
    case ClientLogLevel::Info:
      name = "info";
      break;
    case ClientLogLevel::Warn:
      name = "warning";
      break;
    case ClientLogLevel::Error:
      name = "error";
      break;
    case ClientLogLevel::Fatal:
      name = "fatal";
      break;
    default:
      break;
  }
  return name;
}

// --------------------------------------------------------------------------
ClientLogLevel::Value GetClientLogLevelByName(const string &name) {
  ClientLogLevel::Value level = ClientLogLevel::Warn;
  if (name.empty()) {
    return level;
  }

  string name_lowercase = QS::StringUtils::ToLower(name);
  if (name_lowercase == "debug") {
    level = ClientLogLevel::Debug;
  } else if (name_lowercase == "info") {
    level = ClientLogLevel::Info;
  } else if (name_lowercase == "error") {
    level = ClientLogLevel::Error;
  } else if (name_lowercase == "fatal") {
    level = ClientLogLevel::Fatal;
  }

  return level;
}

static shared_ptr<ClientConfiguration> clientConfigInstance;
static boost::once_flag clientConfigFlag = BOOST_ONCE_INIT;

namespace {

void SetClientConfigInstance(const shared_ptr<ClientConfiguration> &config) {
  clientConfigInstance = config;
}

void ConstructClientConfigInstance() {
  clientConfigInstance =
      shared_ptr<ClientConfiguration>(new ClientConfiguration);
}

}  // namespace

// --------------------------------------------------------------------------
void InitializeClientConfiguration(
    const shared_ptr<ClientConfiguration> &config) {
  assert(config);
  call_once(clientConfigFlag,
            bind(boost::type<void>(), SetClientConfigInstance, config));
}

// --------------------------------------------------------------------------
ClientConfiguration &ClientConfiguration::Instance() {
  call_once(clientConfigFlag, ConstructClientConfigInstance);
  return *clientConfigInstance.get();
}

// --------------------------------------------------------------------------
ClientConfiguration::ClientConfiguration(const Credentials &credentials)
    : m_accessKeyId(credentials.GetAccessKeyId()),
      m_secretKey(credentials.GetSecretKey()),
      m_zone(GetDefaultZone()),
      m_host(QS::Client::Http::StringToHost(GetDefaultHostName())),
      m_protocol(QS::Client::Http::StringToProtocol(GetDefaultProtocolName())),
      m_port(GetDefaultPort(GetDefaultProtocolName())),
      m_debugCurl(false),
      m_additionalUserAgent(std::string()),
      m_logLevel(ClientLogLevel::Warn),
      m_sdkLogDirectory(AppendPathDelim(GetDefaultLogDirectory()) +
                        GetSDKLogFolderBaseName()),
      m_transactionRetries(GetDefaultTransactionRetries()),
      m_transactionTimeDuration(GetDefaultTransactionTimeDuration()),
      m_maxListCount(GetMaxListObjectsCount()),
      m_clientPoolSize(GetClientDefaultPoolSize()),
      m_parallelTransfers(GetDefaultParallelTransfers()),
      m_transferBufferSizeInMB(GetDefaultTransferBufSize() / QS::Size::MB1) {}

// --------------------------------------------------------------------------
ClientConfiguration::ClientConfiguration(const CredentialsProvider &provider)
    : m_accessKeyId(provider.GetCredentials().GetAccessKeyId()),
      m_secretKey(provider.GetCredentials().GetSecretKey()),
      m_zone(GetDefaultZone()),
      m_host(QS::Client::Http::StringToHost(GetDefaultHostName())),
      m_protocol(QS::Client::Http::StringToProtocol(GetDefaultProtocolName())),
      m_port(GetDefaultPort(GetDefaultProtocolName())),
      m_debugCurl(false),
      m_additionalUserAgent(std::string()),
      m_logLevel(ClientLogLevel::Warn),
      m_sdkLogDirectory(AppendPathDelim(GetDefaultLogDirectory()) +
                        GetSDKLogFolderBaseName()),
      m_transactionRetries(GetDefaultTransactionRetries()),
      m_transactionTimeDuration(GetDefaultTransactionTimeDuration()),
      m_maxListCount(GetMaxListObjectsCount()),
      m_clientPoolSize(GetClientDefaultPoolSize()),
      m_parallelTransfers(GetDefaultParallelTransfers()),
      m_transferBufferSizeInMB(GetDefaultTransferBufSize() / QS::Size::MB1) {}

// --------------------------------------------------------------------------
void ClientConfiguration::InitializeByOptions() {
  const QS::Configure::Options &options = QS::Configure::Options::Instance();
  m_bucket = options.GetBucket();
  m_zone = options.GetZone();
  m_host = Http::StringToHost(options.GetHost());
  m_protocol = Http::StringToProtocol(options.GetProtocol());
  m_port = options.GetPort();
  m_debugCurl = options.IsDebugCurl();
  m_additionalUserAgent = options.GetAdditionalAgent();
  m_logLevel = static_cast<ClientLogLevel::Value>(options.GetLogLevel());
  if (options.IsDebug()) {
    m_logLevel = ClientLogLevel::Debug;
  }
  if (options.IsDebugCurl()) {
    // qingstor sdk will turn curl if set level to be verbose
    m_logLevel = ClientLogLevel::Verbose;
  }

  if (!QS::Utils::CreateDirectoryIfNotExists(options.GetLogDirectory())) {
    throw QSException(string("Unable to create log directory : ") +
                      strerror(errno) + " " +
                      FormatPath(options.GetLogDirectory()));
  }

  m_sdkLogDirectory =
      AppendPathDelim(options.GetLogDirectory()) + GetSDKLogFolderBaseName();
  if (!QS::Utils::CreateDirectoryIfNotExists(m_sdkLogDirectory)) {
    throw QSException(string("Unable to create sdk log directory : ") +
                      strerror(errno) + " " + FormatPath(m_sdkLogDirectory));
  }

  m_transactionRetries = options.GetRetries();
  m_transactionTimeDuration = options.GetRequestTimeOut();
  m_maxListCount = options.GetMaxListCount();
  m_clientPoolSize = options.GetClientPoolSize();
  m_parallelTransfers = options.GetParallelTransfers();
  m_transferBufferSizeInMB = options.GetTransferBufferSizeInMB();
}

}  // namespace Client
}  // namespace QS
