#include "blas.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <float.h>
#include <math.h>
#include <sys/time.h>
#include "strsm_ref.c"

int main(int argc, char * argv[]) {
  CBlasSide side;
  CBlasUplo uplo;
  CBlasTranspose trans;
  CBlasDiag diag;
  size_t m, n;

  if (argc != 7) {
    fprintf(stderr, "Usage: %s <side> <uplo> <trans> <diag> <m> <n>\n"
                    "where:\n"
                    "  side               is 'l' or 'L' for CBlasLeft and 'r' or 'R' for CBlasRight\n"
                    "  uplo               is 'u' or 'U' for CBlasUpper and 'l' or 'L' for CBlasLower\n"
                    "  trans              is 'n' or 'N' for CBlasNoTrans, 't' or 'T' for CBlasTrans or 'c' or 'C' for CBlasConjTrans\n"
                    "  diag               is 'n' or 'N' for CBlasNonUnit and 'u' or 'U' for CBlasUnit\n"
                    "  m and n           are the sizes of the matrices\n", argv[0]);
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

  char d;
  if (sscanf(argv[4], "%c", &d) != 1) {
    fprintf(stderr, "Unable to read character from '%s'\n", argv[4]);
    return 4;
  }
  switch (d) {
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

  srand(0);

  float alpha, * A, * B, * refB, * C;
  size_t lda, ldb, ldc, * F;

  alpha = (float)rand() / (float)RAND_MAX;

  if (side == CBlasLeft) {
    lda = (m + 3u) & ~3u;
    if ((A = malloc(lda * m * sizeof(float))) == NULL) {
      fputs("Unable to allocate A\n", stderr);
      return -1;
    }

    size_t k = m * 5;
    ldc = (k + 3u) & ~3u;
    if ((C = malloc(ldc * m * sizeof(float))) == NULL) {
      fputs("Unable to allocate C\n", stderr);
      return -1;
    }
    for (size_t j = 0; j < m; j++) {
      for (size_t i = 0; i < k; i++)
        C[j * ldc + i] = (float)rand() / (float)RAND_MAX;
    }
    for (size_t j = 0; j < m; j++) {
      for (size_t i = 0; i < m; i++) {
        float temp = 0.0f;
        for (size_t l = 0; l < k; l++)
          temp += C[i * ldc + l] * C[j * ldc + l];
        A[j * lda + i] = temp;
      }
    }
    free(C);
  }
  else {
    lda = (n + 3u) & ~3u;
    if ((A = malloc(lda * n * sizeof(float))) == NULL) {
      fputs("Unable to allocate A\n", stderr);
      return -1;
    }

    size_t k = n * 5;
    ldc = (k + 3u) & ~3u;
    if ((C = malloc(ldc * n * sizeof(float))) == NULL) {
      fputs("Unable to allocate C\n", stderr);
      return -1;
    }
    for (size_t j = 0; j < n; j++) {
      for (size_t i = 0; i < k; i++)
        C[j * ldc + i] = (float)rand() / (float)RAND_MAX;
    }
    for (size_t j = 0; j < n; j++) {
      for (size_t i = 0; i < n; i++) {
        float temp = 0.0f;
        for (size_t l = 0; l < k; l++)
          temp += C[i * ldc + l] * C[j * ldc + l];
        A[j * lda + i] = temp;
      }
    }
    free(C);
  }

  ldb = (m + 3u) & ~3u;
  if ((B = malloc(ldb * n * sizeof(float))) == NULL) {
    fputs("Unable to allocate B\n", stderr);
    return -3;
  }
  if ((refB = malloc(ldb * n * sizeof(float))) == NULL) {
    fputs("Unable to allocate refB\n", stderr);
    return -4;
  }
  if ((F = calloc(ldb * n, sizeof(size_t))) == NULL) {
    fputs("Unable to allocate F\n", stderr);
    return -5;
  }

  for (size_t j = 0; j < n; j++) {
    for (size_t i = 0; i < m; i++)
      refB[j * ldb + i] = B[j * ldb + i] = (float)rand() / (float)RAND_MAX;
  }

  strsm_ref(side, uplo, trans, diag, m, n, alpha, A, lda, refB, ldb, F);
  strsm(side, uplo, trans, diag, m, n, alpha, A, lda, B, ldb);

  bool passed = true;
  float diff = 0.0f;
  for (size_t j = 0; j < n; j++) {
    for (size_t i = 0; i < m; i++) {
      float d = fabsf(B[j * ldb + i] - refB[j * ldb + i]);
      if (d > diff)
        diff = d;

      if (passed) {
        if (d > (float)F[j * ldb + i] * 2.0f * FLT_EPSILON)
          passed = false;
      }
    }
  }
  free(F);

  struct timeval start, stop;
  if (gettimeofday(&start, NULL) != 0) {
    fputs("gettimeofday failed\n", stderr);
    return -6;
  }
  for (size_t i = 0; i < 20; i++)
    strsm(side, uplo, trans, diag, m, n, alpha, A, lda, B, ldb);
  if (gettimeofday(&stop, NULL) != 0) {
    fputs("gettimeofday failed\n", stderr);
    return -7;
  }

  double time = ((double)(stop.tv_sec - start.tv_sec) +
                 (double)(stop.tv_usec - start.tv_usec) * 1.e-6) / 20.0;

  size_t flops = m * n;
  if (alpha != 0.0f) {
    flops += (side == CBlasLeft) ? m * m * n : m * n * n;
    if (diag == CBlasNonUnit) flops += m * n;
  }

  fprintf(stdout, "%.3es %.3gGFlops/s Error: %.3e\n%sED!\n", time,
          ((double)flops * 1.e-9) / time, diff, (passed) ? "PASS" : "FAIL");

  free(A);
  free(B);
  free(refB);

  return (int)!passed;
}
