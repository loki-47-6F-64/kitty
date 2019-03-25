#include <unistd.h>
#include <fcntl.h>

#include <sys/stat.h>

#include <kitty/log/log.h>

file::Log error;
file::Log warning;
file::Log info;
file::Log debug;

namespace file {

namespace stream {
THREAD_LOCAL util::ThreadLocal<char[DATE_BUFFER_SIZE]> _date;

std::mutex __lock;
}


Log logWrite(std::string &&prepend, const char *file_path) {
  int _fd = ::open(file_path,
    O_CREAT | O_APPEND | O_WRONLY,
    S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH
  );

  return Log { std::chrono::seconds(0), std::move(prepend), _fd };
}

void log_open(const char *logPath) {
  if(!logPath) {
    error   = file::Log { std::chrono::seconds(0), "Error: ",   dup(STDERR_FILENO) };
    warning = file::Log { std::chrono::seconds(0), "Warning: ", dup(STDOUT_FILENO) };
    info    = file::Log { std::chrono::seconds(0), "Info: ",    dup(STDOUT_FILENO) };
    debug   = file::Log { std::chrono::seconds(0), "Debug: ",   dup(STDOUT_FILENO) };
  }

  else {
    error   = logWrite("Error: ",   logPath);
    warning = logWrite("Warning: ", logPath);
    info    = logWrite("Info: ",    logPath);
    debug   = logWrite("Debug: ",   logPath);
  }

  info.append("Opened log.\n").out();
}

}
