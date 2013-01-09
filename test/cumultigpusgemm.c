#include "blas.h"
#include "error.h"
#include <stdio.h>
#include <math.h>
#include <float.h>
#include <sys/time.h>
#include "sgemm_ref.c"

/**
 * Test program for multiGPU SGEMM.
 *
 * Block Sizes:
 *
 * From the CUDA Programming Guide each GPU multiprocessor requires at least 192
 * threads/6 warps to hide global memory latency.  The number of thread blocks
 * should also be as large as possible so that there are enough to distribute
 * among future GPUs with more multiprocessors.
 *
 * These calculations give the minimum block sizes to use for maximum
 * performance when executing GPU SGEMM on one or more GTX 280s with arguments
 * in host memory.
 *
 * When transA == CBlasNoTrans each GPU multiprocessor processes 64x16 blocks of
 * C using 64 threads.  A minimum of 3 blocks is required to mask global memory
 * latency.  Due to register and shared memory requirements a maximum of 8
 * blocks can fit concurrently on each multiprocessor.  This is enough to hide
 * global memory latency and is the minimum number of blocks needed to fully
 * utilise the GPU and give maximum performance.
 *
 * Since there are 30 multiprocessors on a GTX 280 GPU 240 blocks need to be
 * scheduled to give each the 8 blocks required to get maximum performance.
 * Valid block sizes are listed in the table below along with the bandwidth
 * reduction provided by the block size.  The performance across all block sizes
 * is constant for a given k.
 *
 * -------------------------------------------
 * | Factors |   Overall  | Bandwidth |  k   |
 * | of 240  | Block Size | Reduction |      |
 * -------------------------------------------
 * |   1x240 |    64x3840 |   125.90  |   16 |
 * |   2x120 |   128x1920 |   240.00  |  112 |
 * |   3x 80 |   192x1280 |   333.91  |  192*|
 * |   4x 60 |   256x 960 |   404.21  |  192*|
 * |   5x 48 |   320x 768 |   451.76  |  192*|
 * |   6x 40 |   384x 640 |   480.00  |  192*|
 * |   8x 30 |   448x 480 |   463.45  |  192*|
 * |  10x 24 |   640x 384 |   480.00  |  192*|
 * |  12x 20 |   768x 320 |   451.76  |  192*|
 * |  15x 16 |   960x 256 |   404.21  |  192*|
 * |  16x 15 |  1024x 240 |   388.86  |  192*|
 * |  20x 12 |  1280x 192 |   333.91  |  192*|
 * |  24x 10 |  1536x 160 |   289.81  |  192*|
 * |  30x  8 |  1920x 128 |   240.00  |  112 |
 * |  40x  6 |  2560x  96 |   185.06  |   32 |
 * |  48x  5 |  3072x  80 |   155.94  |   16 |
 * |  60x  4 |  3840x  64 |   125.90  |   16 |
 * |  80x  3 |  5120x  48 |    95.11  |   16 |
 * | 120x  2 |  7680x  32 |    63.73  |   16 |
 * | 240x  1 | 15360x  16 |    31.97  |   16 |
 * -------------------------------------------
 * (*minimum value to be compute bound - throughput cannot outperform bandwidth)
 *
 * The GPU is connected to main memory by a PCI Express 2.0 x16 bus.  Using the
 * bandwidth-test benchmark in the minibench directory it is found that this
 * will transfer data at a minimum of 5.5 GB/s with a maximum of 0.06 ms latency
 * (depending on whether it is host-device, device-host and if there is a
 * display attached to the GPU).  Since the internal bandwidth of the GPU is far
 * in excess of the PCI bandwidth and the latency of a memory copy is greater
 * than the latency of a kernel launch it is not possible to choose a kb > 0
 * such that the time taken to transfer a block of A and B matches the time
 * taken to process them.  A single tuning run using a block size of 640x384 was
 * used to measure performance for all block sizes when kb varies from 16-2048
 * in steps of 16 (the amount of unrolling applied to the inner loop of the
 * kernel). As performance increases with k (up to a point), kb is chosen to be
 * the maximum value such that the algorithm remains compute bound (unless
 * performance levels off, then it is taken to be the minimum value that gives
 * maximum performance in order to minimise the difference in time taken for
 * transfers).
 *
 *
 * When transA != CBlasNoTrans each GPU multiprocessor processes 32x32 blocks of
 * C using 64 threads.  A minimum of 3 blocks is required to mask global memory
 * latency.  Due to register and shared memory requirements a maximum of 6
 * blocks can fit concurrently on each multiprocessor.  This is enough to hide
 * global memory latency and is the minimum number of blocks needed to give
 * maximum performance.
 *
 * Since there are 30 multiprocessors on a GTX 280 GPU 180 blocks need to be
 * scheduled to give each the 6 blocks required to get maximum performance.
 * Valid block sizes are listed in the table below along with the bandwidth
 * reduction provided by the block size.  The performance across all block sizes
 * is constant for a given k.
 *
 * -------------------------------------------
 * | Factors |   Overall  | Bandwidth |  k   |
 * | of 180  | Block Size | Reduction |      |
 * -------------------------------------------
 * |   1x180 |    32x5760 |    63.65  |   16 |
 * |   2x 90 |    64x2880 |   125.22  |   32 |
 * |   3x 60 |    96x1920 |   182.86  |   80 |
 * |   4x 45 |   128x1440 |   235.10  |  448*|
 * |   5x 36 |   160x1152 |   280.98  |  448*|
 * |   6x 30 |   192x 960 |   320.00  |  448*|
 * |   9x 20 |   288x 640 |   397.24  |  448*|
 * |  10x 18 |   320x 576 |   411.43  |  448*|
 * |  12x 15 |   384x 480 |   426.66  |  448*|
 * |  15x 12 |   480x 384 |   426.66  |  448*|
 * |  18x 10 |   576x 320 |   411.43  |  448*|
 * |  20x  9 |   640x 288 |   397.24  |  448*|
 * |  30x  6 |   960x 192 |   320.00  |  448*|
 * |  36x  5 |  1152x 160 |   280.98  |  448*|
 * |  45x  4 |  1440x 128 |   235.10  |  448*|
 * |  60x  3 |  1920x  96 |   182.86  |   80 |
 * |  90x  2 |  2880x  64 |   125.22  |   32 |
 * | 180x  1 |  5760x  32 |    63.65  |   16 |
 * -------------------------------------------
 * (*minimum value to be compute bound - throughput cannot outperform bandwidth)
 *
 * The GPU is connected to main memory by a PCI Express 2.0 x16 bus.  Using the
 * bandwidth-test benchmark in the minibench directory it is found that this
 * will transfer data at a minimum of 5.5 GB/s with a maximum of 0.06 ms latency
 * (depending on whether it is host-device, device-host and if there is a
 * display attached to the GPU).  Since the internal bandwidth of the GPU is far
 * in excess of the PCI bandwidth and the latency of a memory copy is greater
 * than the latency of a kernel launch it is not possible to choose a kb > 0
 * such that the time taken to transfer a block of A and B matches the time
 * taken to process them.  A single tuning run using a block size of 480x384 was
 * used to measure performance for all block sizes when kb varies from 8-2048
 * in steps of 8 (the amount of unrolling applied to the inner loop of the
 * kernel). As performance increases with k (up to a point), kb is chosen to be
 * the maximum value such that the algorithm remains compute bound (unless
 * performance levels off, then it is taken to be the minimum value that gives
 * maximum performance in order to minimise the difference in time taken for
 * transfers).
 *
 */

