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

#ifndef QSFS_CONFIGURE_OPTIONS_H_
#define QSFS_CONFIGURE_OPTIONS_H_

#include <stdint.h>  // for uint16_t

#include <sys/stat.h>

#include <ostream>
#include <string>

#include "base/LogLevel.h"
#include "base/Singleton.hpp"
#include "configure/IncludeFuse.h"  // for fuse.h

namespace QS {

namespace FileSystem {
class Mounter;

namespace Parser {
void Parse(int argc, char **argv);

}  // namespace Parser
}  // namespace FileSystem

namespace Configure {

using QS::Logging::LogLevel;

class Options : public Singleton<Options> {
 public:
  ~Options() {
    if (m_fuseArgsInitialized) {
      fuse_opt_free_args(&m_fuseArgs);
    }
  }

 public:
  bool IsNoMount() const { return m_showHelp || m_showVersion; }

  // accessor
  const std::string &GetBucket() const { return m_bucket; }
  const std::string &GetMountPoint() const { return m_mountPoint; }
  const std::string &GetZone() const { return m_zone; }
  const std::string &GetCredentialsFile() const { return m_credentialsFile; }
  const std::string &GetLogDirectory() const { return m_logDirectory; }
  LogLevel::Value GetLogLevel() const { return m_logLevel; }
  mode_t GetFileMode() const { return m_fileMode; }
  mode_t GetDirMode() const { return m_dirMode; }
  mode_t GetUmaskMountPoint() const { return m_umaskMountPoint; }
  bool IsUmaskMountPoint() const {
    return (m_umaskMountPoint & (S_IRWXU | S_IRWXG | S_IRWXO)) != 0;
  }
  uint16_t GetRetries() const { return m_retries; }
  uint32_t GetRequestTimeOut() const { return m_requestTimeOut; }
  uint32_t GetMaxCacheSizeInMB() const { return m_maxCacheSizeInMB; }
  const std::string &GetDiskCacheDirectory() const { return m_diskCacheDir; }
  uint32_t GetMaxStatCountInK() const { return m_maxStatCountInK; }
  int32_t GetMaxListCount() const { return m_maxListCount; }
  int32_t GetStatExpireInMin() const { return m_statExpireInMin; }
  uint16_t GetParallelTransfers() const { return m_parallelTransfers; }
  uint32_t GetTransferBufferSizeInMB() const {
    return m_transferBufferSizeInMB;
  }
  uint16_t GetClientPoolSize() const { return m_clientPoolSize; }
  const std::string &GetHost() const { return m_host; }
  const std::string &GetProtocol() const { return m_protocol; }
  uint16_t GetPort() const { return m_port; }
  const std::string &GetAdditionalAgent() const { return m_additionalAgent; }
  bool IsEnableContentMD5() const { return m_enableContentMD5; }
  bool IsClearLogDir() const { return m_clearLogDir; }
  bool IsForeground() const { return m_foreground; }
  bool IsSingleThread() const { return m_singleThread; }
  bool IsQsfsSingleThread() const { return m_qsfsSingleThread; }
  bool IsDebug() const { return m_debug; }
  bool IsDebugCurl() const { return m_debugCurl; }
  bool IsShowHelp() const { return m_showHelp; }
  bool IsShowVersion() const { return m_showVersion; }
  bool IsAllowOther() const { return m_allowOther; }
  uid_t GetUID() const { return m_uid; }
  gid_t GetGID() const { return m_gid; }
  bool IsOverrideUID() const { return m_isOverrideUID; }
  bool IsOverrideGID() const { return m_isOverrideGID; }
  mode_t GetUmask() const { return m_umask; }
  bool IsUmask() const {
    return (m_umask & (S_IRWXU | S_IRWXG | S_IRWXO)) != 0;
  }

 private:
  Options();

  struct fuse_args &GetFuseArgs() {
    return m_fuseArgs;
  }

