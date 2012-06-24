#include "blas.h"
#include "error.h"
#include <stdio.h>

static inline size_t min(size_t a, size_t b) { return (a < b) ? a : b; }
static inline size_t max(size_t a, size_t b) { return (a > b) ? a : b; }
static inline unsigned int maxj(unsigned int a, unsigned int b) { return (a > b) ? a : b; }

static inline CUresult cuMemcpyHtoD2DAsync(CUdeviceptr A, size_t lda, size_t ai, size_t aj,
                                          const void * B, size_t ldb, size_t bi, size_t bj,
                                          size_t m, size_t n, size_t elemSize, CUstream stream) {
  CUDA_MEMCPY2D copy = {
    bi * elemSize, bj, CU_MEMORYTYPE_HOST, B, 0, 0, ldb * elemSize,
    ai * elemSize, aj, CU_MEMORYTYPE_DEVICE, NULL, A, 0, lda * elemSize,
    m * elemSize, n };
  return cuMemcpy2DAsync(&copy, stream);
}

static inline CUresult cuMemcpyDtoH2DAsync(void * A, size_t lda, size_t ai, size_t aj,
                                          CUdeviceptr B, size_t ldb, size_t bi, size_t bj,
                                          size_t m, size_t n, size_t elemSize, CUstream stream) {
  CUDA_MEMCPY2D copy = {
    bi * elemSize, bj, CU_MEMORYTYPE_DEVICE, NULL, B, 0, ldb * elemSize,
    ai * elemSize, aj, CU_MEMORYTYPE_HOST, A, 0, 0, lda * elemSize,
    m * elemSize, n };
  return cuMemcpy2DAsync(&copy, stream);
}

static const float zero = 0.0f;
static const float one = 1.0f;

// #ifdef MKL_ILP64
//   extern void sgemm_(const char *, const char *, const long *, const long *, const long *, const float *, const float *, const long *, const float *, const long *, const float *, float *, const long *);
// #else
//   extern void sgemm_(const char *, const char *, const int *, const int *, const int *, const float *, const float *, const int *, const float *, const int *, const float *, float *, const int *);
// #endif
void sgemm(CBlasTranspose transA, CBlasTranspose transB, size_t m, size_t n, size_t k, float alpha, const float * restrict A, size_t lda, const float * restrict B, size_t ldb, float beta, float * restrict C, size_t ldc) {
  size_t nRowA = (transA == CBlasNoTrans) ? m : k;
  size_t nRowB = (transB == CBlasNoTrans) ? k : n;

  int info = 0;
// #ifdef MKL_ILP64
//   sgemm_((const char *)&transA, (const char *)&transB, (const long *)&m, (const long *)&n, (const long *)&k, &alpha, A, (const long *)&lda, B, (const long *)&ldb, &beta, C, (const long *)&ldc);
// #else
//   sgemm_((const char *)&transA, (const char *)&transB, (const int *)&m, (const int *)&n, (const int *)&k, &alpha, A, (const int *)&lda, B, (const int *)&ldb, &beta, C, (const int *)&ldc);
// #endif
//   return;
  if (lda < max(1, nRowA))
    info = 8;
  else if (ldb < max(1, nRowB))
    info = 10;
  else if (ldc < max(1, m))
    info = 13;
  if (info != 0) {
    XERBLA(info);
    return;
  }

  if (m == 0 || n == 0 || ((alpha == zero || k == 0) && beta == one)) return;

  if (alpha == zero) {
    if (beta == zero) {
#pragma omp parallel for
      for (size_t j = 0; j < n; j++) {
        for (size_t i = 0; i < m; i++)
          C[j * ldc + i] = zero;
      }
    }
    else {
#pragma omp parallel for
      for (size_t j = 0; j < n; j++) {
        for (size_t i = 0; i < m; i++)
          C[j * ldc + i] *= beta;
      }
    }
    return;
  }

  if (transB == CBlasNoTrans) {
    if (transA == CBlasNoTrans) {
#pragma omp parallel for
      for (size_t j = 0; j < n; j++) {
        if (beta == zero) {
          for (size_t i = 0; i < m; i++)
            C[j * ldc + i] = zero;
        }
        else if (beta != one) {
          for (size_t i = 0; i < m; i++)
            C[j * ldc + i] *= beta;
        }
        for (size_t l = 0; l < k; l++) {
          if (B[j * ldb + l] != zero) {
            register float temp = alpha * B[j * ldb + l];
            for (size_t i = 0; i < m; i++)
              C[j * ldc + i] += temp * A[l * lda + i];
          }
        }
      }
    }
    else {
#pragma omp parallel for
      for (size_t j = 0; j < n; j++) {
        for (size_t i = 0; i < m; i++) {
          register float temp = zero;
          for (size_t l = 0; l < k; l++)
            temp += A[i * lda + l] * B[j * ldb + l];
          if (beta == zero)
            C[j * ldc + i] = alpha * temp;
          else
            C[j * ldc + i] = alpha * temp + beta * C[j * ldc + i];
        }
      }
    }
  }
  else {
    if (transA == CBlasNoTrans) {
#pragma omp parallel for
      for (size_t j = 0; j < n; j++) {
        if (beta == zero) {
          for (size_t i = 0; i < m; i++)
            C[j * ldc + i] = zero;
        }
        else if (beta != one) {
          for (size_t i = 0; i < m; i++)
            C[j * ldc + i] *= beta;
        }
        for (size_t l = 0; l < k; l++) {
          if (B[l * ldb + j] != zero) {
            register float temp = alpha * B[l * ldb + j];
            for (size_t i = 0; i < m; i++)
              C[j * ldc + i] += temp * A[l * lda + i];
          }
        }
      }
    }
    else {
#pragma omp parallel for
      for (size_t j = 0; j < n; j++) {
        for (size_t i = 0; i < m; i++) {
          register float temp = zero;
          for (size_t l = 0; l < k; l++)
            temp += A[i * lda + l] * B[l * ldb + j];
          if (beta == zero)
            C[j * ldc + i] = alpha * temp;
          else
            C[j * ldc + i] = alpha * temp + beta * C[j * ldc + i];
        }
      }
    }
  }
}

