#ifndef KITTY_UTIL_AUTO_RUN_H
#define KITTY_UTIL_AUTO_RUN_H

#include <memory>
#include <atomic>

#include <kitty/util/utility.h>

namespace util {
template<class T> 
class AutoRun {
public:
  typedef T thread;
private:
  std::unique_ptr<std::atomic<bool>> _is_running;
  thread _thread;
  
public:
  AutoRun() = default;
  
  AutoRun(AutoRun &&other) = default;
  
  template<class Start, class Middle, class End>
  AutoRun(Start &&start, Middle &&middle, End &&end) : _is_running(mk_uniq<std::atomic<bool>>(true)) {
    _thread = thread([](std::function<void()> start, std::function<void()> middle, std::function<void()> end, std::atomic<bool> *is_running) {
      start();
      while(is_running->load()) {
        middle();
      }
      end();
    }, std::forward<Start>(start),  std::forward<Middle>(middle),  std::forward<End>(end), _is_running.get());
  }
  
  template<class Middle, class End>
  AutoRun(Middle &&middle, End &&end) : AutoRun([](){}, std::forward<Middle>(middle), std::forward<End>(end)) {}
  
  template<class Middle>
  AutoRun(Middle &&middle) : AutoRun([](){}, std::forward<Middle>(middle), [](){}) {}
  
  AutoRun &operator = (AutoRun &&other) = default;
  
  ~AutoRun() { stop(); }
  
  void stop() {
    if(_is_running && _is_running->exchange(false)) {
      _thread.join();
    }
  }
};

}

#endif