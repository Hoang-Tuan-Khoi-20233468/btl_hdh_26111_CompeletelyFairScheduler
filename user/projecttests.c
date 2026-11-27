// projecttests: chạy cả bộ kiểm thử của dự án CFS và in PASS/FAIL từng ca.
// Mỗi ca chạy một chương trình trong tiến trình con; ca PASS nếu con thoát
// với mã 0 (và, với ca có chuỗi mong đợi, in ra chuỗi đó). Ngoài ra kiểm tra
// rò rỉ trang của cả bộ: số trang trống trước và sau phải bằng nhau.
// Dòng cuối: "ALL PROJECT TESTS PASSED" nếu không ca nào FAIL.
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

#define OUTBUF 4096

static char outbuf[OUTBUF];
static int total, failed;

// 1 nếu chuỗi s (dài n byte) chứa chuỗi con sub.
static int
contains(char *s, int n, char *sub)
{
  int m = strlen(sub);

  for (int i = 0; i + m <= n; i++) {
    int j = 0;
    while (j < m && s[i + j] == sub[j])
      j++;
    if (j == m)
      return 1;
  }
  return 0;
}

// Ghi nhận kết quả một ca.
static void
record(char *name, int pass, char *why)
{
  total++;
  if (pass) {
    printf("PASS %s\n", name);
  } else {
    failed++;
    printf("FAIL %s (%s)\n", name, why);
  }
}

// Chạy argv trong tiến trình con. Nếu expect != 0 thì stdout của con được
// chuyển qua pipe: cha in lại và kiểm tra có chứa expect hay không.
static void
run_case(char *name, char **argv, char *expect)
{
  int p[2];
  int len = 0;
  int st;

  if (expect && pipe(p) < 0) {
    record(name, 0, "pipe");
    return;
  }
  int pid = fork();
  if (pid < 0) {
    record(name, 0, "fork");
    return;
  }
  if (pid == 0) {
    if (expect) {
      close(1);
      dup(p[1]);
      close(p[0]);
      close(p[1]);
    }
    exec(argv[0], argv);
    fprintf(2, "projecttests: exec %s that bai\n", argv[0]);
    exit(127);
  }
  if (expect) {
    char chunk[128];
    int n;

    close(p[1]);
    while ((n = read(p[0], chunk, sizeof(chunk))) > 0) {
      write(1, chunk, n);
      if (len + n <= OUTBUF) {
        memmove(outbuf + len, chunk, n);
        len += n;
      }
    }
    close(p[0]);
  }
  wait(&st);
  if (st != 0)
    record(name, 0, "exit code != 0");
  else if (expect && !contains(outbuf, len, expect))
    record(name, 0, "thieu chuoi mong doi");
  else
    record(name, 1, "");
}

int
main(void)
{
  char *testnice[] = {"testnice", 0};
  char *stress50[] = {"cfsstress", "50", "100", "10", 0};
  char *stress60[] = {"cfsstress", "60", "300", "10", 0};
  char *freetest[] = {"freetest", 0};

  int before = freepages();

  run_case("testnice", testnice, "ALL TESTNICE CHECKS PASSED");
  run_case("cfsstress_50_hog", stress50, 0);
  run_case("cfsstress_60_hog", stress60, 0);
  run_case("freetest", freetest, 0);

  int after = freepages();
  if (before == after) {
    record("leak_check_ca_bo_test", 1, "");
  } else {
    printf("leak_check: before=%d after=%d\n", before, after);
    record("leak_check_ca_bo_test", 0, "so trang trong khac nhau");
  }

  printf("projecttests: %d ca, %d FAIL\n", total, failed);
  if (failed == 0) {
    printf("ALL PROJECT TESTS PASSED\n");
    exit(0);
  }
  printf("PROJECT TESTS FAILED\n");
  exit(1);
}