  // mutator
  void SetBucket(const char *bucket) { m_bucket = bucket; }
  void SetMountPoint(const char *path) { m_mountPoint = path; }
  void SetZone(const char *zone) { m_zone = zone; }
  void SetCredentialsFile(const char *file) { m_credentialsFile = file; }
  void SetLogDirectory(const std::string &path) { m_logDirectory = path; }
  void SetLogLevel(LogLevel::Value level) { m_logLevel = level; }
  void SetFileMode(mode_t fileMode) { m_fileMode = fileMode; }
  void SetDirMode(mode_t dirMode) { m_dirMode = dirMode; }
  void SetUmaskMountPoint(mode_t umask) { m_umaskMountPoint = umask; }
  void SetRetries(unsigned retries) { m_retries = retries; }
  void SetRequestTimeOut(uint32_t timeout) { m_requestTimeOut = timeout; }
  void SetMaxCacheSizeInMB(uint32_t maxcache) { m_maxCacheSizeInMB = maxcache; }
  void SetDiskCacheDirectory(const char *diskdir) { m_diskCacheDir = diskdir; }
  void SetMaxStatCountInK(uint32_t maxstat) { m_maxStatCountInK = maxstat; }
  void SetMaxListCount(int32_t maxlist) { m_maxListCount = maxlist; }
  void SetStatExpireInMin(int32_t expire) { m_statExpireInMin = expire; }
  void SetParallelTransfers(unsigned numtransfers) {
    m_parallelTransfers = numtransfers;
  }
  void SetTransferBufferSizeInMB(uint32_t bufsize) {
    m_transferBufferSizeInMB = bufsize;
  }
  void SetClientPoolSize(uint32_t poolsize) { m_clientPoolSize = poolsize; }
  void SetHost(const char *host) { m_host = host; }
  void SetProtocol(const char *protocol) { m_protocol = protocol; }
  void SetPort(unsigned port) { m_port = port; }
  void SetAdditionalAgent(const char *agent) { m_additionalAgent = agent; }
  void SetEnableContentMD5(bool contentMD5) { m_enableContentMD5 = contentMD5; }
  void SetClearLogDir(bool clearLogDir) { m_clearLogDir = clearLogDir; }
  void SetForeground(bool foreground) { m_foreground = foreground; }
  void SetSingleThread(bool singleThread) { m_singleThread = singleThread; }
  void SetQsfsSingleThread(bool singleThread) {
    m_qsfsSingleThread = singleThread;
  }
  void SetDebug(bool debug) { m_debug = debug; }
  void SetDebugCurl(bool debug) { m_debugCurl = debug; }
  void SetShowHelp(bool showHelp) { m_showHelp = showHelp; }
  void SetShowVerion(bool showVersion) { m_showVersion = showVersion; }
  void SetAllowOther(bool allowOther) { m_allowOther = allowOther; }
  void SetUID(uid_t uid) { m_uid = uid; }
  void SetGID(gid_t gid) { m_gid = gid; }
  void SetOverrideUID(bool overrideUID) { m_isOverrideUID = overrideUID; }
  void SetOverrideGID(bool overrideGID) { m_isOverrideGID = overrideGID; }
  void SetUmask(mode_t umask) { m_umask = umask; }
  void SetFuseArgs(int argc, char **argv) {
    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
    m_fuseArgs = args;
    m_fuseArgsInitialized = true;
  }

  std::string m_bucket;
  std::string m_mountPoint;
  std::string m_zone;
  std::string m_credentialsFile;
  std::string m_logDirectory;
  LogLevel::Value m_logLevel;
  mode_t m_fileMode;
  mode_t m_dirMode;
  mode_t m_umaskMountPoint;
  uint16_t m_retries;         // transaction retries
  uint32_t m_requestTimeOut;  // in milliseconds
  uint32_t m_maxCacheSizeInMB;
  std::string m_diskCacheDir;
  uint32_t m_maxStatCountInK;
  int32_t m_maxListCount;        // negative value will list all files for ls
  int32_t m_statExpireInMin;     //  negative value will disable state expire
  uint16_t m_parallelTransfers;  // count of file transfers in parallel
  uint32_t m_transferBufferSizeInMB;
  uint16_t m_clientPoolSize;
  std::string m_host;
  std::string m_protocol;
  uint16_t m_port;
  std::string m_additionalAgent;
  bool m_enableContentMD5;
  bool m_clearLogDir;
  bool m_foreground;        // FUSE foreground option
  bool m_singleThread;      // FUSE single threaded option
  bool m_qsfsSingleThread;  // qsfs single threaded option
  bool m_debug;
  bool m_debugCurl;
  bool m_showHelp;
  bool m_showVersion;

  // Fuse opts
  bool m_allowOther;
  uid_t m_uid;
  gid_t m_gid;
  bool m_isOverrideUID;
  bool m_isOverrideGID;
  mode_t m_umask;
  struct fuse_args m_fuseArgs;
  bool m_fuseArgsInitialized;

  friend class Singleton<Options>;
  friend class QS::FileSystem::Mounter;  // for DoMount
  friend void QS::FileSystem::Parser::Parse(int argc, char **argv);
  friend std::ostream &operator<<(std::ostream &os, const Options &opts);
};

std::ostream &operator<<(std::ostream &os, const Options &opts);

}  // namespace Configure
}  // namespace QS

#endif  // QSFS_CONFIGURE_OPTIONS_H_