int main(int argc, char * argv[]) {
  CBlasTranspose transA, transB;
  size_t m, n, k;

  if (argc != 6) {
    fprintf(stderr, "Usage: %s <transA> <transB> <m> <n> <k>\n"
                    "where:\n"
                    "  transA and transB  are 'n' or 'N' for CBlasNoTrans, 't' or 'T' for CBlasTrans or 'c' or 'C' for CBlasConjTrans\n"
                    "  m, n and k         are the sizes of the matrices\n", argv[0]);
    return 1;
  }

  char t;
  if (sscanf(argv[1], "%c", &t) != 1) {
    fprintf(stderr, "Unable to read character from '%s'\n", argv[1]);
    return 1;
  }
  switch (t) {
    case 'N': case 'n': transA = CBlasNoTrans; break;
    case 'T': case 't': transA = CBlasTrans; break;
    case 'C': case 'c': transA = CBlasConjTrans; break;
    default: fprintf(stderr, "Unknown transpose '%c'\n", t); return 1;
  }

  if (sscanf(argv[2], "%c", &t) != 1) {
    fprintf(stderr, "Unable to read character from '%s'\n", argv[2]);
    return 2;
  }
  switch (t) {
    case 'N': case 'n': transB = CBlasNoTrans; break;
    case 'T': case 't': transB = CBlasTrans; break;
    case 'C': case 'c': transB = CBlasConjTrans; break;
    default: fprintf(stderr, "Unknown transpose '%c'\n", t); return 1;
  }

  if (sscanf(argv[3], "%zu", &m) != 1) {
    fprintf(stderr, "Unable to parse number from '%s'\n", argv[3]);
    return 3;
  }

  if (sscanf(argv[4], "%zu", &n) != 1) {
    fprintf(stderr, "Unable to parse number from '%s'\n", argv[4]);
    return 4;
  }

  if (sscanf(argv[5], "%zu", &k) != 1) {
    fprintf(stderr, "Unable to parse number from '%s'\n", argv[5]);
    return 5;
  }

  srand(0);
  float alpha, beta, * A, * B, * C, * refC;
  size_t lda, ldb, ldc;

  CU_ERROR_CHECK(cuInit(0));

  int deviceCount;
  CU_ERROR_CHECK(cuDeviceGetCount(&deviceCount));

  CUdevice devices[deviceCount];
  for (int i = 0; i < deviceCount; i++)
    CU_ERROR_CHECK(cuDeviceGet(&devices[i], i));

  CUmultiGPU mGPU;
  CU_ERROR_CHECK(cuMultiGPUCreate(&mGPU, devices, deviceCount));

  alpha = (float)rand() / (float)RAND_MAX;
  beta = (float)rand() / (float)RAND_MAX;

  if (transA == CBlasNoTrans) {
    lda = (m + 3u) & ~3u;
    if ((A = malloc(lda * k * sizeof(float))) == NULL) {
      fputs("Unable to allocate A\n", stderr);
      return -1;
    }

    for (size_t j = 0; j < k; j++) {
      for (size_t i = 0; i < m; i++)
        A[j * lda + i] = (float)rand() / (float)RAND_MAX;
    }
  }
  else {
    lda = (k + 3u) & ~3u;
    if ((A = malloc(lda * m * sizeof(float))) == NULL) {
      fputs("Unable to allocate A\n", stderr);
      return -1;
    }

    for (size_t j = 0; j < m; j++) {
      for (size_t i = 0; i < k; i++)
        A[j * lda + i] = (float)rand() / (float)RAND_MAX;
    }
  }

  if (transB == CBlasNoTrans) {
    ldb = (k + 3u) & ~3u;
    if ((B = malloc(ldb * n * sizeof(float))) == NULL) {
      fputs("Unable to allocate B\n", stderr);
      return -2;
    }

    for (size_t j = 0; j < n; j++) {
      for (size_t i = 0; i < k; i++)
        B[j * ldb + i] = (float)rand() / (float)RAND_MAX;
    }
  }
  else {
    ldb = (n + 3u) & ~3u;
    if ((B = malloc(ldb * k * sizeof(float))) == NULL) {
      fputs("Unable to allocate B\n", stderr);
      return -2;
    }

    for (size_t j = 0; j < k; j++) {
      for (size_t i = 0; i < n; i++)
        B[j * ldb + i] = (float)rand() / (float)RAND_MAX;
    }
  }

  ldc = (m + 3u) & ~3u;
  if ((C = malloc(ldc * n * sizeof(float))) == NULL) {
    fputs("Unable to allocate C\n", stderr);
    return -3;
  }
  if ((refC = malloc(ldc * n * sizeof(float))) == NULL) {
    fputs("Unable to allocate refC\n", stderr);
    return -4;
  }

  for (size_t j = 0; j < n; j++) {
    for (size_t i = 0; i < m; i++)
      refC[j * ldc + i] = C[j * ldc + i] = (float)rand() / (float)RAND_MAX;
  }

  CUmultiGPUSBlasConfig config;
  CU_ERROR_CHECK(cuMultiGPUSBlasConfigCreate(&config, mGPU, transA, transB,
                                             (transA == CBlasNoTrans) ? 640 : 384,
                                             (transA == CBlasNoTrans) ? 384 : 480,
                                             (transA == CBlasNoTrans) ? 192 : 448));

  sgemm_ref(transA, transB, m, n, k, alpha, A, lda, B, ldb, beta, refC, ldc);
  CU_ERROR_CHECK(cuMultiGPUSgemm(config, transA, transB, m, n, k,
                                 alpha, A, lda, B, ldb, beta, C, ldc));

  float diff = 0.0f;
  for (size_t j = 0; j < n; j++) {
    for (size_t i = 0; i < m; i++) {
      float d = fabsf(C[j * ldc + i] - refC[j * ldc + i]);
      if (d > diff)
        diff = d;
    }
  }

  struct timeval start, stop;
  if (gettimeofday(&start, NULL) != 0) {
    fputs("gettimeofday failed\n", stderr);
    return -5;
  }
  for (size_t i = 0; i < 20; i++)
  CU_ERROR_CHECK(cuMultiGPUSgemm(config, transA, transB, m, n, k,
                                 alpha, A, lda, B, ldb, beta, C, ldc));
  CU_ERROR_CHECK(cuMultiGPUSynchronize(mGPU));
  if (gettimeofday(&stop, NULL) != 0) {
    fputs("gettimeofday failed\n", stderr);
    return -6;
  }

  double time = ((double)(stop.tv_sec - start.tv_sec) +
                 (double)(stop.tv_usec - start.tv_usec) * 1.e-6) / 20.0;

  size_t flops = 2 * k - 1;
  if (alpha != 1.0f)
    flops += 1;
  if (beta != 0.0f)
    flops += 2;
  float error = (float)flops * 2.0f * FLT_EPSILON;
  flops *= m * n;

  bool passed = (diff <= error);
  fprintf(stdout, "%.3es %.3gGFlops/s Error: %.3e\n%sED!\n", time,
          ((double)flops * 1.e-9) / time, diff, (passed) ? "PASS" : "FAIL");

  free(A);
  free(B);
  free(C);
  free(refC);

  CU_ERROR_CHECK(cuMultiGPUSBlasConfigDestroy(config));
  CU_ERROR_CHECK(cuMultiGPUDestroy(mGPU));

  return (int)!passed;
}
