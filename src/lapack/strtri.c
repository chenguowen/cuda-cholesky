#include "lapack.h"

static size_t min(size_t a, size_t b) { return (a < b) ? a : b; }

static const float zero = 0.0f;
static const float one = 1.0f;

static inline void strti2(CBlasUplo uplo, CBlasDiag diag,
                          size_t n,
                          const float * restrict A, size_t lda,
                          float * restrict B, size_t ldb,
                          long * restrict info) {
  if (uplo == CBlasUpper) {
    for (size_t j = 0; j < n; j++) {
      register float bjj;
      if (diag == CBlasNonUnit) {
        if (A[j * lda + j] == zero) {
          *info = (long)j + 1;
          return;
        }
        B[j * ldb + j] = one / A[j * lda + j];
        bjj = -B[j * ldb + j];
      }
      else
        bjj = -one;

      for (size_t i = 0; i < j; i++) {
        B[j * ldb + i] = A[j * lda + i];
        if (A[j * lda + i] != zero) {
          register float temp = A[j * lda + i];
          for (size_t k = 0; k < i; k++)
            B[j * ldb + k] += temp * A[i * lda + k];
          if (diag == CBlasNonUnit) B[j * lda + i] *= A[i * lda + i];
        }
      }
      for (size_t i = 0; i < j; i++)
        B[j * lda + i] *= bjj;
    }
  }
  else {
    size_t j = n - 1;
    do {
      register float bjj;
      if (diag == CBlasNonUnit) {
        if (A[j * lda + j] == zero) {
          *info = (long)j + 1;
          return;
        }
        B[j * ldb + j] = one / A[j * lda + j];
        bjj = -B[j * ldb + j];
      }
      else
        bjj = -one;

      if (j < n - 1) {
        size_t i = n - 1;
        do {
          B[j * ldb + i] = A[j * lda + i];
          if (A[j * lda + i] != zero) {
            register float temp = A[j * lda + i];
            for (size_t k = i + 1; k < n; k++)
              B[j * ldb + k] += temp * A[i * lda + k];
            if (diag == CBlasNonUnit) B[j * ldb + i] *= A[i * lda + i];
          }
        } while (i-- > j + 1);
        for (size_t i = j + 1; i < n; i++)
          B[j * ldb + i] *= bjj;
      }
    } while (j-- > 0);
  }
}

void strtri2(CBlasUplo uplo, CBlasDiag diag,
             size_t n,
             const float * restrict A, size_t lda,
             float * restrict B, size_t ldb,
             long * restrict info) {
  *info = 0;
  if (lda < n)
    *info = -5;
  if (ldb < n)
    *info = -7;
  if (*info != 0) {
    XERBLA(-(*info));
    return;
  }

  if (n == 0)
    return;

  const size_t nb = 64;

  if (n < nb) {
    strti2(uplo, diag, n, A, lda, B, ldb, info);
    return;
  }

  if (uplo == CBlasUpper) {
    for (size_t j = 0; j < n; j += nb) {
      const size_t jb = min(nb, n - j);
      strmm2(CBlasLeft, CBlasUpper, CBlasNoTrans, diag, j, jb, one, B, ldb, &A[j * lda], lda, &B[j * ldb], lda);
      strsm(CBlasRight, CBlasUpper, CBlasNoTrans, diag, j, jb, -one, &A[j * lda + j], lda, &B[j * ldb], ldb);
      strti2(CBlasUpper, diag, jb, &A[j * lda + j], lda, &B[j * ldb + j], ldb, info);
      if (*info != 0) {
        *info += (long)j;
        return;
      }
    }
  }
  else {
    size_t j = (n + nb - 1) & ~(nb - 1);
    do {
      j -= nb;
      const size_t jb = min(nb, n - j);
      if (j + jb < n) {
        strmm2(CBlasLeft, CBlasLower, CBlasNoTrans, diag, n - j - jb, jb, one, &B[(j + jb) * ldb + j + jb], ldb, &A[j * lda + j + jb], lda, &B[j * ldb + j + jb], ldb);
        strsm(CBlasRight, CBlasLower, CBlasNoTrans, diag, n - j - jb, jb, -one, &A[j * lda + j], lda, &B[j * ldb + j + jb], ldb);
      }
      strti2(CBlasLower, diag, jb, &A[j * lda + j], lda, &B[j * ldb + j], ldb, info);
      if (*info != 0) {
        *info += (long)j;
        return;
      }
    } while (j > 0);
  }
}
