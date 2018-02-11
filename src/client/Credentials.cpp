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

#include "client/Credentials.h"

#include <assert.h>
#include <errno.h>
#include <string.h>

#include <sys/stat.h>

#include <fstream>
#include <string>
#include <utility>

#include "boost/bind.hpp"
#include "boost/make_shared.hpp"
#include "boost/shared_ptr.hpp"
#include "boost/thread/once.hpp"

#include "base/Exception.h"
#include "base/LogMacros.h"
#include "base/StringUtils.h"
#include "base/UtilsWithLog.h"
#include "configure/Options.h"

namespace QS {

namespace Client {

using boost::bind;
using boost::call_once;
using boost::make_shared;
using boost::once_flag;
using boost::shared_ptr;
using QS::Exception::QSException;
using QS::StringUtils::FormatPath;
using std::ifstream;
using std::make_pair;
using std::pair;
using std::string;

static shared_ptr<CredentialsProvider> credentialsProvider;
static once_flag credentialsProviderOnceFlag = BOOST_ONCE_INIT;

namespace {

pair<bool, string> ErrorOut(const string& str) { return make_pair(false, str); }
pair<bool, string> CheckCredentialsFilePermission(const string& file);

void SetProvider(const shared_ptr<CredentialsProvider>& provider) {
  credentialsProvider = provider;
}

void BuildDefaultProvider() {
  credentialsProvider = make_shared<DefaultCredentialsProvider>(
      QS::Configure::Options::Instance().GetCredentialsFile());
}

}  // namespace

// --------------------------------------------------------------------------
void InitializeCredentialsProvider(
    const shared_ptr<CredentialsProvider>& provider) {
  assert(provider);
  call_once(credentialsProviderOnceFlag,
            bind(boost::type<void>(), SetProvider, provider));
}

// --------------------------------------------------------------------------
CredentialsProvider& GetCredentialsProviderInstance() {
  call_once(credentialsProviderOnceFlag, BuildDefaultProvider);
  return *credentialsProvider.get();
}

// --------------------------------------------------------------------------
DefaultCredentialsProvider::DefaultCredentialsProvider(
    const string& credentialFile)
    : m_credentialsFile(credentialFile) {
  pair<bool, string> outcome = ReadCredentialsFile(credentialFile);
  if (!outcome.first) {
    throw QSException(outcome.second);
  }
}

// --------------------------------------------------------------------------
Credentials DefaultCredentialsProvider::GetCredentials() const {
  if (!HasDefaultKey()) {
    throw QSException(
        "Fail to fetch default credentials which is not existing");
  }

  return Credentials(m_defaultAccessKeyId, m_defaultSecretKey);
}

// --------------------------------------------------------------------------
Credentials DefaultCredentialsProvider::GetCredentials(
    const std::string& bucket) const {
  BucketToKeyPairMapConstIterator it = m_bucketMap.find(bucket);
  if (it == m_bucketMap.end()) {
    throw QSException("Fail to fetch access key for bucket " + bucket +
                      "which is not found in credentials file " +
                      FormatPath(m_credentialsFile));
  }
  return Credentials(it->second.first, it->second.second);
}

// --------------------------------------------------------------------------
pair<bool, string> DefaultCredentialsProvider::ReadCredentialsFile(
    const std::string& file) {
  if (file.empty()) {
    return ErrorOut("Credentials file is not specified");
  }

  bool success = true;
  string errMsg;

  if (QS::UtilsWithLog::FileExists(file)) {
    // Check credentials file permission
    pair<bool, string> outcome = CheckCredentialsFilePermission(file);
    if (!outcome.first) return outcome;

    // Check if have permission to read credetials file
    if (!QS::UtilsWithLog::HavePermission(file)) {
      return ErrorOut("Credentials file permisson denied " + FormatPath(file));
    }

    // Read credentials
    static const char* invalidChars = " \t";  // Not allow space and tab
    static const char DELIM = ':';

    ifstream credentials(file.c_str());
    if (credentials) {
      string line;
      string::size_type firstPos = string::npos;
      string::size_type lastPos = string::npos;
      while (std::getline(credentials, line)) {
        if (line[0] == '#' || line.empty()) continue;
        if (line[line.size() - 1] == '\r') {
          line.erase(line.size() - 1, 1);
          if (line.empty()) continue;
        }
        if (line[0] == '[') {
          return ErrorOut(
              "Invalid line starting with a bracket \"[\" is found in "
              "credentials file " +
              FormatPath(file));
        }
        if (line.find_first_of(invalidChars) != string::npos) {
          return ErrorOut(
              "Invalid line with whitespace or tab is found in credentials "
              "file " +
              FormatPath(file));
        }

        firstPos = line.find_first_of(DELIM);
        if (firstPos == string::npos) {
          return ErrorOut(
              "Invalid line with no \":\" seperator is found in credentials "
              "file " +
              FormatPath(file));
        }
        lastPos = line.find_last_of(DELIM);

        if (firstPos == lastPos) {  // Found default key
          if (HasDefaultKey()) {
            DebugWarning(
                "More than one default key pairs are provided in credentials "
                "file " +
                FormatPath(file) + ". Only set with the first one");
            continue;
          } else {
            SetDefaultKey(line.substr(0, firstPos), line.substr(firstPos + 1));
          }
        } else {  // Found bucket specified key
          pair<BucketToKeyPairMapIterator, bool> res =
              m_bucketMap.emplace(line.substr(0, firstPos),
                                  make_pair(line.substr(firstPos + 1, lastPos),
                                            line.substr(lastPos + 1)));
          if(!res.second) {
            return ErrorOut("Fail to store key pair" + FormatPath(file));
          }
        }
      }
    } else {
      return ErrorOut("Fail to read credentilas file : " +
                      string(strerror(errno)) + FormatPath(file));
    }
  } else {
    return ErrorOut("Credentials file not exist " + FormatPath(file));
  }

  return make_pair(success, errMsg);
}

namespace {

pair<bool, string> CheckCredentialsFilePermission(const string& file) {
  if (file.empty()) {
    return ErrorOut("Credentials file is not specified");
  }

  struct stat st;
  if (stat(file.c_str(), &st) != 0) {
    return ErrorOut("Unable to read credentials file : " +
                    string(strerror(errno)) + FormatPath(file));
  }
  if ((st.st_mode & S_IROTH) || (st.st_mode & S_IWOTH) ||
      (st.st_mode & S_IXOTH)) {
    return ErrorOut("Credentials file should not have others permissions " +
                    FormatPath(file));
  }
  if ((st.st_mode & S_IRGRP) || (st.st_mode & S_IWGRP) ||
      (st.st_mode & S_IXGRP)) {
    return ErrorOut("Credentials file should not have group permissions " +
                    FormatPath(file));
  }
  if ((st.st_mode & S_IXUSR)) {
    return ErrorOut("Credentials file should not have executable permissions " +
                    FormatPath(file));
  }

  return make_pair(true, "");
}

}  // namespace

}  // namespace Client
}  // namespace QS
