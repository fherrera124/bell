#include "bell/Utils.h"

#include <unistd.h>  // for usleep

using namespace bell;

timeval bell::utils::millisecondsToTimeval(uint32_t milliseconds) {
  struct timeval tv {};
  tv.tv_sec = milliseconds / 1000;
  tv.tv_usec = static_cast<int32_t>(milliseconds % 1000) * 1000;
  return tv;
}

void bell::utils::sleepMs(uint32_t milliseconds) {
  usleep(milliseconds * 1000);
}