CUresult cuSgemm(CUmodule module, CBlasTranspose transA, CBlasTranspose transB, size_t m, size_t n, size_t k, float alpha, CUdeviceptr A, size_t lda, CUdeviceptr B, size_t ldb, float beta, CUdeviceptr C, size_t ldc, CUstream stream) {
  size_t nRowA = (transA == CBlasNoTrans) ? m : k;
  size_t nRowB = (transB == CBlasNoTrans) ? k : n;

  int info = 0;
  if (lda < max(1, nRowA))
    info = 8;
  else if (ldb < max(1, nRowB))
    info = 10;
  else if (ldc < max(1, m))
    info = 13;
  if (info != 0) {
    XERBLA(info);
    return CUDA_ERROR_INVALID_VALUE;
  }

  if (m == 0 || n == 0 || ((alpha == zero || k == 0) && beta == one)) return CUDA_SUCCESS;

  const unsigned int mb = (transA == CBlasNoTrans) ? 64 : 32;
  const unsigned int nb = (transA == CBlasNoTrans) ? 16 : 32;
  const unsigned int kb = (transA == CBlasNoTrans) ? 16 : ((transB == CBlasNoTrans) ? 16 : 8);
  const unsigned int bx = (transA == CBlasNoTrans) ? 16 :  8;
  const unsigned int by = (transA == CBlasNoTrans) ?  4 :  8;

  char name[80];
  snprintf(name, 80, "_Z5sgemmIL14CBlasTranspose%dELS0_%dELj%uELj%uELj%uELj%uELj%uEEviiifPKfiS2_ifPfi", transA, transB, mb, nb, kb, bx, by);

  CUfunction function;
  CU_ERROR_CHECK(cuModuleGetFunction(&function, module, name));

  void * params[] = { &m, &n, &k, &alpha, &A, &lda, &B, &ldb, &beta, &C, &ldc };

  CU_ERROR_CHECK(cuLaunchKernel(function, maxj(1, ((unsigned int)m + mb - 1) / mb), maxj(1, ((unsigned int)n + nb - 1) / nb), 1, bx, by, 1, 0, stream, params, NULL));

  return CUDA_SUCCESS;
}

