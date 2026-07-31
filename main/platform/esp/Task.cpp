#include "bell/utils/Task.h"

// Library includes
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <freertos/timers.h>
#include "sdkconfig.h"

using namespace bell::utils;

class Task::Impl {
 public:
  Impl(std::string taskName, int stackSize, int espPriority,
       TaskCore espTaskCore, bool espStackOnPsram)
      : stackSize(stackSize),
        espTaskCore(espTaskCore),
        espStackOnPsram(espStackOnPsram),
        espPriority(espPriority),
        taskName(std::move(taskName)) {

    if (this->espStackOnPsram) {
      xStack = static_cast<StackType_t*>(heap_caps_malloc(
          this->stackSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));

      xTaskBuffer = static_cast<StaticTask_t*>(heap_caps_malloc(
          sizeof(StaticTask_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    }
  }
  ~Impl() {
    // Ensure the task is stopped (no-op if it wasn't started or has already
    // been stopped). Must happen before we free the stack memory below.
    if (taskPtr != nullptr) {
      stopTask(taskPtr);
    }

    if (doneSem != nullptr) {
      vSemaphoreDelete(doneSem);
      doneSem = nullptr;
    }

    if (xStack) {
      heap_caps_free(xStack);

      // Create a cleanup timer for PSRAM task TCB
      auto* timerHandle =
          xTimerCreate("TaskCleanupTimer", pdMS_TO_TICKS(5000), pdFALSE,
                       xTaskBuffer, [](TimerHandle_t timer) {
                         heap_caps_free(pvTimerGetTimerID(timer));
                         xTimerDelete(timer, portMAX_DELAY);
                       });
      xTimerStart(timerHandle, portMAX_DELAY);
    }
  };

  // Delete copy constructor and copy assignment operator
  Impl(const Impl&) = delete;
  Impl& operator=(const Impl&) = delete;

  // Task entry point shim, used to call the member method. Signals the
  // completion semaphore before self-deleting so stopTask() can confirm the
  // task has fully exited its run body.
  static void taskEntryPointShim(void* ctx) {
    auto* impl = static_cast<Task::Impl*>(ctx);
    SemaphoreHandle_t localDoneSem = impl->doneSem;
    if (impl->taskPtr != nullptr) {
      impl->taskPtr->runTask();
    }
    // Signal completion before self-deletion. After this give, stopTask()
    // may return and impl/taskPtr may be destroyed -- do not touch them.
    xSemaphoreGive(localDoneSem);
    vTaskDelete(NULL);
  }

  size_t getStackHighWaterMarkWords() const {
    return xTaskHandle ? uxTaskGetStackHighWaterMark(xTaskHandle) : 0;
  }

  bool startTask(Task* task) {
    taskPtr = task;
    if (doneSem == nullptr) {
      doneSem = xSemaphoreCreateBinary();
      if (doneSem == nullptr) {
        return false;
      }
    }

    // Set taskRunning BEFORE creating the task; if a stopTask() beats the
    // scheduler, the taskLoop body will exit on its first iteration.
    task->taskRunning = true;

    if (espStackOnPsram) {
      xTaskHandle = xTaskCreateStaticPinnedToCore(
          taskEntryPointShim, this->taskName.c_str(), this->stackSize, this,
          this->espPriority + CONFIG_PTHREAD_TASK_PRIO_DEFAULT, xStack,
          xTaskBuffer, getFreeRTOSTaskCore());
    } else {
      if (xTaskCreatePinnedToCore(
              taskEntryPointShim, this->taskName.c_str(), this->stackSize, this,
              this->espPriority + CONFIG_PTHREAD_TASK_PRIO_DEFAULT,
              &xTaskHandle, getFreeRTOSTaskCore()) != pdPASS) {
        xTaskHandle = nullptr;
      }
    }

    if (xTaskHandle == nullptr) {
      task->taskRunning = false;
      return false;
    }
    taskStarted = true;
    return true;
  }

  void stopTask(Task* task) {
    task->taskRunning = false;
    task->wakeTask();
    if (taskStarted && doneSem != nullptr) {
      // Wait for the task's run body to complete. The semaphore is given
      // by taskEntryPointShim just before self-delete.
      xSemaphoreTake(doneSem, portMAX_DELAY);
      taskStarted = false;
    }
  }

 private:
  int stackSize = 0;
  TaskCore espTaskCore;
  bool espStackOnPsram = false;
  int espPriority;
  std::string taskName;

  StaticTask_t* xTaskBuffer = nullptr;
  StackType_t* xStack = nullptr;
  TaskHandle_t xTaskHandle = nullptr;
  Task* taskPtr = nullptr;
  SemaphoreHandle_t doneSem = nullptr;
  bool taskStarted = false;

  // Returns the FreeRTOS task core
  BaseType_t getFreeRTOSTaskCore() {
    switch (espTaskCore) {
      case TaskCore::Core0:
        return 0;
      case TaskCore::Core1:
        return 1;
      case TaskCore::CoreAny:
        return tskNO_AFFINITY;
    }

    return tskNO_AFFINITY;
  }
};

// Task constructor and member methods
Task::Task(const std::string& taskName, int stackSize, int espPriority,
           TaskCore espTaskCore, bool espStackOnPsram)
    : pImpl(std::make_unique<Impl>(taskName, stackSize, espPriority,
                                   espTaskCore, espStackOnPsram)) {}

Task::~Task() {
  // Safety net; derived destructors should already have stopped the task.
  stopTask();
}

bool Task::startTask() {
  return pImpl->startTask(this);
}

void Task::stopTask() {
  pImpl->stopTask(this);
}

size_t Task::getStackHighWaterMarkWords() const {
  return pImpl->getStackHighWaterMarkWords();
}
