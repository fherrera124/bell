#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <string>

namespace bell::utils {
// Enumeration of task cores, used on Espressif platforms
enum class TaskCore {
  Core0 = 0,
  Core1 = 1,
  CoreAny = -1,
};

class Task {
 public:
  /**
   * @brief Default constructor for the Task base class
   * @param taskName The name of the task
   * @param stackSize The size of the task stack
   * @param espPriority The priority of the task, Espressif platforms only
   * @param espTaskCore The core to run the task on, Espressif platforms only
   * @param espStackOnPsram Whether to allocate the stack on PSRAM, Espressif platforms only.
   */
  Task(const std::string& taskName, int stackSize, int espPriority = 0,
       TaskCore espTaskCore = TaskCore::CoreAny, bool espStackOnPsram = true);
  virtual ~Task();

  // Delete copy constructor and copy assignment operator
  Task(const Task&) = delete;
  Task& operator=(const Task&) = delete;

  /**
   * @brief Stop the task, blocking until it has fully exited.
   *
   * Sets taskRunning to false, calls wakeTask() to let the derived class
   * unblock any condition variables / semaphores it may be waiting on, and
   * then waits for the OS thread (pthread_join on POSIX, completion
   * semaphore on ESP) to confirm the task has finished.
   *
   * This is the key guarantee: after stopTask() returns, there is no
   * OS thread left that could still access this object -- making it safe
   * for a derived class destructor to proceed with member teardown.
   */
  void stopTask();

 protected:
  // Default task runner implementation, can be overridden by derived classes
  virtual void runTask() {
    // The taskRunning flag is initialized by startTask() *before* the OS
    // thread is created. Don't overwrite it here -- if stopTask() was
    // called before we actually started running, taskRunning is already
    // false and we should exit immediately instead of starting work.
    std::scoped_lock lock(taskRunningMutex);
    while (taskRunning) {
      taskLoop();
    }
  }

  /**
   * @brief The task loop function. This function is called repeatedly by the runTask method.
   * @remark This method should be implemented by the derived class to perform the task's work, unless a custom runTask method is provided.
   */
  virtual void taskLoop(){};

  /**
   * @brief Hook called by stopTask() to wake the task if it's blocked.
   *
   * The default implementation does nothing. Override to signal any
   * condition variables, semaphores, or event groups the task may be
   * waiting on, so the taskLoop / runTask can observe the cleared
   * taskRunning flag and exit promptly.
   */
  virtual void wakeTask() {}

  // @brief Starts the task's execution. This method is implemented per-platform.
  bool startTask();

  // Used to keep track of the task state during runTask execution
  std::mutex taskRunningMutex;
  std::atomic<bool> taskRunning = false;

 private:
  class Impl;
  std::unique_ptr<Impl> pImpl;
};

}  // namespace bell::utils

namespace bell {
using Task = utils::Task;
using TaskCore = utils::TaskCore;
}  // namespace bell
