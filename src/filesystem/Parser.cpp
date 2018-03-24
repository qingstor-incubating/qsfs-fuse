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

#include "filesystem/Parser.h"

#include <assert.h>
#include <stddef.h>  // for offsetof
#include <stdlib.h>  // for strtoul
#include <stdint.h>
#include <string.h>  // for strdup

#include <sys/stat.h>

#include <iostream>
#include <string>

#include "boost/exception/to_string.hpp"
#include "boost/lexical_cast.hpp"

#include "base/Exception.h"
#include "base/LogLevel.h"
#include "base/Size.h"
#include "base/Utils.h"
#include "client/Protocol.h"
#include "configure/Default.h"
#include "configure/IncludeFuse.h"  // for fuse.h
#include "configure/Options.h"

namespace QS {

namespace FileSystem {

using QS::Exception::QSException;

namespace Parser {

namespace {

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
using QS::Utils::GetProcessEffectiveUserID;
using QS::Utils::GetProcessEffectiveGroupID;
using std::string;

void PrintWarnMsg(const char *opt, int invalidVal, int defaultVal,
                  const char *extraMsg = NULL) {
  if (opt == NULL) return;
  std::cerr << "[qsfs] invalid parameter in option " << opt << "="
            << to_string(invalidVal) << ", " << to_string(defaultVal)
            << " is used.";
  if (extraMsg != NULL) {
    std::cerr << " " << extraMsg;
  }
  std::cerr << std::endl;
}

static int String2NCompare(const char *str1, const char *str2) {
  return strncmp(str1, str2, strlen(str2));
}

static struct options {
  // We can't set default values for the char* fields here
  // because fuse_opt_parse would attempt to free() them
  // when user specifies different values on command line.
  const char *bucket;
  const char *mountPoint;
  const char *zone;
  const char *credentials;
  const char *logDirectory;
  const char *logLevel;  // INFO, WARN, ERROR, FATAL
  mode_t fileMode;       // Mode of file
  mode_t dirMode;        // Mode of directory
  mode_t umaskmp;        // umask of mount point
  int retries;           // transaction retries
  int reqtimeout;    // in ms
  int maxcache;      // in MB
  const char *diskdir;
  int maxstat;       // in K
  int maxlist;       // max file count for ls
  int statexpire;    // in mins, negative value disable state expire
  int numtransfer;
  int bufsize;       // in MB
  int threads;
  const char *host;
  const char *protocol;
  int port;
  const char *addtionalAgent;
  int contentMD5;          // default not enable content MD5
  int clearLogDir;         // default not clear log dir
  int foreground;          // default not foreground
  int singleThread;        // default FUSE multi-thread
  int qsSingleThread;      // default qsfs single-thread
  int debug;               // default no debug
  int curldbg;             // default no curl debug msg
  int showHelp;
  int showVersion;

  // some opts for fuse
  bool allowOther;
  uid_t uid;             // User ID of file
  gid_t gid;             // Group ID of file
  mode_t umask;          // umask of permission bits in st_mode

