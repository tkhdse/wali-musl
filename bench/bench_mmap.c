 #include <sys/mman.h>
 #include <time.h>
 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
 #include <stdint.h>
 
 /* ------------------------------------------------------------------ config */
 
 #define WARMUP_ITERS  500
 #define BENCH_ITERS   5000
 
 static const size_t SIZES[] = {
     4096,           /*   4 KiB –  1 page  */
     16384,          /*  16 KiB –  4 pages */
     65536,          /*  64 KiB – 16 pages */
     262144,         /* 256 KiB – 64 pages */
     1048576,        /*   1 MiB            */
     4194304,        /*   4 MiB            */
     16777216,       /*  16 MiB            */
 };
 #define N_SIZES ((int)(sizeof(SIZES) / sizeof(SIZES[0])))
 
 /* ----------------------------------------------------------------- helpers */
 
 static inline uint64_t clock_ns(void)
 {
     struct timespec ts;
     clock_gettime(CLOCK_MONOTONIC, &ts);
     return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
 }
 
 typedef struct { double mean, stddev, lo, hi; } stats_t;
 
 static stats_t compute_stats(const double *s, int n)
 {
     stats_t r = { .lo = s[0], .hi = s[0] };
     double sum = 0.0;
     for (int i = 0; i < n; i++) {
         sum += s[i];
         if (s[i] < r.lo) r.lo = s[i];
         if (s[i] > r.hi) r.hi = s[i];
     }
     r.mean = sum / n;
     double var = 0.0;
     for (int i = 0; i < n; i++) {
         double d = s[i] - r.mean;
         var += d * d;
     }
     /* manual sqrt via Newton's method – avoids -lm dependency */
     double x = var / n;
     if (x > 0.0) {
         double est = x;
         for (int k = 0; k < 40; k++) est = 0.5 * (est + x / est);
         r.stddev = est;
     }
     return r;
 }
 
 static void print_row(const char *tag, size_t sz, const stats_t *s)
 {
     /* pretty-print size */
     char szbuf[16];
     if      (sz >= 1048576) snprintf(szbuf, sizeof szbuf, "%zuM", sz >> 20);
     else if (sz >= 1024)    snprintf(szbuf, sizeof szbuf, "%zuK", sz >> 10);
     else                    snprintf(szbuf, sizeof szbuf, "%zuB", sz);
 
     printf("%-22s  %-7s  %10.1f  %10.1f  %10.1f  %10.1f\n",
            tag, szbuf, s->mean, s->stddev, s->lo, s->hi);
 }
 
 /* ------------------------------------------------------- timer calibration */
 
 static double measure_timer_overhead(void)
 {
     double samples[1000];
     for (int i = 0; i < 1000; i++) {
         uint64_t a = clock_ns();
         uint64_t b = clock_ns();
         samples[i] = (double)(b - a);
     }
     stats_t s = compute_stats(samples, 1000);
     return s.mean;
 }
 
 /* --------------------------------------------------------- mmap benchmarks */
 
 /*
  * bench_mmap_separate: isolates mmap and munmap latency independently.
  * mmap_out[i] = cost of the mmap call alone (ns)
  * unmap_out[i] = cost of the munmap call alone (ns)
  */
 static void bench_mmap_separate(size_t sz, double *mmap_out, double *unmap_out, int n)
 {
     for (int i = 0; i < n; i++) {
         uint64_t t0 = clock_ns();
         void *p = mmap(NULL, sz, PROT_READ | PROT_WRITE,
                        MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
         uint64_t t1 = clock_ns();
 
         if (p == MAP_FAILED) {
             fprintf(stderr, "mmap(%zu) failed at iter %d\n", sz, i);
             exit(1);
         }
         mmap_out[i] = (double)(t1 - t0);
 
         t0 = clock_ns();
         munmap(p, sz);
         t1 = clock_ns();
         unmap_out[i] = (double)(t1 - t0);
     }
 }
 
 /*
  * bench_mmap_touch: end-to-end cost of mmap + writing first & last page
  * (to trigger demand paging) + munmap.  Captures full fault overhead.
  */
 static void bench_mmap_touch(size_t sz, double *out, int n)
 {
     for (int i = 0; i < n; i++) {
         uint64_t t0 = clock_ns();
         volatile char *p = mmap(NULL, sz, PROT_READ | PROT_WRITE,
                                 MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
         if ((void *)p == MAP_FAILED) {
             fprintf(stderr, "mmap(%zu) failed at iter %d\n", sz, i);
             exit(1);
         }
         p[0]      = 1;   /* first page fault */
         p[sz - 1] = 1;   /* last page fault  */
         munmap((void *)p, sz);
         uint64_t t1 = clock_ns();
         out[i] = (double)(t1 - t0);
     }
 }
 
 /*
  * bench_mmap_mprotect: maps once, then benchmarks mprotect round-trips
  * (RW -> RO -> RW) to isolate permission-change syscall cost.
  */
 static void bench_mprotect(size_t sz, double *out, int n)
 {
     void *p = mmap(NULL, sz, PROT_READ | PROT_WRITE,
                    MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
     if (p == MAP_FAILED) {
         fprintf(stderr, "mmap(%zu) failed in mprotect bench\n", sz);
         exit(1);
     }
     for (int i = 0; i < n; i++) {
         uint64_t t0 = clock_ns();
         mprotect(p, sz, PROT_READ);
         mprotect(p, sz, PROT_READ | PROT_WRITE);
         uint64_t t1 = clock_ns();
         out[i] = (double)(t1 - t0);
     }
     munmap(p, sz);
 }
 
 /* ----------------------------------------------------------------------- */
 
 int main(void)
 {
     double *a = malloc(BENCH_ITERS * sizeof(double));
     double *b = malloc(BENCH_ITERS * sizeof(double));
     double *c = malloc(BENCH_ITERS * sizeof(double));
     if (!a || !b || !c) { fputs("malloc failed\n", stderr); return 1; }
 
     printf("WALI mmap syscall overhead benchmark\n");
     printf("warmup=%d  bench_iters=%d\n", WARMUP_ITERS, BENCH_ITERS);
 
     double timer_overhead = measure_timer_overhead();
     printf("clock_gettime overhead (mean): %.1f ns  "
            "(subtract from 1-call benchmarks)\n\n", timer_overhead);
 
     printf("%-22s  %-7s  %10s  %10s  %10s  %10s\n",
            "benchmark", "size", "mean(ns)", "stddev(ns)", "min(ns)", "max(ns)");
     printf("%-22s  %-7s  %10s  %10s  %10s  %10s\n",
            "----------------------", "-------",
            "----------", "----------", "----------", "----------");
 
     for (int si = 0; si < N_SIZES; si++) {
         size_t sz = SIZES[si];
 
         /* --- mmap (anonymous) --- */
         bench_mmap_separate(sz, a, b, WARMUP_ITERS);  /* warmup */
         bench_mmap_separate(sz, a, b, BENCH_ITERS);
         stats_t ms = compute_stats(a, BENCH_ITERS);
         stats_t us = compute_stats(b, BENCH_ITERS);
         print_row("mmap(anon)",   sz, &ms);
         print_row("munmap(anon)", sz, &us);
 
         /* --- mmap+touch+munmap (small/medium sizes only) --- */
         if (sz <= 4194304) {
             bench_mmap_touch(sz, c, WARMUP_ITERS);
             bench_mmap_touch(sz, c, BENCH_ITERS);
             stats_t ts = compute_stats(c, BENCH_ITERS);
             print_row("mmap+touch+munmap", sz, &ts);
         }
 
         /* --- mprotect round-trip (small/medium sizes only) --- */
         if (sz <= 4194304) {
             bench_mprotect(sz, c, WARMUP_ITERS);
             bench_mprotect(sz, c, BENCH_ITERS);
             stats_t ps = compute_stats(c, BENCH_ITERS);
             print_row("mprotect(RW->RO->RW)", sz, &ps);
         }
 
         putchar('\n');
     }
 
     free(a); free(b); free(c);
     return 0;
 }