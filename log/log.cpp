#include <unistd.h>

#include "log.h"

file::Log error  (-1, " Error: "  , dup(STDERR_FILENO));
file::Log warning(-1, " Warning: ", dup(STDOUT_FILENO));
file::Log info   (-1, " Info: "   , dup(STDOUT_FILENO));
file::Log debug  (-1, " Debug: "  , dup(STDOUT_FILENO));

namespace file {
std::mutex stream::_LogBase::_lock;
char stream::_LogBase::_date[DATE_BUFFER_SIZE];

int logStreamWrite(stream::Log<stream::io> &ls, const char *path) {
  return ls.access(path, stream::ioWriteAppend);
}

void log_open(const char *logPath) {
  error  .access(logPath, logStreamWrite);
  warning.access(logPath, logStreamWrite);
  info   .access(logPath, logStreamWrite);
  debug  .access(logPath, logStreamWrite);

  info.append("Opened log.\n").out();
}

}