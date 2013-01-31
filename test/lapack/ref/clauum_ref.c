static void clauum_ref(CBlasUplo uplo, size_t n, float complex * restrict A, size_t lda, long * restrict info) {
  *info = 0;
  if (lda < n)
    *info = -4;
  if (*info != 0) {
    XERBLA(-(*info));
    return;
  }

  if (n == 0) return;

  if (uplo == CBlasUpper) {
    for (size_t j = 0; j < n; j++) {
      float complex ajj = conjf(A[j * lda + j]);
      for (size_t i = 0; i <= j; i++) {
        A[j * lda + i] *= ajj;
        for (size_t k = j + 1; k < n; k++)
          A[j * lda + i] += A[k * lda + i] * conjf(A[k * lda + j]);
      }
    }
  }
  else {
    for (size_t i = 0; i < n; i++) {
      float complex aii = conjf(A[i * lda + i]);
      for (size_t j = 0; j <= i; j++) {
        A[j * lda + i] *= aii;
        for (size_t k = i + 1; k < n; k++)
          A[j * lda + i] += conjf(A[i * lda + k]) * A[j * lda + k];
      }
    }
  }
}

static float complex gaussian() {
  float u0 = ((float)rand() + 1.0f) / (float)RAND_MAX;
  float u1 = ((float)rand() + 1.0f) / (float)RAND_MAX;
  float r = sqrtf(-2.0f * logf(u0));
  float phi = 2.0f * 3.1415926535f * u1;
  float real = r * sinf(phi);
  float imag = r * cosf(phi);
  return real + imag * I;
}
