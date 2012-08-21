#include "blas.h"
#include "error.h"
#include <stdio.h>
#include <sys/time.h>
#include <float.h>

static void ssyrk_ref(CBlasUplo uplo, CBlasTranspose trans, size_t n, size_t k,
                      float alpha, const float * restrict A, size_t lda,
                      float beta, float * restrict C, size_t ldc) {

  if (n == 0 || ((k == 0 || alpha == 0.0f) && beta == 1.0f)) return;

  if (alpha == 0.0f) {
    if (uplo == CBlasUpper) {
      if (beta == 0.0f) {
        for (size_t j = 0; j < n; j++) {
          for (size_t i = 0; i <= j; i++)
            C[j * ldc + i] = 0.0f;
        }
      }
      else {
        for (size_t j = 0; j < n; j++) {
          for (size_t i = 0; i <= j; i++)
            C[j * ldc + i] = beta * C[j * ldc + i];
        }
      }
    }
    else {
      if (beta == 0.0f) {
        for (size_t j = 0; j < n; j++) {
          for (size_t i = j; i < n; i++)
            C[j * ldc + i] = 0.0f;
        }
      }
      else {
        for (size_t j = 0; j < n; j++) {
          for (size_t i = j; i < n; i++)
            C[j * ldc + i] = beta * C[j * ldc + i];
        }
      }
    }
    return;
  }

  for (size_t j = 0; j < n; j++) {
    if (uplo == CBlasUpper) {
      for (size_t i = 0; i <= j; i++) {
        float temp;

        if (trans == CBlasNoTrans) {
          temp = A[i] * A[j];
          for (size_t l = 1; l < k; l++)
            temp += A[l * lda + i] * A[l * lda + j];
        }
        else {
          temp = A[i * lda] * A[j * lda];
          for (size_t l = 1; l < k; l++)
            temp += A[i * lda + l] * A[j * lda + l];
        }

        if (alpha != 1.0f)
          temp *= alpha;
        if (beta != 0.0f)
          temp += beta * C[j * ldc + i];

        C[j * ldc + i] = temp;
      }
    }
    else {
      for (size_t i = j; i < n; i++) {
        float temp;

        if (trans == CBlasNoTrans) {
          temp = A[i] * A[j];
          for (size_t l = 1; l < k; l++)
            temp += A[l * lda + i] * A[l * lda + j];
        }
        else {
          temp = A[i * lda] * A[j * lda];
          for (size_t l = 1; l < k; l++)
            temp += A[i * lda + l] * A[j * lda + l];
        }

        if (alpha != 1.0f)
          temp *= alpha;
        if (beta != 0.0f)
          temp += beta * C[j * ldc + i];

        C[j * ldc + i] = temp;
      }
    }
  }
}

int main(int argc, char * argv[]) {
  CBlasUplo uplo;
  CBlasTranspose trans;
  size_t n, k;

  if (argc != 5) {
    fprintf(stderr, "Usage: %s <uplo> <trans> <n> <k>\nwhere:\n  uplo               is 'u' or 'U' for CBlasUpper or 'l' or 'L' for CBlasLower\n  transA and transB  are 'n' or 'N' for CBlasNoTrans, 't' or 'T' for CBlasTrans or 'c' or 'C' for CBlasConjTrans\n  n and k            are the sizes of the matrices\n", argv[0]);
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

  srand(0);

  float alpha, beta, * A, * C, * refC;
  size_t lda, ldc;

  alpha = (float)rand() / (float)RAND_MAX;
  beta = (float)rand() / (float)RAND_MAX;

  if (trans == CBlasNoTrans) {
    lda = (n + 3u) & ~3u;
    if ((A = malloc(lda * k * sizeof(float))) == NULL) {
      fputs("Unable to allocate A\n", stderr);
      return -1;
    }

    for (size_t j = 0; j < k; j++) {
      for (size_t i = 0; i < n; i++)
        A[j * lda + i] = (float)rand() / (float)RAND_MAX;
    }
  }
  else {
    lda = (k + 3u) & ~3u;
    if ((A = malloc(lda * n * sizeof(float))) == NULL) {
      fputs("Unable to allocate A\n", stderr);
      return -1;
    }

    for (size_t j = 0; j < n; j++) {
      for (size_t i = 0; i < k; i++)
        A[j * lda + i] = (float)rand() / (float)RAND_MAX;
    }
  }

  ldc = (n + 3u) & ~3u;
  if ((C = malloc(ldc * n * sizeof(float))) == NULL) {
    fputs("Unable to allocate C\n", stderr);
    return -3;
  }
  if ((refC = malloc(ldc * n * sizeof(float))) == NULL) {
    fputs("Unable to allocate refC\n", stderr);
    return -4;
  }

  for (size_t j = 0; j < n; j++) {
    for (size_t i = 0; i < n; i++)
      refC[j * ldc + i] = C[j * ldc + i] = (float)rand() / (float)RAND_MAX;
  }

  ssyrk_ref(uplo, trans, n, k, alpha, A, lda, beta, refC, ldc);
  ssyrk(uplo, trans, n, k, alpha, A, lda, beta, C, ldc);

  float diff = 0.0f;
  for (size_t j = 0; j < n; j++) {
    for (size_t i = 0; i < n; i++) {
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
    ssyrk(uplo, trans, n, k, alpha, A, lda, beta, C, ldc);
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
  flops *= n * (n + 1) / 2;

  bool passed = (diff <= error);
  fprintf(stdout, "%.3es %.3gGFlops/s Error: %.3e\n%sED!\n", time,
          ((double)flops * 1.e-9) / time, diff, (passed) ? "PASS" : "FAIL");

  free(A);
  free(C);
  free(refC);

  return (int)!passed;
}