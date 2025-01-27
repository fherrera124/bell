#include "bell/utils/Task.h"

#include <bell/Logger.h>
#include <pthread.h>
#include <algorithm>

using namespace bell::utils;

class Task::Impl {
 public:
  Impl(const std::string& taskName, int stackSize, int espPriority,
       TaskCore espTaskCore, bool espStackOnPsram)
      : stackSize(stackSize),
        espTaskCore(espTaskCore),
        espStackOnPsram(espStackOnPsram),
        espPriority(espPriority),
        taskName(taskName) {}
  ~Impl() {

  };

  // Delete copy constructor and copy assignment operator
  Impl(const Impl&) = delete;
  Impl& operator=(const Impl&) = delete;

  bool startTask(Task* task) { return false; }

 private:
  int stackSize = 0;
  TaskCore espTaskCore;
  bool espStackOnPsram = false;
  int espPriority;
  std::string taskName;
};

// Task constructor and member methods
Task::Task(const std::string& taskName, int stackSize, int espPriority,
           TaskCore espTaskCore, bool espStackOnPsram)
    : pImpl(std::make_unique<Impl>(taskName, stackSize, espPriority,
                                   espTaskCore, espStackOnPsram)) {}

Task::~Task() {
  stopTask();
}

bool Task::startTask() {
  return pImpl->startTask(this);
}
