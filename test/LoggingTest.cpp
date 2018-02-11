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

#include <stdio.h>     // for fopen
#include <sys/stat.h>  // for stat

#include <fstream>
#include <iomanip>
#include <ostream>
#include <string>
#include <vector>

#include "boost/function.hpp"
#include "gtest/gtest.h"

#include "base/LogLevel.h"
#include "base/LogMacros.h"
#include "base/Logging.h"
#include "base/Utils.h"

namespace QS {

namespace Logging {

// Notice this testing based on the assumption that glog
// always link the qsfs.INFO, qsfs.WARN, qsfs.ERROR,
// qsfs.FATAL to the latest printed log files.

// In order to test Log private members, need to define
// test fixture and tests in the same namespae as Log
// so they can be friends of class Log.

using QS::Logging::LogLevel;
using std::fstream;
using std::ostream;
using std::string;
using std::vector;
using ::testing::Values;
using ::testing::WithParamInterface;

static const char *defaultLogDir = "/tmp/qsfs.test.logs/";
const char *infoLogFile = "/tmp/qsfs.test.logs/qsfs.INFO";
const char *fatalLogFile = "/tmp/qsfs.test.logs/qsfs.FATAL";

void MakeDefaultLogDir() {
  bool success = QS::Utils::CreateDirectoryIfNotExists(defaultLogDir);
  ASSERT_TRUE(success) << "Fail to create directory " << defaultLogDir;
}

void ClearFileContent(const std::string &path) {
  FILE *pf = fopen(path.c_str(), "w");
  if (pf != NULL) {
    fclose(pf);
  }
}

void LogNonFatalPossibilities() {
  Error("test Error");
  ErrorIf(true, "test ErrorIf");
  DebugError("test DebugError");
  DebugErrorIf(true, "test DebugErrorIf");
  Warning("test Warning");
  WarningIf(true, "test WarningIf");
  DebugWarning("test DebugWarning");
  DebugWarningIf(true, "test DebugWarningIf");
  Info("test Info");
  InfoIf(true, "test InfoIf");
  DebugInfo("test DebugInfo");
  DebugInfoIf(true, "test DebugInfoIf");
}

void RemoveLastLines(vector<string> &expectedMsgs, int count) {  // NOLINT
  for (int i = 0; i < count && (!expectedMsgs.empty()); ++i) {
    expectedMsgs.pop_back();
  }
}

void VerifyAllNonFatalLogs(LogLevel::Value level) {
  struct stat info;
  int status = stat(infoLogFile, &info);
  ASSERT_EQ(status, 0) << infoLogFile << " is not existing";

  vector<string> logMsgs;
  {
    fstream fs(infoLogFile);
    ASSERT_TRUE(fs.is_open()) << "Fail to open " << infoLogFile;

    string::size_type pos = string::npos;
    for (string line; std::getline(fs, line);) {
      if ((pos = line.find("[INFO]")) != string::npos ||
          (pos = line.find("[WARN]")) != string::npos ||
          (pos = line.find("[ERROR]")) != string::npos) {
        logMsgs.push_back(string(line, pos));
      }
    }
  }

  vector<string> expectedMsgs;
  expectedMsgs.push_back("[ERROR] test Error");
  expectedMsgs.push_back("[ERROR] test ErrorIf");
  expectedMsgs.push_back("[ERROR] test DebugError");
  expectedMsgs.push_back("[ERROR] test DebugErrorIf");
  expectedMsgs.push_back("[WARN] test Warning");
  expectedMsgs.push_back("[WARN] test WarningIf");
  expectedMsgs.push_back("[WARN] test DebugWarning");
  expectedMsgs.push_back("[WARN] test DebugWarningIf");
  expectedMsgs.push_back("[INFO] test Info");
  expectedMsgs.push_back("[INFO] test InfoIf");
  expectedMsgs.push_back("[INFO] test DebugInfo");
  expectedMsgs.push_back("[INFO] test DebugInfoIf");

  if (level == LogLevel::Warn) {
    RemoveLastLines(expectedMsgs, 4);
  } else if (level == LogLevel::Error) {
    RemoveLastLines(expectedMsgs, 8);
  }

  EXPECT_EQ(logMsgs, expectedMsgs);
}

class LoggingTest : public ::testing::Test {
 public:
  LoggingTest() {}

  static void SetUpTestCase() {
    MakeDefaultLogDir();
    Log::Instance().Initialize(defaultLogDir);
  }

  ~LoggingTest() {}

 protected:
  void TestNonFatalLogsLevelInfo() {
    Log::Instance().SetDebug(true);
    Log::Instance().SetLogLevel(LogLevel::Info);
    ClearFileContent(infoLogFile);  // make sure only contain logs of this test
    LogNonFatalPossibilities();
    VerifyAllNonFatalLogs(LogLevel::Info);
  }

  void TestNonFatalLogsLevelWarn() {
    Log::Instance().SetDebug(true);
    Log::Instance().SetLogLevel(LogLevel::Warn);
    ClearFileContent(infoLogFile);  // make sure only contain logs of this test
    LogNonFatalPossibilities();
    VerifyAllNonFatalLogs(LogLevel::Warn);
  }