CUresult cuMultiGPUSgemm(CUcontext * contexts, int deviceCount, CBlasTranspose transA, CBlasTranspose transB, size_t m, size_t n, size_t k, float alpha, const float * restrict A, size_t lda, const float * restrict B, size_t ldb, float beta, float * restrict C, size_t ldc) {
  size_t nRowA = (transA == CBlasNoTrans) ? m : k;
  size_t nRowB = (transB == CBlasNoTrans) ? k : n;

  int info = 0;
  if (lda < max(1, nRowA))
    info = 8;
  else if (ldb < max(1, nRowB))
    info = 10;
  else if (ldc < max(1, m))
    info = 13;
  if (info != 0) {
    XERBLA(info);
    return CUDA_ERROR_INVALID_VALUE;
  }

  if (m == 0 || n == 0 || ((k == 0 || alpha == zero) && beta == one)) return CUDA_SUCCESS;

  if (alpha == zero) {
    if (beta == zero) {
      for (size_t j = 0; j < n; j++) {
        for (size_t i = 0; i < m; i++)
          C[j * ldc + i] = zero;
      }
    }
    else {
      for (size_t j = 0; j < n; j++) {
        for (size_t i = 0; i < m; i++)
          C[j * ldc + i] *= beta;
      }
    }
    return CUDA_SUCCESS;
  }

  CUmodule module[deviceCount];
  CUstream stream0[deviceCount], stream1[deviceCount];
  CUdeviceptr dA0[deviceCount], dA1[deviceCount], dB0[deviceCount], dB1[deviceCount], dC[deviceCount];
  size_t dlda0[deviceCount], dlda1[deviceCount], dldb0[deviceCount], dldb1[deviceCount], dldc[deviceCount];

  for (int i = 0; i < deviceCount; i++) {
    CU_ERROR_CHECK(cuCtxPushCurrent(contexts[i]));

    CU_ERROR_CHECK(cuModuleLoad(&module[i], "sgemm.cubin"));
    CU_ERROR_CHECK(cuStreamCreate(&stream0[i], 0));
    CU_ERROR_CHECK(cuStreamCreate(&stream1[i], 0));

    CU_ERROR_CHECK(cuCtxPopCurrent(&contexts[i]));
  }


  int d = 0;
  if (transA == CBlasNoTrans) {
    if (transB == CBlasNoTrans) {

      const size_t mb = 1024, nb = 1024, kb = 1024;

      for (int d = 0; d < deviceCount; d++) {
        CU_ERROR_CHECK(cuCtxPushCurrent(contexts[d]));

        CU_ERROR_CHECK(cuMemAllocPitch(&dA0[d], &dlda0[d], mb * sizeof(float), kb, sizeof(float))); dlda0[d] /= sizeof(float);
        CU_ERROR_CHECK(cuMemAllocPitch(&dA1[d], &dlda1[d], mb * sizeof(float), kb, sizeof(float))); dlda1[d] /= sizeof(float);
        CU_ERROR_CHECK(cuMemAllocPitch(&dB0[d], &dldb0[d], kb * sizeof(float), nb, sizeof(float))); dldb0[d] /= sizeof(float);
        CU_ERROR_CHECK(cuMemAllocPitch(&dB1[d], &dldb1[d], kb * sizeof(float), nb, sizeof(float))); dldb1[d] /= sizeof(float);
        CU_ERROR_CHECK(cuMemAllocPitch(&dC[d], &dldc[d], mb * sizeof(float), nb, sizeof(float))); dldc[d] /= sizeof(float);

        CU_ERROR_CHECK(cuCtxPopCurrent(&contexts[d]));
      }

      for (size_t j = 0; j < n; j += nb) {
        const size_t jb = min(nb, n - j);
        for (size_t i = 0; i < m; i += mb) {
          const size_t ib = min(mb, m - i);

          CU_ERROR_CHECK(cuCtxPushCurrent(contexts[d]));

          CU_ERROR_CHECK(cuMemcpyHtoD2DAsync(dC[d], dldc[d], 0, 0, C, ldc, i, j, ib, jb, sizeof(float), stream1[d]));

          CU_ERROR_CHECK(cuSgemm(module[d], transA, transB, ib, jb, 0, zero, dA0[d], dlda0[d], dB0[d], dldb0[d], beta, dC[d], dldc[d], stream1[d]));

          for (size_t l = 0; l < k; l += kb) {
            const size_t lb = min(kb, k - l);

            CU_ERROR_CHECK(cuMemcpyHtoD2DAsync(dA0[d], dlda0[d], 0, 0, A, lda, i, l, ib, lb, sizeof(float), stream0[d]));
            CU_ERROR_CHECK(cuMemcpyHtoD2DAsync(dB0[d], dldb0[d], 0, 0, B, ldb, l, j, lb, jb, sizeof(float), stream0[d]));
            CU_ERROR_CHECK(cuSgemm(module[d], transA, transB, ib, jb, lb, alpha, dA0[d], dlda0[d], dB0[d], dldb0[d], one, dC[d], dldc[d], stream0[d]));

            l += kb;
            if (l < k) {
              const size_t lb = min(kb, k - l);

              CU_ERROR_CHECK(cuMemcpyHtoD2DAsync(dA1[d], dlda1[d], 0, 0, A, lda, i, l, ib, lb, sizeof(float), stream1[d]));
              CU_ERROR_CHECK(cuMemcpyHtoD2DAsync(dB1[d], dldb1[d], 0, 0, B, ldb, l, j, lb, jb, sizeof(float), stream1[d]));
              CU_ERROR_CHECK(cuSgemm(module[d], transA, transB, ib, jb, lb, alpha, dA1[d], dlda1[d], dB1[d], dldb1[d], one, dC[d], dldc[d], stream1[d]));
            }
          }

          CU_ERROR_CHECK(cuMemcpyDtoH2DAsync(C, ldc, i, j, dC[d], dldc[d], 0, 0, ib, jb, sizeof(float), NULL));

          CU_ERROR_CHECK(cuCtxPopCurrent(&contexts[d]));
          d = (d + 1) % deviceCount;
        }
      }

    }
    else {

      const size_t mb = 1024, nb = 1024, kb = 1024;

      for (int d = 0; d < deviceCount; d++) {
        CU_ERROR_CHECK(cuCtxPushCurrent(contexts[d]));

        CU_ERROR_CHECK(cuMemAllocPitch(&dA0[d], &dlda0[d], mb * sizeof(float), kb, sizeof(float))); dlda0[d] /= sizeof(float);
        CU_ERROR_CHECK(cuMemAllocPitch(&dA1[d], &dlda1[d], mb * sizeof(float), kb, sizeof(float))); dlda1[d] /= sizeof(float);
        CU_ERROR_CHECK(cuMemAllocPitch(&dB0[d], &dldb0[d], nb * sizeof(float), kb, sizeof(float))); dldb0[d] /= sizeof(float);
        CU_ERROR_CHECK(cuMemAllocPitch(&dB1[d], &dldb1[d], nb * sizeof(float), kb, sizeof(float))); dldb1[d] /= sizeof(float);
        CU_ERROR_CHECK(cuMemAllocPitch(&dC[d], &dldc[d], mb * sizeof(float), nb, sizeof(float))); dldc[d] /= sizeof(float);

        CU_ERROR_CHECK(cuCtxPopCurrent(&contexts[d]));
      }

      for (size_t j = 0; j < n; j += nb) {
        const size_t jb = min(nb, n - j);
        for (size_t i = 0; i < m; i += mb) {
          const size_t ib = min(mb, m - i);

          CU_ERROR_CHECK(cuCtxPushCurrent(contexts[d]));

          CU_ERROR_CHECK(cuMemcpyHtoD2DAsync(dC[d], dldc[d], 0, 0, C, ldc, i, j, ib, jb, sizeof(float), stream1[d]));

          CU_ERROR_CHECK(cuSgemm(module[d], transA, transB, ib, jb, 0, zero, dA0[d], dlda0[d], dB0[d], dldb0[d], beta, dC[d], dldc[d], stream1[d]));

          for (size_t l = 0; l < k; l += kb) {
            const size_t lb = min(kb, k - l);

            CU_ERROR_CHECK(cuMemcpyHtoD2DAsync(dA0[d], dlda0[d], 0, 0, A, lda, i, l, ib, lb, sizeof(float), stream0[d]));
            CU_ERROR_CHECK(cuMemcpyHtoD2DAsync(dB0[d], dldb0[d], 0, 0, B, ldb, j, l, jb, lb, sizeof(float), stream0[d]));
            CU_ERROR_CHECK(cuSgemm(module[d], transA, transB, ib, jb, lb, alpha, dA0[d], dlda0[d], dB0[d], dldb0[d], one, dC[d], dldc[d], stream0[d]));

            l += kb;
            if (l < k) {
              const size_t lb = min(kb, k - l);

              CU_ERROR_CHECK(cuMemcpyHtoD2DAsync(dA1[d], dlda1[d], 0, 0, A, lda, i, l, ib, lb, sizeof(float), stream1[d]));
              CU_ERROR_CHECK(cuMemcpyHtoD2DAsync(dB1[d], dldb1[d], 0, 0, B, ldb, j, l, jb, lb, sizeof(float), stream1[d]));
              CU_ERROR_CHECK(cuSgemm(module[d], transA, transB, ib, jb, lb, alpha, dA1[d], dlda1[d], dB1[d], dldb1[d], one, dC[d], dldc[d], stream1[d]));
            }
          }

          CU_ERROR_CHECK(cuMemcpyDtoH2DAsync(C, ldc, i, j, dC[d], dldc[d], 0, 0, ib, jb, sizeof(float), NULL));

          CU_ERROR_CHECK(cuCtxPopCurrent(&contexts[d]));
          d = (d + 1) % deviceCount;
        }
      }

    }
  }
  else {
    if (transB == CBlasNoTrans) {

      const size_t mb = 1024, nb = 1024, kb = 1024;

      for (int d = 0; d < deviceCount; d++) {
        CU_ERROR_CHECK(cuCtxPushCurrent(contexts[d]));

        CU_ERROR_CHECK(cuMemAllocPitch(&dA0[d], &dlda0[d], kb * sizeof(float), mb, sizeof(float))); dlda0[d] /= sizeof(float);
        CU_ERROR_CHECK(cuMemAllocPitch(&dA1[d], &dlda1[d], kb * sizeof(float), mb, sizeof(float))); dlda1[d] /= sizeof(float);
        CU_ERROR_CHECK(cuMemAllocPitch(&dB0[d], &dldb0[d], kb * sizeof(float), nb, sizeof(float))); dldb0[d] /= sizeof(float);
        CU_ERROR_CHECK(cuMemAllocPitch(&dB1[d], &dldb1[d], kb * sizeof(float), nb, sizeof(float))); dldb1[d] /= sizeof(float);
        CU_ERROR_CHECK(cuMemAllocPitch(&dC[d], &dldc[d], mb * sizeof(float), nb, sizeof(float))); dldc[d] /= sizeof(float);

        CU_ERROR_CHECK(cuCtxPopCurrent(&contexts[d]));
      }

      for (size_t j = 0; j < n; j += nb) {
        const size_t jb = min(nb, n - j);
        for (size_t i = 0; i < m; i += mb) {
          const size_t ib = min(mb, m - i);

          CU_ERROR_CHECK(cuCtxPushCurrent(contexts[d]));

          CU_ERROR_CHECK(cuMemcpyHtoD2DAsync(dC[d], dldc[d], 0, 0, C, ldc, i, j, ib, jb, sizeof(float), stream1[d]));

          CU_ERROR_CHECK(cuSgemm(module[d], transA, transB, ib, jb, 0, zero, dA0[d], dlda0[d], dB0[d], dldb0[d], beta, dC[d], dldc[d], stream1[d]));

          for (size_t l = 0; l < k; l += kb) {
            const size_t lb = min(kb, k - l);

            CU_ERROR_CHECK(cuMemcpyHtoD2DAsync(dA0[d], dlda0[d], 0, 0, A, lda, l, i, lb, ib, sizeof(float), stream0[d]));
            CU_ERROR_CHECK(cuMemcpyHtoD2DAsync(dB0[d], dldb0[d], 0, 0, B, ldb, l, j, lb, jb, sizeof(float), stream0[d]));
            CU_ERROR_CHECK(cuSgemm(module[d], transA, transB, ib, jb, lb, alpha, dA0[d], dlda0[d], dB0[d], dldb0[d], one, dC[d], dldc[d], stream0[d]));

            l += kb;
            if (l < k) {
              const size_t lb = min(kb, k - l);

              CU_ERROR_CHECK(cuMemcpyHtoD2DAsync(dA1[d], dlda1[d], 0, 0, A, lda, l, i, lb, ib, sizeof(float), stream1[d]));
              CU_ERROR_CHECK(cuMemcpyHtoD2DAsync(dB1[d], dldb1[d], 0, 0, B, ldb, l, j, lb, jb, sizeof(float), stream1[d]));
              CU_ERROR_CHECK(cuSgemm(module[d], transA, transB, ib, jb, lb, alpha, dA1[d], dlda1[d], dB1[d], dldb1[d], one, dC[d], dldc[d], stream1[d]));
            }
          }

          CU_ERROR_CHECK(cuMemcpyDtoH2DAsync(C, ldc, i, j, dC[d], dldc[d], 0, 0, ib, jb, sizeof(float), NULL));

          CU_ERROR_CHECK(cuCtxPopCurrent(&contexts[d]));
          d = (d + 1) % deviceCount;
        }
      }

    }
    else {

      const size_t mb = 1024, nb = 1024, kb = 1024;

      for (int d = 0; d < deviceCount; d++) {
        CU_ERROR_CHECK(cuCtxPushCurrent(contexts[d]));

        CU_ERROR_CHECK(cuMemAllocPitch(&dA0[d], &dlda0[d], kb * sizeof(float), mb, sizeof(float))); dlda0[d] /= sizeof(float);
        CU_ERROR_CHECK(cuMemAllocPitch(&dA1[d], &dlda1[d], kb * sizeof(float), mb, sizeof(float))); dlda1[d] /= sizeof(float);
        CU_ERROR_CHECK(cuMemAllocPitch(&dB0[d], &dldb0[d], nb * sizeof(float), kb, sizeof(float))); dldb0[d] /= sizeof(float);
        CU_ERROR_CHECK(cuMemAllocPitch(&dB1[d], &dldb1[d], nb * sizeof(float), kb, sizeof(float))); dldb1[d] /= sizeof(float);
        CU_ERROR_CHECK(cuMemAllocPitch(&dC[d], &dldc[d], mb * sizeof(float), nb, sizeof(float))); dldc[d] /= sizeof(float);

        CU_ERROR_CHECK(cuCtxPopCurrent(&contexts[d]));
      }

      for (size_t j = 0; j < n; j += nb) {
        const size_t jb = min(nb, n - j);
        for (size_t i = 0; i < m; i += mb) {
          const size_t ib = min(mb, m - i);

          CU_ERROR_CHECK(cuCtxPushCurrent(contexts[d]));

          CU_ERROR_CHECK(cuMemcpyHtoD2DAsync(dC[d], dldc[d], 0, 0, C, ldc, i, j, ib, jb, sizeof(float), stream1[d]));

          CU_ERROR_CHECK(cuSgemm(module[d], transA, transB, ib, jb, 0, zero, dA0[d], dlda0[d], dB0[d], dldb0[d], beta, dC[d], dldc[d], stream1[d]));

          for (size_t l = 0; l < k; l += kb) {
            const size_t lb = min(kb, k - l);

            CU_ERROR_CHECK(cuMemcpyHtoD2DAsync(dA0[d], dlda0[d], 0, 0, A, lda, l, i, lb, ib, sizeof(float), stream0[d]));
            CU_ERROR_CHECK(cuMemcpyHtoD2DAsync(dB0[d], dldb0[d], 0, 0, B, ldb, j, l, jb, lb, sizeof(float), stream0[d]));
            CU_ERROR_CHECK(cuSgemm(module[d], transA, transB, ib, jb, lb, alpha, dA0[d], dlda0[d], dB0[d], dldb0[d], one, dC[d], dldc[d], stream0[d]));

            l += kb;
            if (l < k) {
              const size_t lb = min(kb, k - l);

              CU_ERROR_CHECK(cuMemcpyHtoD2DAsync(dA1[d], dlda1[d], 0, 0, A, lda, l, i, lb, ib, sizeof(float), stream1[d]));
              CU_ERROR_CHECK(cuMemcpyHtoD2DAsync(dB1[d], dldb1[d], 0, 0, B, ldb, j, l, jb, lb, sizeof(float), stream1[d]));
              CU_ERROR_CHECK(cuSgemm(module[d], transA, transB, ib, jb, lb, alpha, dA1[d], dlda1[d], dB1[d], dldb1[d], one, dC[d], dldc[d], stream1[d]));
            }
          }

          CU_ERROR_CHECK(cuMemcpyDtoH2DAsync(C, ldc, i, j, dC[d], dldc[d], 0, 0, ib, jb, sizeof(float), NULL));

          CU_ERROR_CHECK(cuCtxPopCurrent(&contexts[d]));
          d = (d + 1) % deviceCount;
        }
      }

    }
  }

  for (int d = 0; d < deviceCount; d++) {
    CU_ERROR_CHECK(cuCtxPushCurrent(contexts[d]));

    CU_ERROR_CHECK(cuMemFree(dA0[d]));
    CU_ERROR_CHECK(cuMemFree(dA1[d]));
    CU_ERROR_CHECK(cuMemFree(dB0[d]));
    CU_ERROR_CHECK(cuMemFree(dB1[d]));
    CU_ERROR_CHECK(cuMemFree(dC[d]));

    CU_ERROR_CHECK(cuStreamDestroy(stream0[d]));
    CU_ERROR_CHECK(cuStreamDestroy(stream1[d]));

    CU_ERROR_CHECK(cuModuleUnload(module[d]));

    CU_ERROR_CHECK(cuCtxPopCurrent(&contexts[d]));
  }

  return CUDA_SUCCESS;
}

