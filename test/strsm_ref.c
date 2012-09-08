static void strsm_ref(CBlasSide side, CBlasUplo uplo, CBlasTranspose trans,
                      CBlasDiag diag, size_t m, size_t n,
                      float alpha, const float * restrict A, size_t lda,
                      float * restrict B, size_t ldb, size_t * E) {

  if (m == 0 || n == 0) return;

  if (alpha == 0.0f) {
    for (size_t j = 0; j < n; j++) {
      for (size_t i = 0; i < m; i++)
        B[j * ldb + i] = 0.0f;
    }
    return;
  }

  if (side == CBlasLeft) {
    if (trans == CBlasNoTrans) {
      if (uplo == CBlasUpper) {
        for (size_t j = 0; j < n; j++) {
          size_t i = m - 1;
          do {
            float temp = alpha * B[j * ldb + i]; E[j * ldb + i] = 1;
            for (size_t k = i + 1; k < m; k++) {
              temp -= A[k * lda + i] * B[j * ldb + k]; E[j * ldb + i] += E[j * ldb + k] + 2; }
            if (diag == CBlasNonUnit) { temp /= A[i * lda + i]; E[j * ldb + i]++; }
            B[j * ldb + i] = temp;
          } while (i-- > 0);
        }
      }
      else {
        for (size_t j = 0; j < n; j++) {
          for (size_t i = 0; i < m; i++) {
            float temp = alpha * B[j * ldb + i]; E[j * ldb + i] = 1;
            for (size_t k = 0; k < i; k++) {
              temp -= A[k * lda + i] * B[j * ldb + k]; E[j * ldb + i] += E[j * ldb + k] + 2; }
            if (diag == CBlasNonUnit) { temp /= A[i * lda + i]; E[j * ldb + i]++; }
            B[j * ldb + i] = temp;
          }
        }
      }
    }
    else {
      if (uplo == CBlasUpper) {
        for (size_t j = 0; j < n; j++) {
          for (size_t i = 0; i < m; i++) {
            float temp = alpha * B[j * ldb + i]; E[j * ldb + i] = 1;
            for (size_t k = 0; k < i; k++) {
              temp -= A[i * lda + k] * B[j * ldb + k]; E[j * ldb + i] += E[j * ldb + k] + 2; }
            if (diag == CBlasNonUnit) { temp /= A[i * lda + i]; E[j * ldb + i]++; }
            B[j * ldb + i] = temp;
          }
        }
      }
      else {
        for (size_t j = 0; j < n; j++) {
          size_t i = m - 1;
          do {
            float temp = alpha * B[j * ldb + i]; E[j * ldb + i] = 1;
            for (size_t k = i + 1; k < m; k++) {
              temp -= A[i * lda + k] * B[j * ldb + k]; E[j * ldb + i] += E[j * ldb + k] + 2; }
            if (diag == CBlasNonUnit) { temp /= A[i * lda + i]; E[j * ldb + i]++; }
            B[j * ldb + i] = temp;
          } while (i-- > 0);
        }
      }
    }
  }
  else {
    if (trans == CBlasNoTrans) {
      if (uplo == CBlasUpper) {
        for (size_t j = 0; j < n; j++) {
          for (size_t i = 0; i < m; i++) {
            float temp = alpha * B[j * ldb + i]; E[j * ldb + i] = 1;
            for (size_t k = 0; k < j; k++) {
              temp -= A[j * lda + k] * B[k * ldb + i]; E[j * ldb + i] += E[k * ldb + i] + 2; }
            if (diag == CBlasNonUnit) { temp /= A[j * lda + j]; E[j * ldb + i]++; }
            B[j * ldb + i] = temp;
          }
        }
      }
      else {
        size_t j = n - 1;
        do {
          for (size_t i = 0; i < m; i++) {
            float temp = alpha * B[j * ldb + i]; E[j * ldb + i] = 1;
            for (size_t k = j + 1; k < n; k++) {
              temp -= A[j * lda + k] * B[k * ldb + i]; E[j * ldb + i] += E[k * ldb + i] + 2; }
            if (diag == CBlasNonUnit) { temp /= A[j * lda + j]; E[j * ldb + i]++; }
            B[j * ldb + i] = temp;
          }
        } while (j-- > 0);
      }
    }
    else {//
      if (uplo == CBlasUpper) {
        size_t j = n - 1;
        do {
          for (size_t i = 0; i < m; i++) {
            float temp = alpha * B[j * ldb + i]; E[j * ldb + i] = 1;
            for (size_t k = j + 1; k < n; k++) {
              temp -= A[k * lda + j] * B[k * ldb + i]; E[j * ldb + i] += E[k * ldb + i] + 2; }
            if (diag == CBlasNonUnit) { temp /= A[j * lda + j]; E[j * ldb + i]++; }
            B[j * ldb + i] = temp;
          }
        } while (j-- > 0);
      }
      else {
        for (size_t j = 0; j < n; j++) {
          for (size_t i = 0; i < m; i++) {
            float temp = alpha * B[j * ldb + i]; E[j * ldb + i] = 1;
            for (size_t k = 0; k < j; k++) {
              temp -= A[k * lda + j] * B[k * ldb + i]; E[j * ldb + i] += E[k * ldb + i] + 2; }
            if (diag == CBlasNonUnit) { temp /= A[j * lda + j]; E[j * ldb + i]++; }
            B[j * ldb + i] = temp;
          }
        }
      }
    }
  }
}

static float gaussian() {
  static bool hasNext = false;
  static float next;

  if (hasNext) {
    hasNext = false;
    return next;
  }

  float u0 = ((float)rand() + 1) / (float)RAND_MAX;
  float u1 = ((float)rand() + 1) / (float)RAND_MAX;
  float r = sqrtf(-2 * logf(u0));
  float phi = 2.f * 3.1415926535f * u1;
  next = r * sinf(phi);
  hasNext = true;

  return r * cosf(phi);
}
