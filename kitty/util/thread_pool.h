#ifndef KITTY_THREAD_POOL_H
#define KITTY_THREAD_POOL_H

#include <thread>
#include <kitty/util/task_pool.h>

namespace util {
/*
 * Allow threads to execute unhindered
 * while keeping full controll over the threads.
 */
class ThreadPool : public TaskPool {
public:
  typedef TaskPool::__task __task;
  
private:
  std::vector<std::thread> _thread;

  std::condition_variable _cv;
  std::mutex _lock;
  
  std::atomic<bool> _continue;
public:

  ThreadPool(int threads) : _thread(threads), _continue(true) {
    for (auto & t : _thread) {
      t = std::thread(&ThreadPool::_main, this);
    }
  }

  ~ThreadPool() {
    join();
  }

  template<class Function, class... Args>
  auto push(Function && newTask, Args &&... args) {
    auto future = TaskPool::push(std::forward<Function>(newTask), std::forward<Args>(args)...);
    
    _cv.notify_one();
    return future;
  }

  template<class Function, class X, class Y, class... Args>
  auto pushDelayed(Function &&newTask, std::chrono::duration<X, Y> duration, Args &&... args) {
    auto future = TaskPool::pushDelayed(std::forward<Function>(newTask), duration, std::forward<Args>(args)...);

    // Update all timers for wait_until
    _cv.notify_all();
    return future;
  }
  
  void join() {
    if (!_continue.exchange(false)) return;
    
    _cv.notify_all();
    for (auto & t : _thread) {
      t.join();
    }
  }

public:

  void _main() {
    while (_continue.load()) {
      if(auto task = this->pop()) {
        (*task)->run();
      }
      else {
        std::unique_lock<std::mutex> uniq_lock(_lock);
        if(auto tp = next()) {
          _cv.wait_until(uniq_lock, *tp);
        }
        else {
          _cv.wait(uniq_lock);
        }
      }
    }

    // Execute remaining tasks
    while(auto task = this->pop()) {
      (*task)->run();
    }
  }
};
}
#endif