#if 0
// gcc -I../../include -I/opt/cuda/include -march=native -O2 -pipe -std=c99 -pedantic -Wall -Wextra -Wconversion -c sgemm.c
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include <sys/time.h>

static void sgemm_ref(CBlasTranspose, CBlasTranspose, size_t, size_t, size_t, float, const float * restrict, size_t, const float * restrict, size_t, float, float * restrict, size_t);
static void * malloc2D(size_t, size_t, size_t *, size_t);
static void rand2D(size_t, size_t, float *, size_t);
static void fprintf2D(FILE *, const char *, size_t, size_t, const float *, size_t);
#ifdef GPU
static CUresult cuMemcpyHtoD2D(CUdeviceptr, size_t, size_t, size_t, const void *, size_t, size_t, size_t, size_t, size_t, size_t);
static CUresult cuMemcpyDtoH2D(void *, size_t, size_t, size_t, CUdeviceptr, size_t, size_t, size_t, size_t, size_t, size_t);
#endif

int main(int argc, char * argv[]) {
  CBlasTranspose transA, transB;
  size_t m, n, k;
#ifdef GPU
  int d;

  if (argc < 6 || argc > 7) {
    fprintf(stderr, "Usage: %s <transA> <transB> <m> <n> <k> [device]\nwhere:\n  transA and transB  are 'n' or 'N' for CBlasNoTrans, 't' or 'T' for CBlasTrans or 'c' or 'C' for CBlasConjTrans\n  m, n and k         are the sizes of the matrices\n  device             is the ordinal of the GPU to use (default 0)\n", argv[0]);
    return 1;
  }
#else

  if (argc != 6) {
    fprintf(stderr, "Usage: %s <transA> <transB> <m> <n> <k>\nwhere:\n  transA and transB  are 'n' or 'N' for CBlasNoTrans, 't' or 'T' for CBlasTrans or 'c' or 'C' for CBlasConjTrans\n  m, n and k         are the sizes of the matrices\n", argv[0]);
    return 1;
  }
#endif

  char t;
  if (sscanf(argv[1], "%c", &t) != 1) {
    fprintf(stderr, "Unable to read character from '%s'\n", argv[1]);
    return 1;
  }
  switch (t) {
    case 'N': case 'n': transA = CBlasNoTrans; break;
    case 'T': case 't': transA = CBlasTrans; break;
    case 'C': case 'c': transA = CBlasConjTrans; break;
    default: fprintf(stderr, "Unknown transpose '%c'\n", t); return 1;
  }

  if (sscanf(argv[2], "%c", &t) != 1) {
    fprintf(stderr, "Unable to read character from '%s'\n", argv[2]);
    return 2;
  }
  switch (t) {
    case 'N': case 'n': transB = CBlasNoTrans; break;
    case 'T': case 't': transB = CBlasTrans; break;
    case 'C': case 'c': transB = CBlasConjTrans; break;
    default: fprintf(stderr, "Unknown transpose '%c'\n", t); return 1;
  }

  if (sscanf(argv[3], "%zu", &m) != 1) {
    fprintf(stderr, "Unable to parse number from '%s'\n", argv[3]);
    return 3;
  }

  if (sscanf(argv[4], "%zu", &n) != 1) {
    fprintf(stderr, "Unable to parse number from '%s'\n", argv[4]);
    return 4;
  }

  if (sscanf(argv[5], "%zu", &k) != 1) {
    fprintf(stderr, "Unable to parse number from '%s'\n", argv[5]);
    return 5;
  }
#ifdef GPU
  if (argc == 7) {
    if (sscanf(argv[6], "%d", &d) != 1) {
      fprintf(stderr, "Unable to parse number from '%s'\n", argv[6]);
      return 6;
    }
  }
  else
    d = 0;
#endif

  srand(0);

  float alpha, beta, * A, * B, * C, * refC;
  size_t lda, ldb, ldc;
#ifdef GPU
  CUdeviceptr dA, dB, dC;
  size_t dlda, dldb, dldc;
#endif

#if defined(GPU) || defined(MULTIGPU)
  CU_ERROR_CHECK(cuInit(0));

#ifdef GPU
  CUdevice device;
  CU_ERROR_CHECK(cuDeviceGet(&device, d));

  CUcontext context;
  CU_ERROR_CHECK(cuCtxCreate(&context, CU_CTX_BLOCKING_SYNC, device));
#else
  int deviceCount;
  CU_ERROR_CHECK(cuDeviceGetCount(&deviceCount));

  CUcontext contexts[deviceCount];
  for (int i = 0; i < deviceCount; i++) {
    CUdevice device;
    CU_ERROR_CHECK(cuDeviceGet(&device, i));
    CU_ERROR_CHECK(cuCtxCreate(&contexts[i], CU_CTX_BLOCKING_SYNC, device));
  }
#endif
#endif

  rand2D(1, 1, &alpha, 0);
  rand2D(1, 1, &beta, 0);
  if (m <= 8 && n <= 8 && k <= 8) {
    fprintf2D(stdout, "alpha", 1, 1, &alpha, 0);
    fprintf2D(stdout, "beta", 1, 1, &beta, 0);
  }

  if (transA == CBlasNoTrans) {
    if ((A = malloc2D(m, k, &lda, sizeof(float))) == NULL) {
      fprintf(stderr, "Unable to allocate A\n");
      return CUDA_ERROR_OUT_OF_MEMORY;
    }

    rand2D(m, k, A, lda);

#ifdef GPU
    CU_ERROR_CHECK(cuMemAllocPitch(&dA, &dlda, m * sizeof(float), k, sizeof(float)));
    dlda /= sizeof(float);
    CU_ERROR_CHECK(cuMemcpyHtoD2D(dA, dlda, 0, 0, A, lda, 0, 0, m, k, sizeof(float)));
#endif

    if (m <= 8 && n <= 8 && k <= 8)
      fprintf2D(stdout, "A", m, k, A, lda);
  }
  else {
    if ((A = malloc2D(k, m, &lda, sizeof(float))) == NULL) {
      fprintf(stderr, "Unable to allocate A\n");
      return CUDA_ERROR_OUT_OF_MEMORY;
    }

    rand2D(k, m, A, lda);

#ifdef GPU
    CU_ERROR_CHECK(cuMemAllocPitch(&dA, &dlda, k * sizeof(float), m, sizeof(float)));
    dlda /= sizeof(float);
    CU_ERROR_CHECK(cuMemcpyHtoD2D(dA, dlda, 0, 0, A, lda, 0, 0, k, m, sizeof(float)));
#endif

    if (m <= 8 && n <= 8 && k <= 8)
      fprintf2D(stdout, "A", k, m, A, lda);
  }

  if (transB == CBlasNoTrans) {
    if ((B = malloc2D(k, n, &ldb, sizeof(float))) == NULL) {
      fprintf(stderr, "Unable to allocate B\n");
      return CUDA_ERROR_OUT_OF_MEMORY;
    }

    rand2D(k, n, B, ldb);

#ifdef GPU
    CU_ERROR_CHECK(cuMemAllocPitch(&dB, &dldb, k * sizeof(float), n, sizeof(float)));
    dldb /= sizeof(float);
    CU_ERROR_CHECK(cuMemcpyHtoD2D(dB, dldb, 0, 0, B, ldb, 0, 0, k, n, sizeof(float)));
#endif

    if (m <= 8 && n <= 8 && k <= 8)
      fprintf2D(stdout, "B", k, n, B, ldb);
  }
  else {
    if ((B = malloc2D(n, k, &ldb, sizeof(float))) == NULL) {
      fprintf(stderr, "Unable to allocate B\n");
      return CUDA_ERROR_OUT_OF_MEMORY;
    }

    rand2D(n, k, B, ldb);

#ifdef GPU
    CU_ERROR_CHECK(cuMemAllocPitch(&dB, &dldb, n * sizeof(float), k, sizeof(float)));
    dldb /= sizeof(float);
    CU_ERROR_CHECK(cuMemcpyHtoD2D(dB, dldb, 0, 0, B, ldb, 0, 0, n, k, sizeof(float)));
#endif

    if (m <= 8 && n <= 8 && k <= 8)
      fprintf2D(stdout, "B", n, k, B, ldb);
  }

  if ((C = malloc2D(m, n, &ldc, sizeof(float))) == NULL) {
    fprintf(stderr, "Unable to allocate C\n");
    return CUDA_ERROR_OUT_OF_MEMORY;
  }
  if ((refC = malloc2D(m, n, &ldc, sizeof(float))) == NULL) {
    fprintf(stderr, "Unable to allocate refC\n");
    return CUDA_ERROR_OUT_OF_MEMORY;
  }

  rand2D(m, n, C, ldc);

  for (size_t j = 0; j < n; j++)
    memcpy(&refC[j * ldc], &C[j * ldc], m * sizeof(float));

#ifdef GPU
  CU_ERROR_CHECK(cuMemAllocPitch(&dC, &dldc, m * sizeof(float), n, sizeof(float)));
  dldc /= sizeof(float);
  CU_ERROR_CHECK(cuMemcpyHtoD2D(dC, dldc, 0, 0, C, ldc, 0, 0, m, n, sizeof(float)));
#endif

  if (m <= 8 && n <= 8 && k <= 8)
    fprintf2D(stdout, "C", m, n, C, ldc);

  sgemm_ref(transA, transB, m, n, k, alpha, A, lda, B, ldb, beta, refC, ldc);
#ifdef GPU
  CUmodule module;
  CU_ERROR_CHECK(cuModuleLoad(&module, "sgemm.cubin"));

  CU_ERROR_CHECK(cuSgemm(module, transA, transB, m, n, k, alpha, dA, dlda, dB, dldb, beta, dC, dldc, NULL));

  CU_ERROR_CHECK(cuMemcpyDtoH2D(C, ldc, 0, 0, dC, dldc, 0, 0, m, n, sizeof(float)));
#else
#ifdef MULTIGPU
  CU_ERROR_CHECK(cuMultiGPUSgemm(contexts, deviceCount, transA, transB, m, n, k, alpha, A, lda, B, ldb, beta, C, ldc));
#else
  sgemm(transA, transB, m, n, k, alpha, A, lda, B, ldb, beta, C, ldc);
#endif
#endif

  if (m <= 8 && n <= 8 && k <= 8) {
    fprintf2D(stdout, "Reference SGEMM", m, n, refC, ldc);
    fprintf2D(stdout, "SGEMM", m, n, C, ldc);
  }

  float diff = zero;
  for (size_t j = 0; j < n; j++) {
    for (size_t i = 0; i < m; i++) {
      float d = fabsf(C[j * ldc + i] - refC[j * ldc + i]);
      if (d > diff)
        diff = d;
    }
  }

#ifdef GPU
  CUevent start, stop;
  CU_ERROR_CHECK(cuEventCreate(&start, CU_EVENT_BLOCKING_SYNC));
  CU_ERROR_CHECK(cuEventCreate(&stop, CU_EVENT_BLOCKING_SYNC));

  CU_ERROR_CHECK(cuEventRecord(start, NULL));
  for (size_t i = 0; i < 20; i++)
    CU_ERROR_CHECK(cuSgemm(module, transA, transB, m, n, k, alpha, dA, dlda, dB, dldb, beta, dC, dldc, NULL));
  CU_ERROR_CHECK(cuEventRecord(stop, NULL));
  CU_ERROR_CHECK(cuEventSynchronize(stop));

  float time;
  CU_ERROR_CHECK(cuEventElapsedTime(&time, start, stop));
  time /= 20000;

  CU_ERROR_CHECK(cuEventDestroy(start));
  CU_ERROR_CHECK(cuEventDestroy(stop));
#else
  struct timeval start, stop;
  if (gettimeofday(&start, NULL) != 0) {
    fprintf(stderr, "gettimeofday failed at %s:%d\n", __FILE__, __LINE__);
    return CUDA_ERROR_OPERATING_SYSTEM;
  }
  for (size_t i = 0; i < 20; i++)
#ifdef MULTIGPU
    CU_ERROR_CHECK(cuMultiGPUSgemm(contexts, deviceCount, transA, transB, m, n, k, alpha, A, lda, B, ldb, beta, C, ldc));
#else
    sgemm(transA, transB, m, n, k, alpha, A, lda, B, ldb, beta, C, ldc);
#endif
  if (gettimeofday(&stop, NULL) != 0) {
    fprintf(stderr, "gettimeofday failed at %s:%d\n", __FILE__, __LINE__);
    return CUDA_ERROR_OPERATING_SYSTEM;
  }

  double time = ((double)(stop.tv_sec - start.tv_sec) + (double)(stop.tv_usec - start.tv_usec) * 1.e-6) / 20.0;
#endif

  size_t flops;
  if (alpha == zero || k == 0)
    flops = (beta != one) ? 1 : 0;
  else {
    flops = 2 * k - 1;
    if (alpha != one)
      flops += 1;
    if (beta != zero)
      flops += 2;
  }
  float error = (float)flops * 2.0f * FLT_EPSILON;
  flops *= m * n;

  bool passed = (diff <= error);
  fprintf(stdout, "%.3ems %.3gGFlops/s Error: %.3e\n%sED!\n", time, ((float)flops * 1.e-9f) / time, diff, (passed) ? "PASS" : "FAIL");

  free(A);
  free(B);
  free(C);
  free(refC);
#ifdef GPU
  CU_ERROR_CHECK(cuMemFree(dA));
  CU_ERROR_CHECK(cuMemFree(dB));
  CU_ERROR_CHECK(cuMemFree(dC));

#ifdef MULTIGPU
  for (int i = 0; i < deviceCount; i++)
    CU_ERROR_CHECK(cuCtxDestroy(contexts[i]));
#else
  CU_ERROR_CHECK(cuModuleUnload(module));

  CU_ERROR_CHECK(cuCtxDestroy(context));
#endif
#endif

  return (int)!passed;
}