  void TestNonFatalLogsLevelError() {
    Log::Instance().SetDebug(true);
    Log::Instance().SetLogLevel(LogLevel::Error);
    ClearFileContent(infoLogFile);  // make sure only contain logs of this test
    LogNonFatalPossibilities();
    VerifyAllNonFatalLogs(LogLevel::Error);
  }
};

// Test Cases
TEST_F(LoggingTest, NonFatalLogsLevelInfo) { TestNonFatalLogsLevelInfo(); }

TEST_F(LoggingTest, NonFatalLogsLevelWarn) { TestNonFatalLogsLevelWarn(); }

TEST_F(LoggingTest, NonFatalLogsLevelError) { TestNonFatalLogsLevelError(); }

//
//
// As glog logging a FATAL message will terminate the program,
// so add death tests for FATAL log.
//
//
typedef boost::function<void(bool)> LogFatalFun;

struct LogFatalState {
  LogFatalState(LogFatalFun func, const string &msg, bool cond, bool dbg,
                bool die)
      : logFatalFunc(func),
        fatalMsg(msg),
        condition(cond),
        isDebug(dbg),
        willDie(die) {}

  friend ostream &operator<<(ostream &os, const LogFatalState &state) {
    return os << "[fatalMsg: " << state.fatalMsg
              << ", condition: " << std::boolalpha << state.condition
              << ", debug: " << state.isDebug << ", will die: " << state.willDie
              << "]";
  }

  LogFatalFun logFatalFunc;
  string fatalMsg;
  bool condition;  // only effective for *If macros
  bool isDebug;    // only effective for Debug* macros
  bool willDie;    // this only for death test
};

void LogFatal(bool condition) { Fatal("test Fatal"); }
void LogFatalIf(bool condition) { FatalIf(condition, "test FatalIf"); }
void LogDebugFatal(bool condition) { DebugFatal("test DebugFatal"); }
void LogDebugFatalIf(bool condition) {
  DebugFatalIf(condition, "test DebugFatalIf");
}

void VerifyFatalLog(const string &expectedMsg) {
  struct stat info;
  int status = stat(fatalLogFile, &info);
  ASSERT_EQ(status, 0) << fatalLogFile << " is not existing";

  string logMsg;
  {
    fstream fs(fatalLogFile);
    ASSERT_TRUE(fs.is_open()) << "Fail to open " << fatalLogFile;

    string::size_type pos = string::npos;
    for (string line; std::getline(fs, line);) {
      if ((pos = line.find("[FATAL]")) != string::npos) {
        logMsg = string(line, pos);
        break;
      }
    }
  }
  EXPECT_EQ(logMsg, expectedMsg);
}

class FatalLoggingDeathTest : public LoggingTest,
                              public WithParamInterface<LogFatalState> {
 public:
  void SetUp() {  // override
    MakeDefaultLogDir();
    m_fatalMsg = GetParam().fatalMsg;
  }

 protected:
  void TestWithDebugAndIf() {
    LogFatalFun func = GetParam().logFatalFunc;
    bool condition = GetParam().condition;
    Log::Instance().SetDebug(GetParam().isDebug);
    // only when log msg fatal sucessfully the test will die,
    // otherwise the test will fail to die.
    if (GetParam().willDie) {
      ASSERT_DEATH({ func(condition); }, "");
    }
    VerifyFatalLog(m_fatalMsg);
  }

 protected:
  string m_fatalMsg;
};

// Test Case
TEST_P(FatalLoggingDeathTest, WithDebugAndIf) { TestWithDebugAndIf(); }

INSTANTIATE_TEST_CASE_P(
    LogFatal, FatalLoggingDeathTest,
    // Notice when macro fail to log message, glog will not flush any stream
    // to the log file, so the expectMsg will keep unchanged.

    // logFun, expectMsg, condition, isDebug, will die
    Values(LogFatalState(LogFatal, "[FATAL] test Fatal", true, false, true),
           LogFatalState(LogFatalIf, "[FATAL] test FatalIf", true, false, true),
           LogFatalState(LogFatalIf, "[FATAL] test FatalIf", false, false,
                         false),
           LogFatalState(LogDebugFatal, "[FATAL] test DebugFatal", true, true,
                         true),
           LogFatalState(LogDebugFatal, "[FATAL] test DebugFatal", true, false,
                         false),
           LogFatalState(LogDebugFatalIf, "[FATAL] test DebugFatalIf", true,
                         true, true),
           LogFatalState(LogDebugFatalIf, "[FATAL] test DebugFatalIf", false,
                         true, false),
           LogFatalState(LogDebugFatalIf, "[FATAL] test DebugFatalIf", true,
                         false, false),
           LogFatalState(LogDebugFatalIf, "[FATAL] test DebugFatalIf", false,
                         false, false)));

}  // namespace Logging
}  // namespace QS

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  int code = RUN_ALL_TESTS();
  return code;
}