  int qsfsMultiThread;       // internal use only to turn on multi thread
} options;

#define OPTION(t, p) \
  { t, offsetof(struct options, p), 1 }

static const struct fuse_opt optionSpec[] = {
    OPTION("-z=%s", zone),           OPTION("--zone=%s",        zone),
    OPTION("-c=%s", credentials),    OPTION("--credentials=%s", credentials),
    OPTION("-l=%s", logDirectory),   OPTION("--logdir=%s",      logDirectory),
    OPTION("-L=%s", logLevel),       OPTION("--loglevel=%s",    logLevel),
    OPTION("-F=%o", fileMode),       OPTION("--filemode=%o",    fileMode),
    OPTION("-D=%o", dirMode),        OPTION("--dirmode=%o",     dirMode),
    OPTION("-u=%o", umaskmp),        OPTION("--umaskmp=%o",     umaskmp),
    OPTION("-r=%i", retries),        OPTION("--retries=%i",     retries),
    OPTION("-R=%i", reqtimeout),     OPTION("--reqtimeout=%i",  reqtimeout),
    OPTION("-Z=%i", maxcache),       OPTION("--maxcache=%i",    maxcache),
    OPTION("-k=%s", diskdir),        OPTION("--diskdir=%s",     diskdir),
    OPTION("-t=%i", maxstat),        OPTION("--maxstat=%i",     maxstat),
    OPTION("-i=%i", maxlist),        OPTION("--maxlist=%i",     maxlist),
    OPTION("-e=%i", statexpire),     OPTION("--statexpire=%i",  statexpire),
    OPTION("-n=%i", numtransfer),    OPTION("--numtransfer=%i", numtransfer),
    OPTION("-b=%i", bufsize),        OPTION("--bufsize=%i",     bufsize),
    OPTION("-T=%i", threads),        OPTION("--threads=%i",     threads),
    OPTION("-H=%s", host),           OPTION("--host=%s",        host),
    OPTION("-p=%s", protocol),       OPTION("--protocol=%s",    protocol),
    OPTION("-P=%i", port),           OPTION("--port=%i",        port),
    OPTION("-a=%s", addtionalAgent), OPTION("--agent=%s",       addtionalAgent),
    OPTION("-m",    contentMD5),     OPTION("--contentMD5",     contentMD5),
    OPTION("-C",    clearLogDir),    OPTION("--clearlogdir",    clearLogDir),
    OPTION("-f",    foreground),     OPTION("--foreground",     foreground),
    OPTION("-s",    singleThread),   OPTION("--single",         singleThread),
    OPTION("-S",    qsSingleThread), OPTION("--Single",         qsSingleThread),
    OPTION("-d",    debug),          OPTION("--debug",          debug),
    OPTION("-U",    curldbg),        OPTION("--curldbg",        curldbg),
    OPTION("-h",    showHelp),       OPTION("--help",           showHelp),
    OPTION("-V",    showVersion),    OPTION("--version",        showVersion),
    OPTION("-M",    qsfsMultiThread),
    {NULL, 0, 0}
};

// This is repeatedly called by the fuse option parser
// if the key is equal to FUSE_OPT_KEY_OPT, it's an option passed in prefixed by
// '-' or '--' e.g.: -f -d -logdir=/tmp/qsfs_log/
//
// if the key is equal to FUSE_OPT_KEY_NONOPT, it's either the bucket name
//  or the mountpoint. The bucket name will always come before the mountpoint
static int MyFuseOptionProccess(void *data, const char *arg, int key,
                                struct fuse_args *outargs) {
  if (key == FUSE_OPT_KEY_NONOPT) {
    // the first NONOPT option is the bucket name
    if (strlen(options.bucket) == 0) {
      options.bucket = strdup(arg);
      return 0;
    } else if (!strcmp(arg, "qsfs")) {
      return 0;
    }

    // the second NONOPT option is the mountpoint
    if (strlen(options.mountPoint) == 0) {
      options.mountPoint = strdup(arg);
      return 1;  // continue for fuse option
    }

    std::cerr << "[qsfs] specify unknown NONOPT option " << arg << std::endl;
    return -1;  // error
  } else if (key == FUSE_OPT_KEY_OPT) {
    if (strcmp(arg, "allow_other") == 0) {
      options.allowOther = true;
      return 1;  // continue for fuse option
    }

    if (String2NCompare(arg, "uid=") == 0) {
      string str = string(arg).substr(4);
      options.uid = boost::lexical_cast<uid_t>(str);

      return 1;  // continue for fuse option
    }

    if (String2NCompare(arg, "gid=") == 0) {
      string str = string(arg).substr(4);
      options.gid = boost::lexical_cast<gid_t>(str);

      return 1;  // continue for fuse option
    }

    if (String2NCompare(arg, "umask=") == 0) {
      string str = string(arg).substr(6);
      // umask is in octal representation
      options.umask = static_cast<mode_t>(strtoul(str.c_str(), NULL, 8));

      return 1;  // continue for fuse option
    }
  }

