#include "lapack.h"
#include "error.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <float.h>
#include <math.h>
#include "ref/dpotrf_ref.c"
#include "util/dlatmc.c"

int main(int argc, char * argv[]) {
  CBlasUplo uplo;
  size_t n;
  int d = 0;

  if (argc < 3 || argc > 4) {
    fprintf(stderr, "Usage: %s <uplo> <n>\n"
                    "where:\n"
                    "  uplo    is 'u' or 'U' for CBlasUpper or 'l' or 'L' for CBlasLower\n"
                    "  n       is the size of the matrix\n"
                    "  device  is the GPU to use (default 0)\n", argv[0]);
    return 1;
  }

  char u;
  if (sscanf(argv[1], "%c", &u) != 1) {
    fprintf(stderr, "Unable to read character from '%s'\n", argv[1]);
    return 1;
  }
  switch (u) {
    case 'U': case 'u': uplo = CBlasUpper; break;
    case 'L': case 'l': uplo = CBlasLower; break;
    default: fprintf(stderr, "Unknown uplo '%c'\n", u); return 1;
  }

  if (sscanf(argv[2], "%zu", &n) != 1) {
    fprintf(stderr, "Unable to parse number from '%s'\n", argv[2]);
    return 2;
  }

  srand(0);

  double * A, * refA;
  CUdeviceptr dA;
  size_t lda, dlda;
  long info, rInfo;

  CU_ERROR_CHECK(cuInit(0));

  CUdevice device;
  CU_ERROR_CHECK(cuDeviceGet(&device, d));

  CUcontext context;
  CU_ERROR_CHECK(cuCtxCreate(&context, CU_CTX_SCHED_BLOCKING_SYNC, device));

  CULAPACKhandle handle;
  CU_ERROR_CHECK(cuLAPACKCreate(&handle));

  lda = (n + 1u) & ~1u;
  if ((A = malloc(lda *  n * sizeof(double))) == NULL) {
    fputs("Unable to allocate A\n", stderr);
    return -1;
  }
  if ((refA = malloc(lda * n * sizeof(double))) == NULL) {
    fputs("Unable to allocate refA\n", stderr);
    return -2;
  }
  CU_ERROR_CHECK(cuMemAllocPitch(&dA, &dlda, n * sizeof(double), n, sizeof(double)));
  dlda /= sizeof(double);

  if (dlatmc(n, 2.0, A, lda) != 0) {
    fputs("Unable to initialise A\n", stderr);
    return -1;
  }

  for (size_t j = 0; j < n; j++)
    memcpy(&refA[j * lda], &A[j * lda], n * sizeof(double));

  CUDA_MEMCPY2D copy = { 0, 0, CU_MEMORYTYPE_HOST, A, 0, NULL, lda * sizeof(double),
                         0, 0, CU_MEMORYTYPE_DEVICE, NULL, dA, NULL, dlda * sizeof(double),
                         n * sizeof(double), n };
  CU_ERROR_CHECK(cuMemcpy2D(&copy));

  dpotrf_ref(uplo, n, refA, lda, &rInfo);
  CU_ERROR_CHECK(cuDpotrf(handle, uplo, n, dA, dlda, &info));

  copy = (CUDA_MEMCPY2D){ 0, 0, CU_MEMORYTYPE_DEVICE, NULL, dA, NULL, dlda * sizeof(double),
                          0, 0, CU_MEMORYTYPE_HOST, A, 0, NULL, lda * sizeof(double),
                          n * sizeof(double), n };
  CU_ERROR_CHECK(cuMemcpy2D(&copy));

  bool passed = (info == rInfo);
  double diff = 0.0;
  for (size_t j = 0; j < n; j++) {
    for (size_t i = 0; i < n; i++) {
      double d = fabs(A[j * lda + i] - refA[j * lda + i]);
      if (d > diff)
        diff = d;
    }
  }

  // Set A to identity so that repeated applications of the cholesky
  // decomposition while benchmarking do not exit early due to
  // non-positive-definite-ness.
  for (size_t j = 0; j < n; j++) {
    for (size_t i = 0; i < n; i++)
      A[j * lda + i] = (i == j) ? 1.0 : 0.0;
  }

  copy = (CUDA_MEMCPY2D){ 0, 0, CU_MEMORYTYPE_HOST, A, 0, NULL, lda * sizeof(double),
                          0, 0, CU_MEMORYTYPE_DEVICE, NULL, dA, NULL, dlda * sizeof(double),
                          n * sizeof(double), n };
  CU_ERROR_CHECK(cuMemcpy2D(&copy));

  CUevent start, stop;
  CU_ERROR_CHECK(cuEventCreate(&start, CU_EVENT_BLOCKING_SYNC));
  CU_ERROR_CHECK(cuEventCreate(&stop, CU_EVENT_BLOCKING_SYNC));

  CU_ERROR_CHECK(cuEventRecord(start, NULL));
  for (size_t i = 0; i < 20; i++)
    CU_ERROR_CHECK(cuDpotrf(handle, uplo, n, dA, dlda, &info));
  CU_ERROR_CHECK(cuEventRecord(stop, NULL));
  CU_ERROR_CHECK(cuEventSynchronize(stop));

  float time;
  CU_ERROR_CHECK(cuEventElapsedTime(&time, start, stop));
  time /= 20;

  CU_ERROR_CHECK(cuEventDestroy(start));
  CU_ERROR_CHECK(cuEventDestroy(stop));

  size_t flops = ((n * n * n) / 3) + ((n * n) / 2) + (n / 6);
  fprintf(stdout, "%.3es %.3gGFlops/s Error: %.3e\n%sED!\n", time,
          ((float)flops * 1.e-6f) / time, diff, (passed) ? "PASS" : "FAIL");

  free(A);
  free(refA);
  CU_ERROR_CHECK(cuMemFree(dA));

  CU_ERROR_CHECK(cuLAPACKDestroy(handle));

  CU_ERROR_CHECK(cuCtxDestroy(context));

  return (int)!passed;
}
