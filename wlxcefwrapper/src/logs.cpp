#include "logs.hpp"
#include <ctime>
#include <pthread.h>

namespace logs {
struct Time {
  uint32_t year;
  uint8_t month;
  uint8_t day;
  uint8_t hour;
  uint8_t min;
  uint8_t sec;
  uint32_t msec; // resets every second
};

Time getCurrentTime() {
  struct timespec spec;
  clock_gettime(CLOCK_REALTIME, &spec);
  time_t raw;
  raw = spec.tv_sec;
  auto *timeinfo = localtime(&raw);

  Time time;
  time.year = timeinfo->tm_year + 1900;
  time.month = timeinfo->tm_mon + 1;
  time.day = timeinfo->tm_mday;
  time.hour = timeinfo->tm_hour;
  time.min = timeinfo->tm_min;
  time.sec = timeinfo->tm_sec;
  time.msec = spec.tv_nsec / 1000000;
  return time;
}

std::string getThreadName() {
  char buf[20];
  if (pthread_getname_np(pthread_self(), buf, sizeof(buf)) == 0) {
    return std::string(buf);
  }
  return {};
}

void logsPrintHeader(FILE *target) {
  auto time = getCurrentTime();

  const char *color_code = "\e[1;37m";

  char buf_thread_name[32];
  buf_thread_name[0] = 0x00;

  auto thread_name = getThreadName();
  if (!thread_name.empty()) {
    snprintf(buf_thread_name, sizeof(buf_thread_name), "\e[0;31m[%s]",
             thread_name.c_str());
  }

  char buf[80];
  snprintf(buf, sizeof(buf),
           "\033[0;90m[%02d-%02d-%02d %02d:%02d:%02d.%03d]%s %s",
           time.year % 100, time.month, time.day, time.hour, time.min, time.sec,
           time.msec, buf_thread_name, color_code);

  fwrite(buf, 1, strlen(buf), target);
}
} // namespace logs