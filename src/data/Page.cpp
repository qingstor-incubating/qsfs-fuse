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

#include "data/Page.h"

#include <assert.h>

#include <fstream>
#include <sstream>
#include <string>

#include "base/LogMacros.h"
#include "base/StringUtils.h"
#include "base/Utils.h"
#include "base/UtilsWithLog.h"
#include "configure/Options.h"
#include "data/IOStream.h"
#include "data/StreamUtils.h"

#include "boost/exception/to_string.hpp"
#include "boost/make_shared.hpp"
#include "boost/noncopyable.hpp"
#include "boost/shared_ptr.hpp"
#include "boost/thread/locks.hpp"
#include "boost/thread/recursive_mutex.hpp"

namespace QS {

namespace Data {

using boost::lock_guard;
using boost::make_shared;
using boost::recursive_mutex;
using boost::shared_ptr;
using boost::to_string;
using QS::Data::IOStream;
using QS::Data::StreamUtils::GetStreamSize;
using QS::StringUtils::FormatPath;
using QS::StringUtils::PointerAddress;
using QS::Utils::GetDirName;
using QS::UtilsWithLog::CreateDirectoryIfNotExists;
using QS::UtilsWithLog::FileExists;
using std::fstream;
using std::iostream;
using std::stringstream;
using std::string;

namespace {

// RAII class to open file and close it in destructor
class FileOpener : private boost::noncopyable {
 public:
  explicit FileOpener(const shared_ptr<iostream> &body) {
    m_file = dynamic_cast<fstream *>(body.get());
  }

  ~FileOpener() {
    if (m_file != NULL && m_file->is_open()) {
      m_file->flush();
      m_file->close();
    }
  }

  void DoOpen(const string &filename, std::ios_base::openmode mode) {
    if (m_file != NULL && !filename.empty()) {
      m_file->open(filename.c_str(), mode);
      if (!m_file->is_open()) {
        DebugError("Fail to open file " + FormatPath(filename));
      }
    } else {
      DebugError("Invalid parameter");
    }
  }

 private:
  FileOpener() : m_file(NULL) {}