static void sgemm_ref(CBlasTranspose transA, CBlasTranspose transB, size_t m, size_t n, size_t k, float alpha, const float * restrict A, size_t lda, const float * restrict B, size_t ldb, float beta, float * restrict C, size_t ldc) {
  size_t nRowA = (transA == CBlasNoTrans) ? m : k;
  size_t nRowB = (transB == CBlasNoTrans) ? k : n;

  int info = 0;
  if (lda < max(1, nRowA))
    info = 8;
  else if (ldb < max(1, nRowB))
    info = 10;
  else if (ldc < max(1, m))
    info = 13;
  if (info != 0) {
    XERBLA(info);
    return;
  }

  if (m == 0 || n == 0 || ((k == 0 || alpha == zero) && beta == one)) return;

  if (alpha == zero) {
    if (beta == zero) {
      for (size_t j = 0; j < n; j++) {
        for (size_t i = 0; i < m; i++)
          C[j * ldc + i] = zero;
      }
    }
    else {
      for (size_t j = 0; j < n; j++) {
        for (size_t i = 0; i < m; i++)
          C[j * ldc + i] *= beta;
      }
    }
    return;
  }

  for (size_t j = 0; j < n; j++) {
    for (size_t i = 0; i < m; i++) {

      float temp;
      if (transA == CBlasNoTrans) {
        if (transB == CBlasNoTrans) {
          temp = A[i] * B[j * ldb];
          for (size_t l = 1; l < k; l++)
            temp += A[l * lda + i] * B[j * ldb + l];
        }
        else {
          temp = A[i] * B[j];
          for (size_t l = 1; l < k; l++)
            temp += A[l * lda + i] * B[l * ldb + j];
        }
      }
      else {
        if (transB == CBlasNoTrans) {
          temp = A[i * lda] * B[j * ldb];
          for (size_t l = 1; l < k; l++)
            temp += A[i * lda + l] * B[j * ldb + l];
        }
        else {
          temp = A[i * lda] * B[j];
          for (size_t l = 1; l < k; l++)
            temp += A[i * lda + l] * B[l * ldb + j];
        }
      }

      if (alpha != one)
        temp *= alpha;
      if (beta != zero)
        temp += beta * C[j * ldc + i];

      C[j * ldc + i] = temp;

    }
  }
}

