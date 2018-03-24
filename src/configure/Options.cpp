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

#include "configure/Options.h"

#include <iomanip>
#include <memory>
#include <ostream>
#include <string>

#include "boost/exception/to_string.hpp"

#include "base/LogLevel.h"
#include "base/Size.h"
#include "base/StringUtils.h"
#include "base/Utils.h"
#include "configure/Default.h"

namespace QS {

namespace Configure {

using boost::to_string;
using QS::Configure::Default::GetClientDefaultPoolSize;
using QS::Configure::Default::GetDefaultCredentialsFile;
using QS::Configure::Default::GetDefaultDiskCacheDirectory;
using QS::Configure::Default::GetDefaultDirMode;
using QS::Configure::Default::GetDefaultFileMode;
using QS::Configure::Default::GetDefaultLogDirectory;
using QS::Configure::Default::GetDefaultLogLevelName;
using QS::Configure::Default::GetDefaultHostName;
using QS::Configure::Default::GetDefaultTransactionRetries;
using QS::Configure::Default::GetDefaultPort;
using QS::Configure::Default::GetDefaultProtocolName;
using QS::Configure::Default::GetDefaultParallelTransfers;
using QS::Configure::Default::GetDefaultTransferBufSize;
using QS::Configure::Default::GetDefaultZone;
using QS::Configure::Default::GetMaxCacheSize;
using QS::Configure::Default::GetMaxListObjectsCount;
using QS::Configure::Default::GetMaxStatCount;
using QS::Configure::Default::GetDefaultTransactionTimeDuration;
using QS::Logging::GetLogLevelName;
using QS::Logging::GetLogLevelByName;
using QS::StringUtils::ModeToString;
using QS::Utils::GetProcessEffectiveUserID;
using QS::Utils::GetProcessEffectiveGroupID;
using std::ostream;
using std::string;

// --------------------------------------------------------------------------
Options::Options()
    : m_bucket(),
      m_mountPoint(),
      m_zone(GetDefaultZone()),
      m_credentialsFile(GetDefaultCredentialsFile()),
      m_logDirectory(GetDefaultLogDirectory()),
      m_logLevel(GetLogLevelByName(GetDefaultLogLevelName())),
      m_fileMode(GetDefaultFileMode()),
      m_dirMode(GetDefaultDirMode()),
      m_umaskMountPoint(0),
      m_retries(GetDefaultTransactionRetries()),
      m_requestTimeOut(GetDefaultTransactionTimeDuration()),
      m_maxCacheSizeInMB(GetMaxCacheSize() / QS::Size::MB1),
      m_diskCacheDir(GetDefaultDiskCacheDirectory()),
      m_maxStatCountInK(GetMaxStatCount() / QS::Size::K1),
      m_maxListCount(GetMaxListObjectsCount()),
      m_statExpireInMin(-1),  // default disable state expire
      m_parallelTransfers(GetDefaultParallelTransfers()),
      m_transferBufferSizeInMB(GetDefaultTransferBufSize() /
                               QS::Size::MB1),
      m_clientPoolSize(GetClientDefaultPoolSize()),
      m_host(GetDefaultHostName()),
      m_protocol(GetDefaultProtocolName()),
      m_port(GetDefaultPort(GetDefaultProtocolName())),
      m_additionalAgent(),
      m_clearLogDir(false),
      m_foreground(false),
      m_singleThread(false),
      m_qsfsSingleThread(false),
      m_debug(false),
      m_debugCurl(false),
      m_showHelp(false),
      m_showVersion(false),
      m_allowOther(false),
      m_uid(GetProcessEffectiveUserID()),
      m_gid(GetProcessEffectiveGroupID()),
      m_isOverrideUID(false),
      m_isOverrideGID(false),
      m_umask(0),
      m_fuseArgsInitialized(false) {}

// --------------------------------------------------------------------------
ostream &operator<<(ostream &os, const Options &opts) {
  struct CatArgv {
    int m_argc;
    char **m_argv;

    CatArgv(int argc, char **argv) : m_argc(argc), m_argv(argv) {}

    string operator() () const {
      string str;
      if (m_argc <=0 || m_argv == NULL) {
        return str;
      }
      int i = 0;
      while (i != m_argc) {
        if (i != 0) { str.append(" "); }
        str.append(m_argv[i]);
        ++i;
      }
      return str;
    }
  };

  const struct fuse_args &fuseArg = opts.m_fuseArgs;

  return os
         << "[bucket: " << opts.m_bucket << "] "
         << "[mount point: " << opts.m_mountPoint << "] "
         << "[zone: " << opts.m_zone << "] "
         << "[credentials: " << opts.m_credentialsFile << "] "
         << "[log directory: " << opts.m_logDirectory << "] "
         << "[log level: " << GetLogLevelName(opts.m_logLevel) << "] "
         << "[file mode: " << std::oct << opts.m_fileMode << "] "
         << "[dir mode: " << opts.m_dirMode << "] "
         << "[umask mp: " << opts.m_umaskMountPoint << std::dec << "] "
         << "[retries: " << to_string(opts.m_retries) << "] "
         << "[req timeout(ms): " << to_string(opts.m_requestTimeOut) << "] "
         << "[max cache(MB): " << to_string(opts.m_maxCacheSizeInMB) << "] "
         << "[disk cache dir: " << opts.m_diskCacheDir << "] "
         << "[max stat(K): " << to_string(opts.m_maxStatCountInK) << "] "
         << "[max list: " << to_string(opts.m_maxListCount) << "] "
         << "[stat expire(min): " << to_string(opts.m_statExpireInMin) << "] "
         << "[num transfers: " << to_string(opts.m_parallelTransfers) << "] "
         << "[transfer buf(MB): " << to_string(opts.m_transferBufferSizeInMB) <<"] "  // NOLINT
         << "[pool size: " << to_string(opts.m_clientPoolSize) << "] "
         << "[host: " << opts.m_host << "] "
         << "[protocol: " << opts.m_protocol << "] "
         << "[port: " << to_string(opts.m_port) << "] "
         << "[additional agent: " << opts.m_additionalAgent << "] "
         << std::boolalpha
         << "[enable content md5: " << opts.m_enableContentMD5 << "] "
         << "[clear logdir: " << opts.m_clearLogDir << "] "
         << "[foreground: " << opts.m_foreground << "] "
         << "[FUSE single thread: " << opts.m_singleThread << "] "
         << "[qsfs single thread: " << opts.m_qsfsSingleThread << "] "
         << "[debug: " << opts.m_debug << "] "
         << "[curldbg: " << opts.m_debugCurl << "] "
         << "[show help: " << opts.m_showHelp << "] "
         << "[show version: " << opts.m_showVersion << "] "
         << "[allow other: " << opts.m_allowOther << "] "
         << "[uid: " << to_string(opts.m_uid) << "] "
         << "[gid: " << to_string(opts.m_gid) << "]"
         << "[override uid: " << opts.m_isOverrideUID << "] "
         << "[override gid: " << opts.m_isOverrideGID << "] "
         << "[umask: " << std::oct << opts.m_umask << std::boolalpha << "] "
         << "[fuse_args.argc: " << to_string(fuseArg.argc) << "] "
         << "[fuse_args.argv: " << CatArgv(fuseArg.argc, fuseArg.argv)() << "] "
         << "[fuse_args.allocated: " << static_cast<bool>(fuseArg.allocated) << "] "  // NOLINT
         << std::noboolalpha;
}

}  // namespace Configure
}  // namespace QS
