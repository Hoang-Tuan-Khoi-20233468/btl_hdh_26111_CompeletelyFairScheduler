// cfsstress: thử áp lực bộ lập lịch với nhiều tiến trình ngốn CPU.
// Dùng: cfsstress [hogs] [ticks] [limit]
//   hogs  : số tiến trình ngốn CPU (mặc định 50, tối đa 60 vì NPROC = 64)
//   ticks : thời gian chạy (mặc định 100)
//   limit : độ trễ đánh thức tối đa cho phép, tính bằng tick (mặc định 10)
// Một tiến trình "thăm dò" được tạo SAU các hog; nó ngủ 1 tick rồi đo thời
// gian thực tế mới được chạy lại. Bộ lập lịch tốt phải cho nó chạy sớm dù có
// hàng chục hog đang chờ CPU. In dòng cuối: "cfsstress: PASS" hoặc "FAIL".
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define MAXHOG                                                                 \
  60 // NPROC = 64, trừ init, sh, tiến trình cha và tiến trình thăm dò

struct probe_result {
  int samples;
  int max_latency; // tick
  int sum_latency;
};

static int
parse_int(const char *s)
{
  int v = 0;

  while ('0' <= *s && *s <= '9')
    v = v * 10 + (*s++ - '0');
  return v;
}

// Ngốn CPU đến khi uptime() >= end.
static void
hog(int end)
{
  volatile int x = 0;

  while (uptime() < end) {
    for (int i = 0; i < 1000; i++)
      x += i;
  }
  (void)x;
}

// Ngủ 1 tick nhiều lần, ghi lại độ trễ thực tế (tick) của mỗi lần.
static void
probe(int end, int fd)
{
  struct probe_result res = {0, 0, 0};

  while (uptime() < end) {
    int t0 = uptime();
    pause(1);
    int lat = uptime() - t0;
    res.samples++;
    res.sum_latency += lat;
    if (lat > res.max_latency)
      res.max_latency = lat;
  }
  write(fd, &res, sizeof(res));
}

int
main(int argc, char *argv[])
{
  int hogs = 50, ticks = 100, limit = 10;
  int fds[2];
  int forked = 0, reaped = 0, ok = 1;
  struct probe_result res = {0, 0, 0};

  if (argc > 1)
    hogs = parse_int(argv[1]);
  if (argc > 2)
    ticks = parse_int(argv[2]);
  if (argc > 3)
    limit = parse_int(argv[3]);
  if (hogs < 1 || hogs > MAXHOG || ticks < 1 || limit < 1) {
    fprintf(2, "usage: cfsstress [hogs 1..%d] [ticks] [limit]\n", MAXHOG);
    exit(1);
  }
  if (pipe(fds) < 0) {
    fprintf(2, "cfsstress: pipe failed\n");
    exit(1);
  }

  int end = uptime() + ticks;

  for (int i = 0; i < hogs; i++) {
    int pid = fork();
    if (pid < 0) {
      printf("cfsstress: fork that bai o hog thu %d\n", i);
      ok = 0;
      break;
    }
    if (pid == 0) {
      close(fds[0]);
      close(fds[1]);
      hog(end);
      exit(0);
    }
    forked++;
  }

  // Tiến trình thăm dò: được tạo sau cùng nên có pid lớn nhất.
  int ppid = fork();
  if (ppid < 0) {
    printf("cfsstress: fork that bai o tien trinh tham do\n");
    ok = 0;
  } else if (ppid == 0) {
    close(fds[0]);
    probe(end, fds[1]);
    close(fds[1]);
    exit(0);
  } else {
    forked++;
  }

  close(fds[1]);
  if (ppid > 0 && read(fds[0], &res, sizeof(res)) != sizeof(res)) {
    printf("cfsstress: khong doc duoc ket qua cua tien trinh tham do\n");
    ok = 0;
  }
  close(fds[0]);

  while (wait(0) > 0)
    reaped++;

  printf("cfsstress: hogs=%d forked=%d reaped=%d ticks=%d limit=%d\n", hogs,
         forked, reaped, ticks, limit);
  printf("probe: samples=%d max_latency=%d avg_x10=%d\n", res.samples,
         res.max_latency,
         res.samples > 0 ? res.sum_latency * 10 / res.samples : 0);

  if (reaped != forked) {
    printf("cfsstress: thu hoi %d tien trinh nhung da tao %d\n", reaped,
           forked);
    ok = 0;
  }
  if (res.samples < 1) {
    printf("cfsstress: tien trinh tham do khong lay duoc mau nao\n");
    ok = 0;
  }
  if (res.max_latency > limit) {
    printf("cfsstress: do tre toi da %d tick vuot gioi han %d\n",
           res.max_latency, limit);
    ok = 0;
  }
  printf("cfsstress: %s\n", ok ? "PASS" : "FAIL");
  exit(ok ? 0 : 1);
}