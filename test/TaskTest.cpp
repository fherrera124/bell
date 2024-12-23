#include <catch2/catch_test_macros.hpp>

#include <iostream>
#include "bell/utils/Task.h"
#include "bell/utils/Utils.h"

class TestTask : public bell::utils::Task {
 public:
  TestTask() : Task("Test Task", 1024), loopCounter(0) { startTask(); }

  // Override taskLoop to increment a counter
  void taskLoop() override {
    ++loopCounter;
    bell::utils::sleepMs(10);
  }

  // Function to get the count of how many times taskLoop was called
  int getLoopCounter() const { return loopCounter; }

 private:
  std::atomic<int> loopCounter;
};

TEST_CASE("bell::utils::Task tests", "[bell::utils::Task]") {
  TestTask task;

  SECTION("task is properly started and stopped") {
    // Allow some time for the task to run
    bell::utils::sleepMs(100);

    // Checking if taskLoop was called at least once
    CHECK(task.getLoopCounter() > 0);

    // Stop the task and give it time to terminate
    task.stopTask();

    int loopCountAfterStop = task.getLoopCounter();
    bell::utils::sleepMs(100);

    // Ensure taskLoop is not called anymore after stopping
    CHECK(task.getLoopCounter() == loopCountAfterStop);
  }
}