static void * malloc2D(size_t m, size_t n, size_t * ld, size_t elemSize) {
  size_t align = (16 / elemSize) - 1;
  *ld = (m + align) & ~align;
  return malloc(*ld * n * elemSize);
}

static void rand2D(size_t m, size_t n, float * A, size_t lda) {
  for (size_t j = 0; j < n; j++) {
    for (size_t i = 0; i < m; i++)
      A[j * lda + i] = (float)rand() / (float)RAND_MAX;
  }
}

static void fprintf2D(FILE * stream, const char * label, size_t m, size_t n, const float * A, size_t lda) {
  fprintf(stream, "%s =\n", label);
  for (size_t i = 0; i < m; i++) {
    for (size_t j = 0; j < n; j++)
      fprintf(stream, "%15.6f", A[j * lda + i]);
    fputs("\n", stream);
  }
}

#ifdef GPU
static CUresult cuMemcpyHtoD2D(CUdeviceptr A, size_t lda, size_t ai, size_t aj,
                               const void * B, size_t ldb, size_t bi, size_t bj,
                               size_t m, size_t n, size_t elemSize) {
  CUDA_MEMCPY2D copy = {
    bi * elemSize, bj, CU_MEMORYTYPE_HOST, B, 0, 0, ldb * elemSize,
    ai * elemSize, aj, CU_MEMORYTYPE_DEVICE, NULL, A, 0, lda * elemSize,
    m * elemSize, n };
  return cuMemcpy2D(&copy);
}

static CUresult cuMemcpyDtoH2D(void * A, size_t lda, size_t ai, size_t aj,
                               CUdeviceptr B, size_t ldb, size_t bi, size_t bj,
                               size_t m, size_t n, size_t elemSize) {
  CUDA_MEMCPY2D copy = {
    bi * elemSize, bj, CU_MEMORYTYPE_DEVICE, NULL, B, 0, ldb * elemSize,
    ai * elemSize, aj, CU_MEMORYTYPE_HOST, A, 0, 0, lda * elemSize,
    m * elemSize, n };
  return cuMemcpy2D(&copy);
}
#endif
#endif
