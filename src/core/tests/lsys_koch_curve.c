#define _POSIX_C_SOURCE 199309L
#include "src/core/include/lsys.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#define BUFCAP 32768
#define BENCH_ITERS 100

static int failures;

static void check(int condition, const char *message)
{
  if(condition) return;
  fprintf(stderr, "FAIL: %s\n", message);
  failures++;
}

static double elapsed_ms(struct timespec start, struct timespec end)
{
  return (double)(end.tv_sec - start.tv_sec) * 1000.0 +
         (double)(end.tv_nsec - start.tv_nsec) / 1000000.0;
}

static void check_generation(lsys G, u32 n, usize expected_len)
{
  char  buf[BUFCAP];
  char  scratch[BUFCAP];
  usize nr_f    = 0;
  usize nr_plus = 0;
  usize nr_minus= 0;

  memset(buf, 0, sizeof buf);
  check(
    lsysrep(G, n, buf, sizeof buf, scratch, sizeof scratch) == 0,
    "lsysrep should succeed");
  check(strlen(buf) == expected_len, "generated length should match");
  check(
    lsysqry_bufsz(G, n) == (isize)expected_len + 1,
    "queried buffer size should include the null terminator");

  for(const char *p= buf; *p; p++)
  {
    nr_f+= *p == 'F';
    nr_plus+= *p == '+';
    nr_minus+= *p == '-';
  }

  usize pow5= 1;
  for(u32 i= 0; i < n; i++) pow5*= 5;

  check(nr_f == pow5, "generation should contain 5^n instances of F");
  check(
    nr_plus == (pow5 - 1) / 2,
    "generation should contain the expected number of plus signs");
  check(
    nr_minus == (pow5 - 1) / 2,
    "generation should contain the expected number of minus signs");

  isize prevsz= lsysqry_bufsz(G, n - 1);
  printf(
    "%3u  %12zu  %14zu  %14zu\n",
    n,
    expected_len,
    (usize)prevsz + expected_len + 1,
    sizeof buf + sizeof scratch);
}

static void benchmark(lsys G)
{
  char              buf[BUFCAP];
  char              scratch[BUFCAP];
  struct timespec   start, end;
  volatile unsigned checksum= 0;

  clock_gettime(CLOCK_MONOTONIC, &start);
  for(usize i= 0; i < BENCH_ITERS; i++)
  {
    int rc= lsysrep(G, 6, buf, sizeof buf, scratch, sizeof scratch);
    if(rc != 0)
    {
      check(0, "benchmark generation should succeed");
      return;
    }
    checksum+= (unsigned char)buf[0] + (unsigned char)buf[31248];
  }
  clock_gettime(CLOCK_MONOTONIC, &end);

  double ms            = elapsed_ms(start, end);
  double us_per_rep    = ms * 1000.0 / BENCH_ITERS;
  double chars_per_rep = 9.0 + 49.0 + 249.0 + 1249.0 + 6249.0 + 31249.0;
  double mchars_per_sec= chars_per_rep * BENCH_ITERS / (ms * 1000.0);

  printf("\nBenchmark: generation 6, %u repetitions\n", BENCH_ITERS);
  printf("  elapsed:       %.3f ms\n", ms);
  printf("  average:       %.3f us/repetition\n", us_per_rep);
  printf("  throughput:    %.3f million generated chars/s\n", mchars_per_sec);
  printf("  checksum:      %u\n", checksum);
}

int main(void)
{
  lsys G= {
    .axm= "F", .P= {{.pred= 'F', .succ= "F+F-F-F+F"}}, .nP= 1, .V= "F+-"};

  char buf[BUFCAP]    = {0};
  char scratch[BUFCAP]= {0};

  check(
    lsysrep(G, 1, buf, sizeof buf, scratch, sizeof scratch) == 0,
    "first generation should succeed");
  check(
    strcmp(buf, "F+F-F-F+F") == 0,
    "first generation should equal the Koch production");

  puts("Generation diagnostics:");
  puts("  n        chars  minimum bytes*  reserved bytes");
  check_generation(G, 1, 9);
  check_generation(G, 2, 49);
  check_generation(G, 3, 249);
  check_generation(G, 4, 1249);
  check_generation(G, 5, 6249);
  check_generation(G, 6, 31249);
  puts("  * previous and current strings, including terminators");

  if(failures)
  {
    fprintf(stderr, "%d Koch curve test(s) failed\n", failures);
    return 1;
  }

  benchmark(G);
  if(failures) return 1;

  puts("Koch curve tests passed");
  return 0;
}
