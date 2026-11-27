// freetest: kiểm tra rò rỉ trang vật lý của kernel.
// Đếm số trang trống (syscall freepages) trước và sau khi tạo rồi thu hồi
// nhiều tiến trình; hai số phải bằng nhau vì mọi trang của tiến trình con
// (bảng trang, trapframe, bộ nhớ user) được trả lại trong wait().
// Thoát với mã 0 nếu mọi ca PASS, mã 1 nếu có ca FAIL.
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define FORK_CYCLES 1000
#define SBRK_CYCLES 100
#define SBRK_BYTES  (64 * 1024)

// Báo kết quả một ca; trả về 1 nếu PASS.
static int
report(char *name, int before, int after)
{
  if (before == after) {
    printf("PASS %s before=%d after=%d\n", name, before, after);
    return 1;
  }
  printf("FAIL %s before=%d after=%d\n", name, before, after);
  return 0;
}

// Ca 1: tạo rồi thu hồi tiến trình con thoát ngay.
static int
leak_fork(void)
{
  int before = freepages();

  for (int i = 0; i < FORK_CYCLES; i++) {
    int pid = fork();
    if (pid < 0) {
      printf("FAIL leak_fork: fork that bai o vong %d\n", i);
      return 0;
    }
    if (pid == 0)
      exit(0);
    wait(0);
  }
  return report("leak_fork", before, freepages());
}

// Ca 2: tiến trình con xin thêm bộ nhớ user rồi thoát.
static int
leak_sbrk(void)
{
  int before = freepages();

  for (int i = 0; i < SBRK_CYCLES; i++) {
    int pid = fork();
    if (pid < 0) {
      printf("FAIL leak_sbrk: fork that bai o vong %d\n", i);
      return 0;
    }
    if (pid == 0) {
      if (sbrk(SBRK_BYTES) == SBRK_ERROR)
        exit(1);
      exit(0);
    }
    int st;
    wait(&st);
    if (st != 0) {
      printf("FAIL leak_sbrk: sbrk that bai o vong %d\n", i);
      return 0;
    }
  }
  return report("leak_sbrk", before, freepages());
}

int
main(void)
{
  int ok = 1;

  ok &= leak_fork();
  ok &= leak_sbrk();
  exit(ok ? 0 : 1);
}
