#include "bell/utils/Task.h"

#include <bell/Logger.h>
#include <pthread.h>
#include <algorithm>
#include <cstring>

using namespace bell::utils;

class Task::Impl {
 public:
  Impl(const std::string& /*taskName*/, int stackSize, int /*espPriority*/,
       TaskCore /*espTaskCore*/, bool /*espStackOnPsram*/)
      : stackSize(stackSize) {}
  ~Impl() {
    if (threadAttrInitialized) {
      pthread_attr_destroy(&threadAttr);
    }
  };

  // Delete copy constructor and copy assignment operator
  Impl(const Impl&) = delete;
  Impl& operator=(const Impl&) = delete;

  bool startTask(Task* task) {
    if (threadAttrInitialized) {
      pthread_attr_destroy(&threadAttr);
      threadAttrInitialized = false;
    }
    int ret = pthread_attr_init(&threadAttr);

    if (ret > 0) {
      // Could not initialize the pthread attribute
      return false;
    }

    threadAttrInitialized = true;

    // Set taskRunning to true BEFORE spawning the thread. If the new thread
    // gets delayed and a stopTask() call slips in before it runs, the flag
    // will be false by the time the thread executes runTask() and the
    // default loop will exit immediately. Setting the flag inside runTask
    // would overwrite that signal and cause runaway / use-after-free.
    task->taskRunning = true;

    // Joinable thread (no pthread_detach). stopTask() calls pthread_join,
    // which guarantees the OS thread has fully terminated before this
    // object can be destroyed.
    int err = pthread_create(&threadHandle, &threadAttr, threadEntryFunc, task);
    if (err == 0) {
      threadJoinable = true;
      return true;
    }

    // pthread_create failed; never started running.
    task->taskRunning = false;
    BELL_LOG(error, "BellTask", "Failed to create thread. Error: {}",
             strerror(err));
    return false;
  }

  void stopTask(Task* task) {
    // Multiple calls are safe; pthread_join must only happen once.
    task->taskRunning = false;
    task->wakeTask();
    if (threadJoinable) {
      pthread_join(threadHandle, nullptr);
      threadJoinable = false;
    }
  }

  // Pthread thread entry function
  static void* threadEntryFunc(void* ctx) {
    static_cast<Task*>(ctx)->runTask();
    return nullptr;
  }

 private:
  pthread_t threadHandle = 0;
  pthread_attr_t threadAttr{};
  bool threadAttrInitialized = false;
  bool threadJoinable = false;

  int stackSize;
};

// Task constructor and member methods
Task::Task(const std::string& taskName, int stackSize, int espPriority,
           TaskCore espTaskCore, bool espStackOnPsram)
    : pImpl(std::make_unique<Impl>(taskName, stackSize, espPriority,
                                   espTaskCore, espStackOnPsram)) {}

Task::~Task() {
  // Final safety net: ensure the thread is joined before the pImpl (and
  // anything else) goes away. Derived destructors should have already
  // called stopTask() before any of their own members begin to destruct --
  // wakeTask() dispatches through the vtable, which is no longer derived
  // at this point.
  stopTask();
}

bool Task::startTask() {
  return pImpl->startTask(this);
}

void Task::stopTask() {
  pImpl->stopTask(this);
}
