#include "blas.h"
#include "error.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <float.h>
#include <math.h>
#include <complex.h>
#include "ref/ctrmm_ref.c"

int main(int argc, char * argv[]) {
  CBlasSide side;
  CBlasUplo uplo;
  CBlasTranspose trans;
  CBlasDiag diag;
  size_t m, n;
  int d = 0;

  if (argc < 7 || argc > 8) {
    fprintf(stderr, "Usage: %s <side> <uplo> <trans> <diag> <m> <n> [device]\n"
                    "where:\n"
                    "  side     is 'l' or 'L' for CBlasLeft and 'r' or 'R' for CBlasRight\n"
                    "  uplo     is 'u' or 'U' for CBlasUpper and 'l' or 'L' for CBlasLower\n"
                    "  trans    is 'n' or 'N' for CBlasNoTrans, 't' or 'T' for CBlasTrans or 'c' or 'C' for CBlasConjTrans\n"
                    "  diag     is 'n' or 'N' for CBlasNonUnit and 'u' or 'U' for CBlasUnit\n"
                    "  m and n  are the sizes of the matrices\n"
                    "  device   is the GPU to use (default 0)\n", argv[0]);
    return 1;
  }

  char s;
  if (sscanf(argv[1], "%c", &s) != 1) {
    fprintf(stderr, "Unable to read character from '%s'\n", argv[1]);
    return 1;
  }
  switch (s) {
    case 'L': case 'l': side = CBlasLeft; break;
    case 'R': case 'r': side = CBlasRight; break;
    default: fprintf(stderr, "Unknown side '%c'\n", s); return 1;
  }

  char u;
  if (sscanf(argv[2], "%c", &u) != 1) {
    fprintf(stderr, "Unable to read character from '%s'\n", argv[2]);
    return 2;
  }
  switch (u) {
    case 'U': case 'u': uplo = CBlasUpper; break;
    case 'L': case 'l': uplo = CBlasLower; break;
    default: fprintf(stderr, "Unknown uplo '%c'\n", u); return 2;
  }

  char t;
  if (sscanf(argv[3], "%c", &t) != 1) {
    fprintf(stderr, "Unable to read character from '%s'\n", argv[3]);
    return 3;
  }
  switch (t) {
    case 'N': case 'n': trans = CBlasNoTrans; break;
    case 'T': case 't': trans = CBlasTrans; break;
    case 'C': case 'c': trans = CBlasConjTrans; break;
    default: fprintf(stderr, "Unknown transpose '%c'\n", t); return 3;
  }

  char di;
  if (sscanf(argv[4], "%c", &di) != 1) {
    fprintf(stderr, "Unable to read character from '%s'\n", argv[4]);
    return 4;
  }
  switch (di) {
    case 'N': case 'n': diag = CBlasNonUnit; break;
    case 'U': case 'u': diag = CBlasUnit; break;
    default: fprintf(stderr, "Unknown diag '%c'\n", t); return 4;
  }

  if (sscanf(argv[5], "%zu", &m) != 1) {
    fprintf(stderr, "Unable to parse number from '%s'\n", argv[5]);
    return 5;
  }

  if (sscanf(argv[6], "%zu", &n) != 1) {
    fprintf(stderr, "Unable to parse number from '%s'\n", argv[6]);
    return 6;
  }

  if (argc > 7) {
    if (sscanf(argv[7], "%d", &d) != 1) {
      fprintf(stderr, "Unable to parse number from '%s'\n", argv[7]);
      return 7;
    }
  }

  srand(0);

  float complex alpha, * A, * B, * refB;
  CUdeviceptr dA, dB, dX;
  size_t lda, ldb, dlda, dldb, dldx;

  CU_ERROR_CHECK(cuInit(0));

  CUdevice device;
  CU_ERROR_CHECK(cuDeviceGet(&device, d));

  CUcontext context;
  CU_ERROR_CHECK(cuCtxCreate(&context, CU_CTX_SCHED_BLOCKING_SYNC, device));

  CUBLAShandle handle;
  CU_ERROR_CHECK(cuBLASCreate(&handle));

  alpha = (float)rand() / (float)RAND_MAX + ((float)rand() / (float)RAND_MAX) * I;

  if (side == CBlasLeft) {
    lda = (m + 1u) & ~1u;
    if ((A = malloc(lda * m * sizeof(float complex))) == NULL) {
      fputs("Unable to allocate A\n", stderr);
      return -1;
    }
    CU_ERROR_CHECK(cuMemAllocPitch(&dA, &dlda, m * sizeof(float complex), m, sizeof(float complex)));
    dlda /= sizeof(float complex);

    for (size_t j = 0; j < m; j++) {
      for (size_t i = 0; i < m; i++)
        A[j * lda + i] = (float)rand() / (float)RAND_MAX + ((float)rand() / (float)RAND_MAX) * I;
    }

    CUDA_MEMCPY2D copy = { 0, 0, CU_MEMORYTYPE_HOST, A, 0, NULL, lda * sizeof(float complex),
                           0, 0, CU_MEMORYTYPE_DEVICE, NULL, dA, NULL, dlda * sizeof(float complex),
                           m * sizeof(float complex), m };
    CU_ERROR_CHECK(cuMemcpy2D(&copy));
  }
  else {
    lda = (n + 1u) & ~1u;
    if ((A = malloc(lda * n * sizeof(float complex))) == NULL) {
      fputs("Unable to allocate A\n", stderr);
      return -1;
    }
    CU_ERROR_CHECK(cuMemAllocPitch(&dA, &dlda, n * sizeof(float complex), n, sizeof(float complex)));
    dlda /= sizeof(float complex);

    for (size_t j = 0; j < n; j++) {
      for (size_t i = 0; i < n; i++)
        A[j * lda + i] = (float)rand() / (float)RAND_MAX + ((float)rand() / (float)RAND_MAX) * I;
    }

    CUDA_MEMCPY2D copy = { 0, 0, CU_MEMORYTYPE_HOST, A, 0, NULL, lda * sizeof(float complex),
                           0, 0, CU_MEMORYTYPE_DEVICE, NULL, dA, NULL, dlda * sizeof(float complex),
                           n * sizeof(float complex), n };
    CU_ERROR_CHECK(cuMemcpy2D(&copy));
  }

  ldb = (m + 1u) & ~1u;
  if ((B = malloc(ldb * n * sizeof(float complex))) == NULL) {
    fputs("Unable to allocate B\n", stderr);
    return -3;
  }
  if ((refB = malloc(ldb * n * sizeof(float complex))) == NULL) {
    fputs("Unable to allocate refB\n", stderr);
    return -4;
  }
  CU_ERROR_CHECK(cuMemAllocPitch(&dB, &dldb, m * sizeof(float complex), n, sizeof(float complex)));
  dldb /= sizeof(float complex);
  CU_ERROR_CHECK(cuMemAllocPitch(&dX, &dldx, m * sizeof(float complex), n, sizeof(float complex)));
  dldx /= sizeof(float complex);

  for (size_t j = 0; j < n; j++) {
    for (size_t i = 0; i < m; i++)
      refB[j * ldb + i] = B[j * ldb + i] = (float)rand() / (float)RAND_MAX + ((float)rand() / (float)RAND_MAX) * I;
  }

  CUDA_MEMCPY2D copy = { 0, 0, CU_MEMORYTYPE_HOST, B, 0, NULL, ldb * sizeof(float complex),
                         0, 0, CU_MEMORYTYPE_DEVICE, NULL, dB, NULL, dldb * sizeof(float complex),
                         m * sizeof(float complex), n };
  CU_ERROR_CHECK(cuMemcpy2D(&copy));

  ctrmm_ref(side, uplo, trans, diag, m, n, alpha, A, lda, refB, ldb);
  CU_ERROR_CHECK(cuCtrmm2(handle, side, uplo, trans, diag, m, n, alpha, dA, dlda, dB, dldb, dX, dldx, NULL));

  copy = (CUDA_MEMCPY2D){ 0, 0, CU_MEMORYTYPE_DEVICE, NULL, dX, NULL, dldx * sizeof(float complex),
                          0, 0, CU_MEMORYTYPE_HOST, B, 0, NULL, ldb * sizeof(float complex),
                          m * sizeof(float complex), n };
  CU_ERROR_CHECK(cuMemcpy2D(&copy));

  bool passed = true;
  float rdiff = 0.0f, idiff = 0.0f;
  for (size_t j = 0; j < n; j++) {
    for (size_t i = 0; i < m; i++) {
      float d = fabsf(crealf(B[j * ldb + i]) - crealf(refB[j * ldb + i]));
      if (d > rdiff)
        rdiff = d;

      float c = fabsf(cimagf(B[j * ldb + i]) - cimagf(refB[j * ldb + i]));
      if (c > idiff)
        idiff = c;

      size_t flops;
      if (side == CBlasLeft)
        flops = 2 * i + 1;
      else
        flops = 2 * j + 1;
      if (diag == CBlasNonUnit)
        flops++;
      flops *= 3;

      if (d > (float)flops * 2.0f * FLT_EPSILON ||
          c > (float)flops * 2.0f * FLT_EPSILON)
        passed = false;
    }
  }

  CUevent start, stop;
  CU_ERROR_CHECK(cuEventCreate(&start, CU_EVENT_BLOCKING_SYNC));
  CU_ERROR_CHECK(cuEventCreate(&stop, CU_EVENT_BLOCKING_SYNC));

  CU_ERROR_CHECK(cuEventRecord(start, NULL));
  for (size_t i = 0; i < 20; i++)
    CU_ERROR_CHECK(cuCtrmm2(handle, side, uplo, trans, diag, m, n, alpha, dA, dlda, dB, dldb, dX, dldx, NULL));
  CU_ERROR_CHECK(cuEventRecord(stop, NULL));
  CU_ERROR_CHECK(cuEventSynchronize(stop));

  float time;
  CU_ERROR_CHECK(cuEventElapsedTime(&time, start, stop));
  time /= 20;

  CU_ERROR_CHECK(cuEventDestroy(start));
  CU_ERROR_CHECK(cuEventDestroy(stop));

  const size_t flops = (side == CBlasLeft) ?
                        (6 * (n * m * (m + 1) / 2) + 2 * (n * m * (m - 1) / 2)) :
                        (6 * (m * n * (n + 1) / 2) + 2 * (m * n * (n - 1) / 2));

  fprintf(stdout, "%.3es %.3gGFlops/s Error: %.3e + %.3ei\n%sED!\n", time * 1.e-3f,
          ((float)flops * 1.e-6f) / time, rdiff, idiff, (passed) ? "PASS" : "FAIL");

  free(A);
  free(B);
  free(refB);
  CU_ERROR_CHECK(cuMemFree(dA));
  CU_ERROR_CHECK(cuMemFree(dB));
  CU_ERROR_CHECK(cuMemFree(dX));

  CU_ERROR_CHECK(cuBLASDestroy(handle));

  CU_ERROR_CHECK(cuCtxDestroy(context));

  return (int)!passed;
}
