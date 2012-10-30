#include "lapack.h"

static size_t min(size_t a, size_t b) { return (a < b) ? a : b; }

static const float zero = 0.0f;
static const float one = 1.0f;

static inline void strti2(CBlasUplo uplo, CBlasDiag diag, size_t n, float * restrict A, size_t lda, long * restrict info) {
  if (uplo == CBlasUpper) {
    for (size_t j = 0; j < n; j++) {
      register float ajj;
      if (diag == CBlasNonUnit) {
        if (A[j * lda + j] == zero) {
          *info = (long)j + 1;
          return;
        }
        A[j * lda + j] = one / A[j * lda + j];
        ajj = -A[j * lda + j];
      }
      else
        ajj = -one;

      for (size_t i = 0; i < j; i++) {
        if (A[j * lda + i] != zero) {
          register float temp = A[j * lda + i];
          for (size_t k = 0; k < i; k++)
            A[j * lda + k] += temp * A[i * lda + k];
          if (diag == CBlasNonUnit) A[j * lda + i] *= A[i * lda + i];
        }
      }
      for (size_t i = 0; i < j; i++)
        A[j * lda + i] *= ajj;
    }
  }
  else {
    size_t j = n - 1;
    do {
      register float ajj;
      if (diag == CBlasNonUnit) {
        if (A[j * lda + j] == zero) {
          *info = (long)j + 1;
          return;
        }
        A[j * lda + j] = one / A[j * lda + j];
        ajj = -A[j * lda + j];
      }
      else
        ajj = -one;

      if (j < n - 1) {
        size_t i = n - 1;
        do {
          if (A[j * lda + i] != zero) {
            register float temp = A[j * lda + i];
            for (size_t k = i + 1; k < n; k++)
              A[j * lda + k] += temp * A[i * lda + k];
            if (diag == CBlasNonUnit) A[j * lda + i] *= A[i * lda + i];
          }
        } while (i-- > j + 1);
        for (size_t i = j + 1; i < n; i++)
          A[j * lda + i] *= ajj;
      }
    } while (j-- > 0);
  }
}

void strtri(CBlasUplo uplo, CBlasDiag diag, size_t n, float * restrict A, size_t lda, long * restrict info) {
  *info = 0;
  if (lda < n)
    *info = -5;
  if (*info != 0) {
    XERBLA(-(*info));
    return;
  }

  if (n == 0)
    return;

  const size_t nb = 64;

  if (n < nb) {
    strti2(uplo, diag, n, A, lda, info);
    return;
  }

  if (uplo == CBlasUpper) {
    for (size_t j = 0; j < n; j += nb) {
      const size_t jb = min(nb, n - j);
      strmm(CBlasLeft, CBlasUpper, CBlasNoTrans, diag, j, jb, one, A, lda, &A[j * lda], lda);
      strsm(CBlasRight, CBlasUpper, CBlasNoTrans, diag, j, jb, -one, &A[j * lda + j], lda, &A[j * lda], lda);
      strti2(CBlasUpper, diag, jb, &A[j * lda + j], lda, info);
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
        strmm(CBlasLeft, CBlasLower, CBlasNoTrans, diag, n - j - jb, jb, one, &A[(j + jb) * lda + j + jb], lda, &A[j * lda + j + jb], lda);
        strsm(CBlasRight, CBlasLower, CBlasNoTrans, diag, n - j - jb, jb, -one, &A[j * lda + j], lda, &A[j * lda + j + jb], lda);
      }
      strti2(CBlasLower, diag, jb, &A[j * lda + j], lda, info);
      if (*info != 0) {
        *info += (long)j;
        return;
      }
    } while (j > 0);
  }
}
