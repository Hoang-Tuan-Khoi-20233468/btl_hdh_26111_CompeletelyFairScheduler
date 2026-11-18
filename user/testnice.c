#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

struct result {
  int nice;
  uint64 vruntime;
  int ticks;
};

static unsigned int next_seed = 1;

static int
rand(void)
{
  next_seed = next_seed * 1103515245 + 12345;
  return (unsigned int)(next_seed / 65536) % 32768;
}

static void
srand(unsigned int seed)
{
  next_seed = seed;
}

// Sinh giá trị nice âm trong khoảng [-15, -5] để luôn so sánh được với nice = 0
static int
random_negative_nice(void)
{
  return -15 + (rand() % 11);
}

static void
check_setnice(int nice_val, int expected_ret, const char *desc)
{
  int ret = setnice(nice_val);
  if (ret == expected_ret) {
    printf("[PASS] %s: setnice(%d) -> tra ve %d\n", desc, nice_val, ret);
  } else {
    printf("[FAIL] %s: setnice(%d) -> ky vong %d nhung nhan ve %d\n", desc,
           nice_val, expected_ret, ret);
    exit(1);
  }
}

static void
cpu_work(void)
{
  volatile int x = 0;
  for (int i = 0; i < 200000000; i++) {
    x += i;
  }
  (void)x;
}

int
main(int argc, char *argv[])
{
  printf("=== 1. KIEM TRA BIEN SYSTEM CALL SETNICE ===\n");
  check_setnice(-25, -1, "Nho hon bien duoi (-25)");
  check_setnice(-21, -1, "Sat ngoai bien duoi (-21)");
  check_setnice(20, -1, "Sat ngoai bien tren (20)");
  check_setnice(100, -1, "Lon hon bien tren (100)");

  check_setnice(-20, 0, "Tai bien duoi hop le (-20)");
  check_setnice(19, 0, "Tai bien tren hop le (19)");
  check_setnice(0, 0, "Gia tri mac dinh (0)");
  check_setnice(-10, 0, "Gia tri uu tien cao (-10)");
  check_setnice(10, 0, "Gia tri uu tien thap (10)");

  setnice(0);

  printf("\n=== 2. KIEM TRA TOC DO TANG VRUNTIME (NICE AM VS NICE 0) ===\n");
  srand(uptime());
  int nice1 = random_negative_nice(); // Luôn âm: [-15, -5]
  int nice2 = 0;                      // Chuẩn mặc định: 0

  int p1[2], p2[2];
  if (pipe(p1) < 0 || pipe(p2) < 0) {
    printf("[FAIL] Khong tao duoc pipe\n");
    exit(1);
  }

  printf("Cau hinh do luong:\n");
  printf("  -> Tien trinh con 1: nice = %d (uu tien cao)\n", nice1);
  printf("  -> Tien trinh con 2: nice = %d (mac dinh)\n", nice2);
  printf("Dang chay tinh toan CPU...\n");

  int pid1 = fork();
  if (pid1 == 0) {
    close(p1[0]);
    setnice(nice1);
    int t_start = uptime();
    cpu_work();
    int t_end = uptime();

    struct result res;
    res.nice = nice1;
    res.vruntime = getvruntime();
    res.ticks = t_end - t_start;
    write(p1[1], &res, sizeof(res));
    close(p1[1]);
    exit(0);
  }

  int pid2 = fork();
  if (pid2 == 0) {
    close(p2[0]);
    setnice(nice2);
    int t_start = uptime();
    cpu_work();
    int t_end = uptime();

    struct result res;
    res.nice = nice2;
    res.vruntime = getvruntime();
    res.ticks = t_end - t_start;
    write(p2[1], &res, sizeof(res));
    close(p2[1]);
    exit(0);
  }

  close(p1[1]);
  close(p2[1]);

  struct result r1, r2;
  if (read(p1[0], &r1, sizeof(r1)) != sizeof(r1) ||
      read(p2[0], &r2, sizeof(r2)) != sizeof(r2)) {
    printf("[FAIL] Loi doc du lieu tu pipe\n");
    exit(1);
  }
  close(p1[0]);
  close(p2[0]);

  wait(0);
  wait(0);

  printf("\nKet qua do luong:\n");
  printf("  -> Con 1 (nice = %d): vruntime = %lu, thoi gian = %d ticks\n",
         r1.nice, r1.vruntime, r1.ticks);
  printf("  -> Con 2 (nice = %d): vruntime = %lu, thoi gian = %d ticks\n",
         r2.nice, r2.vruntime, r2.ticks);

  if (r1.vruntime > 0 && r1.vruntime < r2.vruntime) {
    printf(
      "[PASS] Tien trinh nice am (%d) co vruntime tang cham hon nice 0 (%lu < %lu)\n",
      r1.nice, r1.vruntime, r2.vruntime);
    printf("\nALL TESTNICE CHECKS PASSED!\n");
    exit(0);
  } else {
    printf(
      "[FAIL] Ky vong vruntime(nice=%d) < vruntime(nice=0) nhung nhan %lu >= %lu\n",
      r1.nice, r1.vruntime, r2.vruntime);
    exit(1);
  }
}