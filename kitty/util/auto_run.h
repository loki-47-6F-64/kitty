#ifndef KITTY_UTIL_AUTO_RUN_H
#define KITTY_UTIL_AUTO_RUN_H

#include <memory>
#include <atomic>
#include <thread>
#include <chrono>
#include <mutex>

#include <kitty/util/utility.h>

namespace util {
enum class run_state_e {
  INIT,
  RUNNING,
  CANCEL
};

template<class T> 
class AutoRun {
public:
  typedef T thread;
private:
  std::atomic<run_state_e> _state;
  thread _loop;
  thread _hangup;
  
  std::chrono::steady_clock::time_point _loop_start;
public:
  AutoRun() : _state(run_state_e::INIT), _loop_start() {}
  
  template<class Start, class Middle, class End, class Hang>
  void run(Start &&start, Middle &&middle, End &&end, Hang &&hang) {
    _loop = thread([this](auto &&start, auto &&middle, auto &&end, auto &&hang) {
      start();
      
      _loop_start = std::chrono::steady_clock::now();
      _hangup = thread([this](auto &&hangup) {
        while(_state.load() != run_state_e::CANCEL) {
          if(_loop_start + std::chrono::seconds(1) < std::chrono::steady_clock::now()) {
            hangup();
          }
          
          std::this_thread::sleep_for(std::chrono::seconds(1));
        }
      }, std::forward<decltype(hang)>(hang));
      
      if(_state.exchange(run_state_e::RUNNING) == run_state_e::CANCEL) {
        _state.store(run_state_e::INIT);

        return;
      }

      while(_state.load() != run_state_e::CANCEL) {
        _loop_start = std::chrono::steady_clock::now();
        middle();
      }
      end();

      _state.store(run_state_e::INIT);
    }, std::forward<Start>(start),  std::forward<Middle>(middle),  std::forward<End>(end), std::forward<Hang>(hang));
  }
  
  ~AutoRun() { stop(); }
  
  void stop() {
    _state.store(run_state_e::CANCEL);
  }
  
  void join() {
    if(_loop.joinable()) {
      _loop.join();
    }
    
    if(_hangup.joinable()) {
      _hangup.join();
    }
  }
  
  bool isRunning() const { return _state.load() == run_state_e::RUNNING; }
};

template<>
class AutoRun<void> {
  std::atomic<run_state_e> _state;
  
  std::mutex _lock;
public:
  AutoRun() : _state(run_state_e::INIT) {}

  
  template<class Start, class Middle, class End>
  void run(Start &&start, Middle &&middle, End &&end) {
    std::lock_guard<std::mutex> lg(_lock);
    
    start();

    if(_state.exchange(run_state_e::RUNNING) == run_state_e::CANCEL) {
      _state.store(run_state_e::INIT);

      return;
    }

    while(_state.load() != run_state_e::CANCEL) {
      middle();
    }
    end();

    _state.store(run_state_e::INIT);
  }
  
  template<class Middle, class End>
  void run(Middle &&middle, End &&end) { run([](){}, std::forward<Middle>(middle), std::forward<End>(end)); }
  
  template<class Middle>
  void run(Middle &&middle) { run([](){}, std::forward<Middle>(middle), [](){}); }
  
  ~AutoRun() { stop(); }
  
  void stop() {
    _state.store(run_state_e::CANCEL);
  }
  
  void join() {
    std::lock_guard<std::mutex> lg(_lock);
  }

  bool isRunning() const { return _state.load() == run_state_e::RUNNING; }
};

}

#endif