  fstream *m_file;
};

}  // namespace

// --------------------------------------------------------------------------
Page::Page(off_t offset, size_t len, const char *buffer)
    : m_offset(offset), m_size(len), m_body(make_shared<IOStream>(len)) {
  bool isValidInput = offset >= 0 && len >= 0 && buffer != NULL;
  assert(isValidInput);
  if (!isValidInput) {
    DebugError("Try to new a page with invalid input " +
               ToStringLine(offset, len, buffer));
    return;
  }

  lock_guard<recursive_mutex> lock(m_mutex);
  UnguardedPutToBody(offset, len, buffer);
}

// --------------------------------------------------------------------------
Page::Page(off_t offset, size_t len, const char *buffer, const string &diskfile)
    : m_offset(offset), m_size(len), m_diskFile(diskfile) {
  bool isValidInput = offset >= 0 && len >= 0 && buffer != NULL;
  assert(isValidInput);
  if (!isValidInput) {
    DebugError("Try to new a page with invalid input " +
               ToStringLine(offset, len, buffer));
    return;
  }

  lock_guard<recursive_mutex> lock(m_mutex);
  if (SetupDiskFile()) {
    UnguardedPutToBody(offset, len, buffer);
  }
}

// --------------------------------------------------------------------------
Page::Page(off_t offset, size_t len, const shared_ptr<iostream> &instream)
    : m_offset(offset), m_size(len), m_body(make_shared<IOStream>(len)) {
  bool isValidInput = offset >= 0 && len >= 0 && instream;
  assert(isValidInput);
  if (!isValidInput) {
    DebugError("Try to new a page with invalid input " +
               ToStringLine(offset, len));
    return;
  }

  lock_guard<recursive_mutex> lock(m_mutex);
  UnguardedPutToBody(offset, len, instream);
}

// --------------------------------------------------------------------------
Page::Page(off_t offset, size_t len, const shared_ptr<iostream> &instream,
           const string &diskfile)
    : m_offset(offset), m_size(len), m_diskFile(diskfile) {
  bool isValidInput = offset >= 0 && len > 0 && instream;
  assert(isValidInput);
  if (!isValidInput) {
    DebugError("Try to new a page with invalid input " +
               ToStringLine(offset, len));
    return;
  }

  lock_guard<recursive_mutex> lock(m_mutex);
  if (SetupDiskFile()) {
    UnguardedPutToBody(offset, len, instream);
  }
}

// --------------------------------------------------------------------------
bool Page::UseDiskFile() {
  lock_guard<recursive_mutex> lock(m_mutex);
  return !m_diskFile.empty();
}

// --------------------------------------------------------------------------
bool Page::UseDiskFileNoLock() {
  return !m_diskFile.empty();
}

// --------------------------------------------------------------------------
void Page::UnguardedPutToBody(off_t offset, size_t len, const char *buffer) {
  if (!m_body) {
    DebugError("null body stream " + ToStringLine(offset, len, buffer));
    return;
  }
  if(len ==0){
    return;
  }
  FileOpener opener(m_body);
  if (UseDiskFileNoLock()) {
    // Notice: need to open in both output and input mode to avoid truncate file
    // as open in out mode only will actually truncate file.
    opener.DoOpen(m_diskFile, std::ios_base::binary | std::ios_base::ate |
                  std::ios_base::in | std::ios_base::out);  // open for write
    m_body->seekp(m_offset, std::ios_base::beg);
  } else {
    m_body->seekp(0, std::ios_base::beg);
  }
  if (!m_body->good()) {
    DebugError("Fail to seek body " + ToStringLine(offset, len, buffer));
  } else {
    m_body->write(buffer, len);
    if (!m_body->good()) {
      DebugError("Fail to write buffer " + ToStringLine(offset, len, buffer));
    }
  }
}

// --------------------------------------------------------------------------
void Page::UnguardedPutToBody(off_t offset, size_t len,
                              const shared_ptr<iostream> &instream) {
  if(len == 0) {
    return;
  }
  size_t instreamLen = GetStreamSize(instream);
  if (instreamLen < len) {
    DebugWarning(
        "Input stream buffer len is less than parameter 'len', Ajust it");
    len = instreamLen;
    m_size = instreamLen;
  }

  FileOpener opener(m_body);
  if (UseDiskFileNoLock()) {
    opener.DoOpen(m_diskFile, std::ios_base::binary | std::ios_base::ate |
                  std::ios_base::in | std::ios_base::out);  // open for write
    m_body->seekp(m_offset, std::ios_base::beg);
  } else {
    m_body->seekp(0, std::ios_base::beg);
  }
  if (!m_body->good()) {
    DebugError("Fail to seek body " + ToStringLine(offset, len));
  } else {
    instream->seekg(0, std::ios_base::beg);
    if (len == instreamLen) {
      (*m_body) << instream->rdbuf();
    } else if (len < instreamLen) {
      stringstream ss;
      ss << instream->rdbuf();
      m_body->write(ss.str().c_str(), len);
    }
    if (!m_body->good()) {
      DebugError("Fail to write buffer " + ToStringLine(offset, len));
    }
  }
}

// --------------------------------------------------------------------------
void Page::SetStream(const shared_ptr<iostream> &stream) {
  lock_guard<recursive_mutex> lock(m_mutex);
  m_body = stream;
}

// --------------------------------------------------------------------------
bool Page::SetupDiskFile() {
  lock_guard<recursive_mutex> lock(m_mutex);
  CreateDirectoryIfNotExists(
      QS::Configure::Options::Instance().GetDiskCacheDirectory());

  std::ios_base::openmode mode = std::ios_base::binary | std::ios_base::ate |
                                 std::ios_base::in | std::ios_base::out;
  if (!FileExists(m_diskFile)) {
    // Notice: set open mode with std::ios_base::out to create file in disk if
    // it is not exist
    mode = std::ios_base::binary | std::ios_base::out;
  }
  shared_ptr<fstream> file = make_shared<fstream>(m_diskFile.c_str(), mode);
  if (file && file->is_open()) {
    DebugInfo("Open file " + FormatPath(m_diskFile));
  }
  file->close();
  m_body = file;
  return true;
}

// --------------------------------------------------------------------------
void Page::ResizeToSmallerSize(size_t smallerSize) {
  // Do a lazy resize:
  // 1. Change size to 'samllerSize'.
  // 2. Set output position indicator to 'samllerSize'.
  assert(0 <= smallerSize && smallerSize <= m_size);
  lock_guard<recursive_mutex> lock(m_mutex);
  m_size = smallerSize;
  if (UseDiskFileNoLock()) {
    m_body->seekp(m_offset + smallerSize, std::ios_base::beg);
  } else {
    m_body->seekp(smallerSize, std::ios_base::beg);
  }
}

// --------------------------------------------------------------------------
bool Page::Refresh(off_t offset, size_t len, const char *buffer,
                   const string &diskfile) {
  if (len == 0) {
    return true;  // do nothing
  }

  bool isValidInput = offset >= m_offset && buffer != NULL && len > 0;
  assert(isValidInput);
  if (!isValidInput) {
    DebugError("Try to refresh page(" + ToStringLine(m_offset, m_size) +
               ") with invalid input " + ToStringLine(offset, len, buffer));
    return false;
  }

  lock_guard<recursive_mutex> lock(m_mutex);
  return UnguardedRefresh(offset, len, buffer, diskfile);
}

// --------------------------------------------------------------------------
bool Page::UnguardedRefresh(off_t offset, size_t len, const char *buffer,
                            const string &diskfile) {
  size_t moreLen = offset + len - Next();
  size_t dataLen = moreLen > 0 ? m_size + moreLen : m_size;
  shared_ptr<IOStream> data = make_shared<IOStream>(dataLen);
  {
    FileOpener opener(m_body);
    if (UseDiskFileNoLock()) {
      opener.DoOpen(m_diskFile,
                    std::ios_base::binary | std::ios_base::in);  // for read
      m_body->seekg(offset, std::ios_base::beg);
    } else {
      m_body->seekg(0, std::ios_base::beg);
    }
    (*data) << m_body->rdbuf();
  }

  data->seekp(offset - m_offset, std::ios_base::beg);
  data->write(buffer, len);

  if (!data->good()) {
    DebugError("Fail to refresh page(" + ToStringLine(m_offset, m_size) +
               ") with input " + ToStringLine(offset, len, buffer));
    return false;
  }

  if (!diskfile.empty()) {
    m_diskFile = diskfile;
    if (!SetupDiskFile()) {
      DebugError("Unable to set up file " + FormatPath(m_diskFile));
      return false;
    }
  }

  FileOpener opener(m_body);
  if (UseDiskFileNoLock()) {
    opener.DoOpen(m_diskFile, std::ios_base::binary | std::ios_base::ate |
                  std::ios_base::in | std::ios_base::out);  // open for write
    m_body->seekp(m_offset, std::ios_base::beg);
    if (!m_body->good()) {
      DebugError("Fail to seek page(" + ToStringLine(m_offset, m_size) +
                 ") with input " + ToStringLine(offset, len, buffer));
      return false;
    }
    data->seekg(0, std::ios_base::beg);
    (*m_body) << data->rdbuf();  // put pages' all content into disk file
  } else {
    data->seekg(0, std::ios_base::beg);
    m_body = data;
  }
  if (moreLen > 0) {
    m_size += moreLen;
  }
  if (m_body->good()) {
    return true;
  } else {
    DebugError("Fail to refresh page(" + ToStringLine(m_offset, m_size) +
               ") with input " + ToStringLine(offset, len, buffer));
    return false;
  }
}

// --------------------------------------------------------------------------
size_t Page::Read(off_t offset, size_t len, char *buffer) {
  if (len == 0) {
    return 0;  // do nothing
  }

  bool isValidInput =
      (offset >= m_offset && offset < Next() && buffer != NULL && len > 0 &&
       len <= static_cast<size_t>(Next() - offset));
  assert(isValidInput);
  DebugErrorIf(!isValidInput,
               "Try to read page (" + ToStringLine(m_offset, m_size) +
               ") with invalid input " + ToStringLine(offset, len, buffer));

  lock_guard<recursive_mutex> lock(m_mutex);
  return UnguardedRead(offset, len, buffer);
}

// --------------------------------------------------------------------------
size_t Page::UnguardedRead(off_t offset, size_t len, char *buffer) {
  if (!m_body) {
    DebugError("null body stream " + ToStringLine(offset, len, buffer));
    return 0;
  }
  FileOpener opener(m_body);
  if (UseDiskFileNoLock()) {
    opener.DoOpen(m_diskFile,
                  std::ios_base::binary | std::ios_base::in);  // open for read
    m_body->seekg(offset, std::ios_base::beg);
  } else {
    m_body->seekg(offset - m_offset, std::ios_base::beg);
  }
  if (!m_body->good()) {
    DebugError("Fail to seek page(" + ToStringLine(m_offset, m_size) +
               ") with input " + ToStringLine(offset, len, buffer));
    return 0;
  }

  m_body->read(buffer, len);
  if (!m_body->good()) {
    DebugError("Fail to read page(" + ToStringLine(m_offset, m_size) +
               ") with input " + ToStringLine(offset, len, buffer));
    return 0;
  } else {
    return len;
  }
}

// --------------------------------------------------------------------------
string ToStringLine(off_t offset, size_t len, const char *buffer) {
  return "[offset:size:buffer=" + to_string(offset) + ":" + to_string(len) +
         ":" + PointerAddress(buffer) + "]";
}

// --------------------------------------------------------------------------
string ToStringLine(off_t offset, size_t size) {
  return "[offset:size=" + to_string(offset) + ":" + to_string(size) + "]";
}

// --------------------------------------------------------------------------
string ToStringLine(const string &fileId, off_t offset, size_t len,
                    const char *buffer) {
  return "[fileId:offset:size:buffer=" + fileId + ":" + to_string(offset) +
         ":" + to_string(len) + ":" + PointerAddress(buffer) + "]";
}

}  // namespace Data
}  // namespace QS
