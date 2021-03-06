#include "blas.h"
#include "error.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <float.h>
#include <math.h>
#include <complex.h>
#include "ref/cherk_ref.c"

int main(int argc, char * argv[]) {
  CBlasUplo uplo;
  CBlasTranspose trans;
  size_t n, k;
  int d = 0;

  if (argc < 5 || argc > 6) {
    fprintf(stderr, "Usage: %s <uplo> <trans> <n> <k> [device]\n"
                    "where:\n"
                    "  uplo     is 'u' or 'U' for CBlasUpper or 'l' or 'L' for CBlasLower\n"
                    "  trans    are 'n' or 'N' for CBlasNoTrans or 'c' or 'C' for CBlasConjTrans\n"
                    "  n and k  are the sizes of the matrices\n"
                    "  device   is the GPU to use (default 0)\n", argv[0]);
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

  char t;
  if (sscanf(argv[2], "%c", &t) != 1) {
    fprintf(stderr, "Unable to read character from '%s'\n", argv[2]);
    return 2;
  }
  switch (t) {
    case 'N': case 'n': trans = CBlasNoTrans; break;
    case 'T': case 't': trans = CBlasTrans; break;
    case 'C': case 'c': trans = CBlasConjTrans; break;
    default: fprintf(stderr, "Unknown transpose '%c'\n", t); return 2;
  }

  if (sscanf(argv[3], "%zu", &n) != 1) {
    fprintf(stderr, "Unable to parse number from '%s'\n", argv[3]);
    return 3;
  }

  if (sscanf(argv[4], "%zu", &k) != 1) {
    fprintf(stderr, "Unable to parse number from '%s'\n", argv[4]);
    return 4;
  }

  if (argc > 5) {
    if (sscanf(argv[5], "%d", &d) != 1) {
      fprintf(stderr, "Unable to parse number from '%s'\n", argv[5]);
      return 5;
    }
  }

  srand(0);

  float alpha, beta;
  float complex * A, * C, * refC;
  CUdeviceptr dA, dC;
  size_t lda, ldc, dlda, dldc;

  CU_ERROR_CHECK(cuInit(0));

  CUdevice device;
  CU_ERROR_CHECK(cuDeviceGet(&device, d));

  CUcontext context;
  CU_ERROR_CHECK(cuCtxCreate(&context, CU_CTX_SCHED_BLOCKING_SYNC, device));

  CUBLAShandle handle;
  CU_ERROR_CHECK(cuBLASCreate(&handle));

  alpha = (float)rand() / (float)RAND_MAX;
  beta = (float)rand() / (float)RAND_MAX;

  if (trans == CBlasNoTrans) {
    lda = (n + 1u) & ~1u;
    if ((A = malloc(lda * k * sizeof(float complex))) == NULL) {
      fputs("Unable to allocate A\n", stderr);
      return -1;
    }
    CU_ERROR_CHECK(cuMemAllocPitch(&dA, &dlda, n * sizeof(float complex), k, sizeof(float complex)));
    dlda /= sizeof(float complex);

    for (size_t j = 0; j < k; j++) {
      for (size_t i = 0; i < n; i++)
        A[j * lda + i] = ((float)rand() / (float)RAND_MAX) + ((float)rand() / (float)RAND_MAX) * I;
    }

    CUDA_MEMCPY2D copy = { 0, 0, CU_MEMORYTYPE_HOST, A, 0, NULL, lda * sizeof(float complex),
                           0, 0, CU_MEMORYTYPE_DEVICE, NULL, dA, NULL, dlda * sizeof(float complex),
                           n * sizeof(float complex), k };
    CU_ERROR_CHECK(cuMemcpy2D(&copy));
  }
  else {
    lda = (k + 1u) & ~1u;
    if ((A = malloc(lda * n * sizeof(float complex))) == NULL) {
      fputs("Unable to allocate A\n", stderr);
      return -1;
    }
    CU_ERROR_CHECK(cuMemAllocPitch(&dA, &dlda, k * sizeof(float complex), n, sizeof(float complex)));
    dlda /= sizeof(float complex);

    for (size_t j = 0; j < n; j++) {
      for (size_t i = 0; i < k; i++)
        A[j * lda + i] = ((float)rand() / (float)RAND_MAX) + ((float)rand() / (float)RAND_MAX) * I;
    }

    CUDA_MEMCPY2D copy = { 0, 0, CU_MEMORYTYPE_HOST, A, 0, NULL, lda * sizeof(float complex),
                           0, 0, CU_MEMORYTYPE_DEVICE, NULL, dA, NULL, dlda * sizeof(float complex),
                           k * sizeof(float complex), n };
    CU_ERROR_CHECK(cuMemcpy2D(&copy));
  }

  ldc = (n + 1u) & ~1u;
  if ((C = malloc(ldc * n * sizeof(float complex))) == NULL) {
    fputs("Unable to allocate C\n", stderr);
    return -3;
  }
  if ((refC = malloc(ldc * n * sizeof(float complex))) == NULL) {
    fputs("Unable to allocate refC\n", stderr);
    return -4;
  }
  CU_ERROR_CHECK(cuMemAllocPitch(&dC, &dldc, n * sizeof(float complex), n, sizeof(float complex)));
  dldc /= sizeof(float complex);

  for (size_t j = 0; j < n; j++) {
    for (size_t i = 0; i < n; i++)
      refC[j * ldc + i] = C[j * ldc + i] = ((float)rand() / (float)RAND_MAX) + ((float)rand() / (float)RAND_MAX) * I;
  }

  CUDA_MEMCPY2D copy = { 0, 0, CU_MEMORYTYPE_HOST, C, 0, NULL, ldc * sizeof(float complex),
                         0, 0, CU_MEMORYTYPE_DEVICE, NULL, dC, NULL, dldc * sizeof(float complex),
                         n * sizeof(float complex), n };
  CU_ERROR_CHECK(cuMemcpy2D(&copy));

  cherk_ref(uplo, trans, n, k, alpha, A, lda, beta, refC, ldc);
  CU_ERROR_CHECK(cuCherk(handle, uplo, trans, n, k, alpha, dA, dlda, beta, dC, dldc, NULL));

  copy = (CUDA_MEMCPY2D){ 0, 0, CU_MEMORYTYPE_DEVICE, NULL, dC, NULL, dldc * sizeof(float complex),
           0, 0, CU_MEMORYTYPE_HOST, C, 0, NULL, ldc * sizeof(float complex),
           n * sizeof(float complex), n };
  CU_ERROR_CHECK(cuMemcpy2D(&copy));

  float rdiff = 0.0f, idiff = 0.0f;
  for (size_t j = 0; j < n; j++) {
    for (size_t i = 0; i < n; i++) {
      float d = fabsf(crealf(C[j * ldc + i]) - crealf(refC[j * ldc + i]));
      if (d > rdiff)
        rdiff = d;
      d = fabsf(cimagf(C[j * ldc + i]) - cimagf(refC[j * ldc + i]));
      if (d > idiff)
        idiff = d;
    }
  }

  CUevent start, stop;
  CU_ERROR_CHECK(cuEventCreate(&start, CU_EVENT_BLOCKING_SYNC));
  CU_ERROR_CHECK(cuEventCreate(&stop, CU_EVENT_BLOCKING_SYNC));

  CU_ERROR_CHECK(cuEventRecord(start, NULL));
  for (size_t i = 0; i < 20; i++)
    CU_ERROR_CHECK(cuCherk(handle, uplo, trans, n, k, alpha, dA, dlda, beta, dC, dldc, NULL));
  CU_ERROR_CHECK(cuEventRecord(stop, NULL));
  CU_ERROR_CHECK(cuEventSynchronize(stop));

  float time;
  CU_ERROR_CHECK(cuEventElapsedTime(&time, start, stop));
  time /= 20;

  CU_ERROR_CHECK(cuEventDestroy(start));
  CU_ERROR_CHECK(cuEventDestroy(stop));

  size_t flops = k * 6 + (k - 1) * 2;   // k multiplies and k - 1 adds per element
  if (alpha != 1.0f)
    flops += 1;                 // additional multiply by alpha
  if (beta != 0.0f)
    flops += 2;                 // additional multiply and add by beta
  float error = (float)flops * 2.0f * FLT_EPSILON;     // maximum per element error
  flops *= n * (n + 1) / 2;     // n(n + 1) / 2 elements

  bool passed = (rdiff <= error) && (idiff <= error);
  fprintf(stdout, "%.3ems %.3gGFlops/s Error: %.3e + %.3ei\n%sED!\n", time,
          ((float)flops * 1.e-6f) / time, rdiff, idiff, (passed) ? "PASS" : "FAIL");

  free(A);
  free(C);
  free(refC);
  CU_ERROR_CHECK(cuMemFree(dA));
  CU_ERROR_CHECK(cuMemFree(dC));

  CU_ERROR_CHECK(cuBLASDestroy(handle));

  CU_ERROR_CHECK(cuCtxDestroy(context));

  return (int)!passed;
}