  return 1;  // continue for fuse option
}

}  // namespace

void Parse(int argc, char **argv) {
  // Set defaults
  // we have to use strdup so that fuse_opt_parse
  // can free the defaults if other values are specified.
  options.bucket         = strdup("");
  options.mountPoint     = strdup("");
  options.zone           = strdup(GetDefaultZone().c_str());
  options.credentials    = strdup(GetDefaultCredentialsFile().c_str());
  options.logDirectory   = strdup(GetDefaultLogDirectory().c_str());
  options.logLevel       = strdup(GetDefaultLogLevelName().c_str());
  options.fileMode       = GetDefaultFileMode();
  options.dirMode        = GetDefaultDirMode();
  options.umaskmp        = 0;  // default 0000
  options.retries        = GetDefaultTransactionRetries();
  options.reqtimeout     = GetDefaultTransactionTimeDuration();
  options.maxcache       = GetMaxCacheSize() / QS::Size::MB1;
  options.diskdir        = strdup(GetDefaultDiskCacheDirectory().c_str());
  options.maxstat        = GetMaxStatCount() / QS::Size::K1;
  options.maxlist        = GetMaxListObjectsCount();
  options.statexpire     =  -1;
  options.numtransfer    = GetDefaultParallelTransfers();
  options.bufsize        = GetDefaultTransferBufSize() / QS::Size::MB1;
  options.threads        = GetClientDefaultPoolSize();
  options.host           = strdup(GetDefaultHostName().c_str());
  options.protocol       = strdup(GetDefaultProtocolName().c_str());
  options.port           = GetDefaultPort(GetDefaultProtocolName());
  options.addtionalAgent = strdup("");
  options.contentMD5     = 0;
  options.clearLogDir    = 0;
  options.foreground     = 0;
  options.singleThread   = 0;
  options.qsSingleThread = 1;  // default qsfs single
  options.debug          = 0;
  options.curldbg        = 0;
  options.showHelp       = 0;
  options.showVersion    = 0;
  options.qsfsMultiThread = 0;
  options.allowOther     = false;
  options.uid            = GetProcessEffectiveUserID();
  options.gid            = GetProcessEffectiveGroupID();
  options.umask          = 0;  // default 0000

  // Do Parse
  QS::Configure::Options &qsOptions = QS::Configure::Options::Instance();
  qsOptions.SetFuseArgs(argc, argv);

  struct fuse_args &args = qsOptions.GetFuseArgs();
  if (0 != fuse_opt_parse(&args, &options, optionSpec, MyFuseOptionProccess)) {
    throw QSException("Error while parsing command line options.");
  }

  // Store options
  qsOptions.SetBucket(options.bucket);
  qsOptions.SetMountPoint(options.mountPoint);
  qsOptions.SetZone(options.zone);
  qsOptions.SetCredentialsFile(options.credentials);
  qsOptions.SetLogDirectory(options.logDirectory);
  qsOptions.SetLogLevel(QS::Logging::GetLogLevelByName(options.logLevel));
  options.fileMode &= (S_IRWXU | S_IRWXG | S_IRWXO);
  qsOptions.SetFileMode(options.fileMode);
  options.dirMode &= (S_IRWXU | S_IRWXG | S_IRWXO);
  qsOptions.SetDirMode(options.dirMode);
  options.umaskmp &= (S_IRWXU | S_IRWXG | S_IRWXO);
  if (options.umaskmp != 0) {
    qsOptions.SetUmaskMountPoint(options.umaskmp);
  }

  if (options.retries <= 0) {
    PrintWarnMsg("-r|--retries", options.retries, GetDefaultTransactionRetries());
    qsOptions.SetRetries(GetDefaultTransactionRetries());
  } else {
    qsOptions.SetRetries(options.retries);
  }

  if (options.reqtimeout <= 0) {
    PrintWarnMsg("-R|--reqtimeout", options.reqtimeout,
                 GetDefaultTransactionTimeDuration());
    qsOptions.SetRequestTimeOut(GetDefaultTransactionTimeDuration());
  } else {
    qsOptions.SetRequestTimeOut(options.reqtimeout);
  }

  if (options.maxcache <= 0) {
    PrintWarnMsg("-Z|--maxcache", options.maxcache,
                 GetMaxCacheSize() / QS::Size::MB1);
    qsOptions.SetMaxCacheSizeInMB(GetMaxCacheSize() / QS::Size::MB1);
  } else {
    qsOptions.SetMaxCacheSizeInMB(options.maxcache);
  }

  qsOptions.SetDiskCacheDirectory(options.diskdir);

  if (options.maxstat <= 0) {
    PrintWarnMsg("-t|--maxstat", options.maxstat,
                 GetMaxStatCount() / QS::Size::K1);
    qsOptions.SetMaxStatCountInK(GetMaxStatCount() / QS::Size::K1);
  } else {
    qsOptions.SetMaxStatCountInK(options.maxstat);
  }

  qsOptions.SetMaxListCount(options.maxlist);
  qsOptions.SetStatExpireInMin(options.statexpire);

  if (options.numtransfer <= 0) {
    PrintWarnMsg("-n|--numtransfer", options.numtransfer,
                 GetDefaultParallelTransfers());
    qsOptions.SetParallelTransfers(GetDefaultParallelTransfers());
  } else {
    qsOptions.SetParallelTransfers(options.numtransfer);
  }

  if (options.bufsize <= 0) {
    PrintWarnMsg("-b|--bufsize", options.bufsize,
                 GetDefaultTransferBufSize() / QS::Size::MB1);
    qsOptions.SetTransferBufferSizeInMB(GetDefaultTransferBufSize() /
                                        QS::Size::MB1);
  } else {
    qsOptions.SetTransferBufferSizeInMB(options.bufsize);
  }

  if (options.threads <= 0) {
    PrintWarnMsg("-T|--threads", options.threads, GetClientDefaultPoolSize());
    qsOptions.SetClientPoolSize(GetClientDefaultPoolSize());
  } else {
    qsOptions.SetClientPoolSize(options.threads);
  }

  qsOptions.SetHost(options.host);
  qsOptions.SetProtocol(options.protocol);

  if (options.port <= 0) {
    PrintWarnMsg("-P|--port", options.port,
                 GetDefaultPort(GetDefaultProtocolName()));
    qsOptions.SetPort(GetDefaultPort(GetDefaultProtocolName()));
  } else {
    qsOptions.SetPort(options.port);
  }

  qsOptions.SetAdditionalAgent(options.addtionalAgent);
  qsOptions.SetEnableContentMD5(options.contentMD5 !=0);
  qsOptions.SetClearLogDir(options.clearLogDir != 0);
  qsOptions.SetForeground(options.foreground != 0);
  qsOptions.SetSingleThread(options.singleThread != 0);
  qsOptions.SetQsfsSingleThread(options.qsSingleThread != 0);
  if (options.qsfsMultiThread != 0) {
    qsOptions.SetQsfsSingleThread(false);
  }
  qsOptions.SetDebug(options.debug != 0);
  qsOptions.SetDebugCurl(options.curldbg != 0);
  qsOptions.SetShowHelp(options.showHelp != 0);
  qsOptions.SetShowVerion(options.showVersion !=0);

  qsOptions.SetAllowOther(options.allowOther);

  if (GetProcessEffectiveUserID() != 0 && options.uid == 0) {
    PrintWarnMsg("-o uid", options.uid, GetProcessEffectiveUserID(),
                 "Only root user can specify uid=0.");
    qsOptions.SetUID(GetProcessEffectiveUserID());
  } else {
    qsOptions.SetUID(options.uid);
    qsOptions.SetOverrideUID(true);
  }

  if (GetProcessEffectiveGroupID() != 0 && options.gid == 0) {
    PrintWarnMsg("-o gid", options.gid, GetProcessEffectiveGroupID(),
                 "Only root user can specify gid=0.");
    qsOptions.SetGID(GetProcessEffectiveGroupID());
  } else {
    qsOptions.SetGID(options.gid);
    qsOptions.SetOverrideGID(true);
  }

  options.umask &= (S_IRWXU | S_IRWXG | S_IRWXO);
  if (umask != 0) {
    qsOptions.SetUmask(options.umask);
  }


  // Let MyFuseOptionProccess return 1 for mountpoint to keep it as first arg
  // if (!qsOptions.GetMountPoint().empty()) {
  // assert(fuse_opt_add_arg(&args, qsOptions.GetMountPoint().c_str()) == 0);
  // }
  if (qsOptions.IsShowHelp()) {
    assert(fuse_opt_add_arg(&args, "-ho") == 0);  // without FUSE usage line
  }
  if (qsOptions.IsShowVersion()) {
    assert(fuse_opt_add_arg(&args, "--version") == 0);
  }
  if (qsOptions.IsForeground()) {
    assert(fuse_opt_add_arg(&args, "-f") == 0);
  }
  if (qsOptions.IsSingleThread()) {
    assert(fuse_opt_add_arg(&args, "-s") == 0);
  }
  if (qsOptions.IsDebug()) {
    assert(fuse_opt_add_arg(&args, "-d") == 0);
  }
}

}  // namespace Parser
}  // namespace FileSystem
}  // namespace QS
