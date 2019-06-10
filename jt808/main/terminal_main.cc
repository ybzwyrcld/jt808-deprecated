#include <sys/time.h>

#include <stdint.h>

#include "terminal/jt808_terminal.h"


uint64_t inline CalculatingTimeMs(const struct timeval &time_begin,
                                  const struct timeval &time_end) {
  uint64_t begin = static_cast<uint64_t>(time_begin.tv_sec) * 1000;
  begin += (time_begin.tv_usec / 1000);

  uint64_t end = static_cast<uint64_t>(time_end.tv_sec) * 1000;
  end += (time_end.tv_usec / 1000);

  return (end - begin);
}

// static void DelayInMs(const uint64_t &millisecond) {
//   struct timeval time_begin;
//   struct timeval time_end;
// 
//   gettimeofday(&time_begin, NULL);
//   do {
//     gettimeofday(&time_end, NULL);
//   } while (CalculatingTimeMs(time_begin, time_end) < millisecond);
// }

int main(int argc, char *argv[]) {
  int retval = -1;
  int report_interval = 1;
  struct timeval time_begin = {0};
  struct timeval time_end = {0};
  Jt808Terminal my_terminal;

  Jt808Info jt808_info = {"127.0.0.1", 8193, "13826539850", 10};
  my_terminal.set_jt808_info(jt808_info);

  if (my_terminal.ConnectRemote() != 0) {
    exit(0);
  }

  while (1) {
    gettimeofday(&time_begin, NULL);
    if (--report_interval == 0) {
      report_interval = my_terminal.report_interval();
      if (my_terminal.ReportPosition() < 0) {
        break;
      }
    }
    do {
      if ((retval = my_terminal.RecvFrameData()) > 0) {
        my_terminal.Jt808FrameParse();
      }
      gettimeofday(&time_end, NULL);
    } while (CalculatingTimeMs(time_begin, time_end) < 1000);
  }

  my_terminal.ClearConnect();
  return 0;
}

