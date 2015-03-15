#ifndef KITTY_THREAD_POOL_H
#define KITTY_THREAD_POOL_H

#include <kitty/util/task_pool.h>
#include <kitty/util/thread_t.h>

namespace util {
/*
 * Allow threads to execute unhindered
 * while keeping full controll over the threads.
 */
class ThreadPool : public TaskPool {
public:
  typedef TaskPool::__task __task;
  
private:
  std::vector<thread_t> _thread;

  bool _continue;
public:

  ThreadPool(int threads) : _thread(threads), _continue(true) {
    for (auto & t : _thread) {
      t = thread_t(&ThreadPool::_main, this);
    }
  }

  ~ThreadPool() {
    join();
  }

  void join() {
    if (!_continue) return;

    _continue = false;
    for (auto & t : _thread) {
      t.join();
    }
  }

public:

  void _main() {
    std::chrono::milliseconds sleeper(10);

    util::Optional<__task> task;
    while (_continue) {
      if((task = this->pop())) {
        std::this_thread::sleep_for(sleeper);
        continue;
      }
      (*task)->run();
    }

    // Execute remaining tasks
    while((task = this->pop())) {
      (*task)->run();
    }
  }
};

}
#endif
