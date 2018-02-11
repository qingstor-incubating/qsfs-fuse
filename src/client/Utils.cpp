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

#include "client/Utils.h"

#include <assert.h>

#include <iostream>
#include <sstream>
#include <string>
#include <utility>

#include "boost/exception/to_string.hpp"
#include "boost/tuple/tuple.hpp"

#include "base/LogMacros.h"
#include "base/StringUtils.h"

namespace QS {

namespace Client {

namespace Utils {

using boost::make_tuple;
using boost::to_string;
using boost::tuple;
using std::istream;
using std::make_pair;
using std::pair;
using std::string;

template <char C>
istream &expect(istream &in) {
  if ((in >> std::ws).peek() == C) {
    in.ignore();
  } else {
    in.setstate(std::ios_base::failbit);
  }
  return in;
}
template istream &expect<'-'>(istream &in);
template istream &expect<'/'>(istream &in);

// --------------------------------------------------------------------------
std::string BuildRequestRange(off_t start, size_t size) {
  assert(size > 0);
  DebugWarningIf(size == 0, "Invalide input with zero range size");
  // format: "bytes=start_offset-stop_offset"
  // e.g. bytes=0-0 return the first byte
  string range = "bytes=";
  range += to_string(start);
  range += "-";
  range += to_string(start + size - 1);
  return range;
}

// --------------------------------------------------------------------------
string BuildRequestRangeStart(off_t start) {
  // format of "bytes=start_offset-"
  string range = "bytes=";
  range += to_string(start);
  range += "-";
  return range;
}

// --------------------------------------------------------------------------
string BuildRequestRangeEnd(off_t suffixLen) {
  // format of "bytes=-suffix_length"
  string range = "bytes=-";
  range += to_string(suffixLen);
  return range;
}

// --------------------------------------------------------------------------
tuple<off_t, size_t, size_t> ParseResponseContentRange(
    const std::string &contentRange) {
  string cpy(QS::StringUtils::Trim(contentRange, ' '));
  assert(!cpy.empty());
  if (cpy.empty()) {
    DebugWarning("Invalid input with empty content range");
    return make_tuple(0, 0, 0);
  }

  // format: "bytes start_offset-stop_offset/file_size"
  if (cpy.find("bytes ") == string::npos || cpy.find("-") == string::npos ||
      cpy.find("/") == string::npos) {
    DebugWarning("Invalid input: " + cpy);
    return make_tuple(0, 0, 0);
  }
  cpy = cpy.substr(6);  // remove leading "bytes "
  off_t start = 0;
  off_t stop = 0;
  size_t size = 0;
  std::istringstream in(cpy);
  if (in >> start >> expect<'-'> >> stop >> expect<'/'> >> size) {
    if (!(start >= 0 && stop >= start && size > 0)) {
      DebugWarning("Invalid input: " + cpy);
      return make_tuple(0, 0, 0);
    }
    return make_tuple(start, stop - start + 1, size);
  } else {
    DebugWarning("Invalid input: " + cpy);
    return make_tuple(0, 0, 0);
  }
}

// --------------------------------------------------------------------------
pair<off_t, size_t> ParseRequestContentRange(const string &requestRange) {
  string cpy(QS::StringUtils::Trim(requestRange, ' '));
  assert(!cpy.empty());
  if (cpy.empty()) {
    DebugWarning("Invalid input with empty content range");
    return make_pair(0, 0);
  }

  // format: "bytes=start_offset-stop_offset"
  if (cpy.find("bytes=") == string::npos || cpy.find("-") == string::npos) {
    DebugWarning("Invalid input: " + cpy);
    return make_pair(0, 0);
  }
  cpy = cpy.substr(6);  // remove leading "bytes="
  off_t start = 0;
  off_t stop = 0;
  std::istringstream in(cpy);
  if (in >> start >> expect<'-'> >> stop) {
    if (!(start >= 0 && stop >= start)) {
      DebugWarning("Invalid input: " + cpy);
      return make_pair(0, 0);
    }
    return make_pair(start, stop - start + 1);
  } else {
    DebugWarning("Invalid input: " + cpy);
    return make_pair(0, 0);
  }
}

}  // namespace Utils
}  // namespace Client
}  // namespace QS
