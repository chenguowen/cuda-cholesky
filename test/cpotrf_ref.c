static void cpotrf_ref(CBlasUplo uplo, size_t n, float complex * restrict A, size_t lda, long * restrict info) {
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
      for (size_t i = 0; i < j; i++) {
        float complex temp = A[j * lda + i];
        for (size_t k = 0; k < i; k++)
          temp -= A[j * lda + k] * conjf(A[i * lda + k]);
        A[j * lda + i] = temp / crealf(A[i * lda + i]);
      }

      float ajj = crealf(A[j * lda + j]);
      for (size_t k = 0; k < j; k++)
        ajj -= A[j * lda + k] * conjf(A[j * lda + k]);
      if (ajj <= 0.0f || isnan(ajj)) {
        A[j * lda + j] = ajj;
        *info = (long)j + 1;
        return;
      }
      A[j * lda + j] = sqrtf(ajj);
    }
  }
  else {
    for (size_t j = 0; j < n; j++) {
      float ajj = crealf(A[j * lda + j]);
      for (size_t k = 0; k < j; k++)
        ajj -= A[k * lda + j] * conjf(A[k * lda + j]);
      if (ajj <= 0.0f || isnan(ajj)) {
        A[j * lda + j] = ajj;
        *info = (long)j + 1;
        return;
      }
      ajj = sqrtf(ajj);
      A[j * lda + j] = ajj;

      for (size_t i = j + 1; i < n; i++) {
        float complex temp = A[j * lda + i];
        for (size_t k = 0; k < j; k++)
          temp -= conjf(A[k * lda + j]) * A[k * lda + i];
        A[j * lda + i] = temp / ajj;
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
