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

#ifndef QSFS_DATA_STREAMBUF_H_
#define QSFS_DATA_STREAMBUF_H_

#include <stddef.h>  // for size_t

#include <streambuf>  // NOLINT
#include <vector>

#include "boost/noncopyable.hpp"
#include "boost/shared_ptr.hpp"

namespace QS {

namespace Client {
class QSTransferManager;
struct ReceivedHandlerMultipleDownload;
struct ReceivedHandlerSingleUpload;
struct ReceivedHandlerMultipleUpload;
}  // namespace Client

namespace Data {

class IOSTream;

typedef boost::shared_ptr<std::vector<char> >Buffer;

/**
 * A stream buf to use with std::iostream
 */
class StreamBuf : public std::streambuf, private boost::noncopyable {
 public:
  StreamBuf(Buffer buf, size_t lenghtToRead);

  ~StreamBuf();

  const Buffer &GetBuffer() const { return m_buffer; }

 protected:
  pos_type seekoff(off_type off, std::ios_base::seekdir dir,
                   std::ios_base::openmode which = std::ios_base::in |
                                                   std::ios_base::out);
  pos_type seekpos(pos_type pos,
                   std::ios_base::openmode which = std::ios_base::in |
                                                   std::ios_base::out);

 private:
  Buffer &GetBuffer() { return m_buffer; }
  Buffer ReleaseBuffer();

  char *begin() { return &(*m_buffer)[0]; }
  char *end() { return begin() + m_lengthToRead; }

 private:
  StreamBuf() {}
  Buffer m_buffer;
  size_t m_lengthToRead;  // length in bytes to actually use in the buffer
                          // e.g. you have a 1kb buffer, but only want
                          // stream to see 500 b of it.

  friend class IOStream;
  friend class QS::Client::QSTransferManager;
  friend struct QS::Client::ReceivedHandlerMultipleDownload;
  friend struct QS::Client::ReceivedHandlerSingleUpload;
  friend struct QS::Client::ReceivedHandlerMultipleUpload;
  friend class StreamBufTest;
};

}  // namespace Data
}  // namespace QS

#endif  // QSFS_DATA_STREAMBUF_H_
