#include "bell/Utils.h"

using namespace bell;

timeval bell::utils::millisecondsToTimeval(uint32_t milliseconds) {
  struct timeval tv {};
  tv.tv_sec = milliseconds / 1000;
  tv.tv_usec = static_cast<int32_t>(milliseconds % 1000) * 1000;
  return tv;
}
