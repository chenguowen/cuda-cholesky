#include "blas.h"
#include "error.h"
#include <stdio.h>
#include "../handle.h"

static inline size_t min(size_t a, size_t b) { return (a < b) ? a : b; }
static inline size_t max(size_t a, size_t b) { return (a > b) ? a : b; }

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

void sgemm(CBlasTranspose transA, CBlasTranspose transB,
           size_t m, size_t n, size_t k,
           float alpha, const float * restrict A, size_t lda, const float * restrict B, size_t ldb,
           float beta, float * restrict C, size_t ldc) {
  size_t nRowA = (transA == CBlasNoTrans) ? m : k;
  size_t nRowB = (transB == CBlasNoTrans) ? k : n;

  int info = 0;
  if (lda < nRowA)
    info = 8;
  else if (ldb < nRowB)
    info = 10;
  else if (ldc < m)
    info = 13;
  if (info != 0) {
    XERBLA(info);
    return;
  }

  if (m == 0 || n == 0 || ((alpha == zero || k == 0) && beta == one))
    return;

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

CUresult cuSgemm2(CUhandle handle, CBlasTranspose transA, CBlasTranspose transB,
                  size_t m, size_t n, size_t k,
                  float alpha, CUdeviceptr A, size_t lda, CUdeviceptr B, size_t ldb,
                  float beta, CUdeviceptr C, size_t ldc, CUdeviceptr D, size_t ldd,
                  CUstream stream) {
  size_t nRowA = (transA == CBlasNoTrans) ? m : k;
  size_t nRowB = (transB == CBlasNoTrans) ? k : n;

  int info = 0;
  if (lda < nRowA)
    info = 8;
  else if (ldb < nRowB)
    info = 10;
  else if (ldc < m)
    info = 13;
  else if (ldd < m)
    info = 15;
  if (info != 0) {
    XERBLA(info);
    return CUDA_ERROR_INVALID_VALUE;
  }

  if (m == 0 || n == 0 || ((alpha == zero || k == 0) && beta == one))
    return CUDA_SUCCESS;

  const unsigned int mb = (transA == CBlasNoTrans) ? 64 : 32;
  const unsigned int nb = (transA == CBlasNoTrans) ? 16 : 32;
  const unsigned int kb = (transA == CBlasNoTrans) ? 16 :  8;
  const unsigned int bx = (transA == CBlasNoTrans) ? 16 :  8;
  const unsigned int by = (transA == CBlasNoTrans) ?  4 :  8;

  CUmodule module;
  CU_ERROR_CHECK(cuHandleGetModule(handle, &module, CU_HANDLE_SINGLE, CU_HANDLE_GEMM));

  char name[84];
  snprintf(name, 84,
           "_Z5sgemmIL14CBlasTranspose%dELS0_%dELj%uELj%uELj%uELj%uELj%uEEviiifPKfiS2_ifS2_iPfi",
           transA, transB, mb, nb, kb, bx, by);

  CUfunction function;
  CU_ERROR_CHECK(cuModuleGetFunction(&function, module, name));

  void * params[] = { &m, &n, &k, &alpha, &A, &lda, &B, &ldb, &beta, &C, &ldc, &D, &ldd };

  CU_ERROR_CHECK(cuLaunchKernel(function, (unsigned int)(m + mb - 1) / mb, (unsigned int)(n + nb - 1) / nb, 1,
                                bx, by, 1, 0, stream, params, NULL));

  return CUDA_SUCCESS;
}

CUresult cuMultiGPUSgemm(CUhandle * handles, int deviceCount,
                         CBlasTranspose transA, CBlasTranspose transB,
                         size_t m, size_t n, size_t k,
                         float alpha, const float * restrict A, size_t lda, const float * restrict B, size_t ldb,
                         float beta, float * restrict C, size_t ldc) {
  size_t nRowA = (transA == CBlasNoTrans) ? m : k;
  size_t nRowB = (transB == CBlasNoTrans) ? k : n;

  int info = 0;
  if (lda < nRowA)
    info = 8;
  else if (ldb < nRowB)
    info = 10;
  else if (ldc < m)
    info = 13;
  if (info != 0) {
    XERBLA(info);
    return CUDA_ERROR_INVALID_VALUE;
  }

  if (m == 0 || n == 0 || ((alpha == zero || k == 0) && beta == one))
    return CUDA_SUCCESS;

  if (alpha == zero) {
    if (beta == zero) {
#pragma omp parallel for
      for (size_t j = 0; j < n; j++) {
        for (size_t i = 0; i < m; i++)
          C[j * ldc + i] = zero;
      }
    }
    else {
      for (size_t j = 0; j < n; j++) {
#pragma omp parallel for
        for (size_t i = 0; i < m; i++)
          C[j * ldc + i] *= beta;
      }
    }
    return CUDA_SUCCESS;
  }

  CUstream stream0[deviceCount], stream1[deviceCount];
  CUdeviceptr dA0[deviceCount], dA1[deviceCount], dB0[deviceCount], dB1[deviceCount], dC[deviceCount];
  size_t dlda0[deviceCount], dlda1[deviceCount], dldb0[deviceCount], dldb1[deviceCount], dldc[deviceCount];

  int d = 0;
  if (transA == CBlasNoTrans) {
    /*
     * Each GPU MP processes blocks of 64x16 using 64 threads per block.
     * There are 30 MPs on the GTX 280 and each requires a minimum of 3 blocks
     * to mask memory latency (64 * 3 = 192 threads/6 warps).  We can fit a
     * maximum of 8 blocks on each MP due to shared memory and register
     * requirements.  Best performance should therefore occur when we have
     * over 180 blocks sent to the GPU.  This requires a 9x20, 12x15, 6x30, etc.
     * block size here.  9x20 is chosen to retain the m >> n behaviour needed
     * for SPOTRF('L',..).
     * mb =  9 * 64 = 576
     * nb = 20 * 16 = 320
     * kb defines the amount of work done by each thread and the memory (and
     * bandwidth) needed for A and B so needs to be tuned to give maximum
     * performance.  kb = 512 gives 400GFlops/s.
     * In SPOTRF('L',...) k ~ m so 512 seems sensible.
     */
    const size_t mb = 576, nb = 320, kb = 512;

    if (transB == CBlasNoTrans) {

      for (int d = 0; d < deviceCount; d++) {
        CUcontext ctx = cuHandleGetContext(handles[d]);
        CU_ERROR_CHECK(cuCtxPushCurrent(ctx));

        CU_ERROR_CHECK(cuHandleGetStream(handles[d], &stream0[d], 0));
        CU_ERROR_CHECK(cuHandleGetStream(handles[d], &stream1[d], 1));

        CUdeviceptr ptr;
        size_t pitch;
        CU_ERROR_CHECK(cuHandleMemAllocPitch(handles[d], &ptr, &pitch, max(mb, kb) * sizeof(float), max(kb, nb), sizeof(float)));

        dA0[d] = ptr; dlda0[d] = pitch / sizeof(float);
        dA1[d] = ptr + pitch * kb; dlda1[d] = pitch / sizeof(float);
        dB0[d] = ptr + pitch * kb * 2; dldb0[d] = pitch / sizeof(float);
        dB1[d] = ptr + pitch * kb * 2 + pitch * nb; dldb1[d] = pitch / sizeof(float);
        dC[d] = ptr + pitch * kb * 2 + pitch * nb * 2; dldc[d] = pitch / sizeof(float);

        CU_ERROR_CHECK(cuCtxPopCurrent(&ctx));
      }

      for (size_t j = 0; j < n; j += nb) {
        const size_t jb = min(nb, n - j);
        for (size_t i = 0; i < m; i += mb) {
          const size_t ib = min(mb, m - i);

          CUcontext ctx = cuHandleGetContext(handles[i]);
          CU_ERROR_CHECK(cuCtxPushCurrent(ctx));

          CU_ERROR_CHECK(cuMemcpyHtoD2DAsync(dC[d], dldc[d], 0, 0, C, ldc, i, j, ib, jb, sizeof(float), stream1[d]));

          CU_ERROR_CHECK(cuSgemm(handles[i], transA, transB, ib, jb, 0, zero, dA0[d], dlda0[d], dB0[d], dldb0[d], beta, dC[d], dldc[d], stream1[d]));

          for (size_t l = 0; l < k; l += kb) {
            const size_t lb = min(kb, k - l);

            CU_ERROR_CHECK(cuMemcpyHtoD2DAsync(dA0[d], dlda0[d], 0, 0, A, lda, i, l, ib, lb, sizeof(float), stream0[d]));
            CU_ERROR_CHECK(cuMemcpyHtoD2DAsync(dB0[d], dldb0[d], 0, 0, B, ldb, l, j, lb, jb, sizeof(float), stream0[d]));
            CU_ERROR_CHECK(cuSgemm(handles[d], transA, transB, ib, jb, lb, alpha, dA0[d], dlda0[d], dB0[d], dldb0[d], one, dC[d], dldc[d], stream0[d]));

            l += kb;
            if (l < k) {
              const size_t lb = min(kb, k - l);

              CU_ERROR_CHECK(cuMemcpyHtoD2DAsync(dA1[d], dlda1[d], 0, 0, A, lda, i, l, ib, lb, sizeof(float), stream1[d]));
              CU_ERROR_CHECK(cuMemcpyHtoD2DAsync(dB1[d], dldb1[d], 0, 0, B, ldb, l, j, lb, jb, sizeof(float), stream1[d]));
              CU_ERROR_CHECK(cuSgemm(handles[d], transA, transB, ib, jb, lb, alpha, dA1[d], dlda1[d], dB1[d], dldb1[d], one, dC[d], dldc[d], stream1[d]));
            }
          }

          CU_ERROR_CHECK(cuMemcpyDtoH2DAsync(C, ldc, i, j, dC[d], dldc[d], 0, 0, ib, jb, sizeof(float), NULL));

          CU_ERROR_CHECK(cuCtxPopCurrent(&ctx));
          d = (d + 1) % deviceCount;
        }
      }

    }
    else {

      for (int d = 0; d < deviceCount; d++) {
        CUcontext ctx = cuHandleGetContext(handles[d]);
        CU_ERROR_CHECK(cuCtxPushCurrent(ctx));

        CU_ERROR_CHECK(cuHandleGetStream(handles[d], &stream0[d], 0));
        CU_ERROR_CHECK(cuHandleGetStream(handles[d], &stream1[d], 1));

        CUdeviceptr ptr;
        size_t pitch;
        CU_ERROR_CHECK(cuHandleMemAllocPitch(handles[d], &ptr, &pitch, max(mb, nb) * sizeof(float), max(kb, nb), sizeof(float)));

        dA0[d] = ptr; dlda0[d] = pitch / sizeof(float);
        dA1[d] = ptr + pitch * kb; dlda1[d] = pitch / sizeof(float);
        dB0[d] = ptr + pitch * kb * 2; dldb0[d] = pitch / sizeof(float);
        dB1[d] = ptr + pitch * kb * 2 + pitch * kb; dldb1[d] = pitch / sizeof(float);
        dC[d] = ptr + pitch * kb * 2 + pitch * kb * 2; dldc[d] = pitch / sizeof(float);

        CU_ERROR_CHECK(cuCtxPopCurrent(&ctx));
      }

      for (size_t j = 0; j < n; j += nb) {
        const size_t jb = min(nb, n - j);
        for (size_t i = 0; i < m; i += mb) {
          const size_t ib = min(mb, m - i);

          CUcontext ctx = cuHandleGetContext(handles[d]);
          CU_ERROR_CHECK(cuCtxPushCurrent(ctx));

          CU_ERROR_CHECK(cuMemcpyHtoD2DAsync(dC[d], dldc[d], 0, 0, C, ldc, i, j, ib, jb, sizeof(float), stream1[d]));

          CU_ERROR_CHECK(cuSgemm(handles[d], transA, transB, ib, jb, 0, zero, dA0[d], dlda0[d], dB0[d], dldb0[d], beta, dC[d], dldc[d], stream1[d]));

          for (size_t l = 0; l < k; l += kb) {
            const size_t lb = min(kb, k - l);

            CU_ERROR_CHECK(cuMemcpyHtoD2DAsync(dA0[d], dlda0[d], 0, 0, A, lda, i, l, ib, lb, sizeof(float), stream0[d]));
            CU_ERROR_CHECK(cuMemcpyHtoD2DAsync(dB0[d], dldb0[d], 0, 0, B, ldb, j, l, jb, lb, sizeof(float), stream0[d]));
            CU_ERROR_CHECK(cuSgemm(handles[d], transA, transB, ib, jb, lb, alpha, dA0[d], dlda0[d], dB0[d], dldb0[d], one, dC[d], dldc[d], stream0[d]));

            l += kb;
            if (l < k) {
              const size_t lb = min(kb, k - l);

              CU_ERROR_CHECK(cuMemcpyHtoD2DAsync(dA1[d], dlda1[d], 0, 0, A, lda, i, l, ib, lb, sizeof(float), stream1[d]));
              CU_ERROR_CHECK(cuMemcpyHtoD2DAsync(dB1[d], dldb1[d], 0, 0, B, ldb, j, l, jb, lb, sizeof(float), stream1[d]));
              CU_ERROR_CHECK(cuSgemm(handles[d], transA, transB, ib, jb, lb, alpha, dA1[d], dlda1[d], dB1[d], dldb1[d], one, dC[d], dldc[d], stream1[d]));
            }
          }

          CU_ERROR_CHECK(cuMemcpyDtoH2DAsync(C, ldc, i, j, dC[d], dldc[d], 0, 0, ib, jb, sizeof(float), NULL));

          CU_ERROR_CHECK(cuCtxPopCurrent(&ctx));
          d = (d + 1) % deviceCount;
        }
      }

    }
  }
  else {
    /*
     * Each GPU MP processes blocks of 32x32 using 64 threads per block.
     * There are 30 MPs on the GTX 280 and each requires a minimum of 3 blocks
     * to mask memory latency (64 * 3 = 192 threads/6 warps).  We can fit a
     * maximum of 6 blocks on each MP due to shared memory and register
     * requirements.  Best performance should therefore occur when we have over
     * 90 blocks sent to the GPU.  This requires a 9x10, 6x15, 3x30, etc. block
     * size here.  6x15 is chosen to retain the m << n behaviour needed for
     * SPOTRF('U',..).
     * mb =  6 * 32 = 192
     * nb = 15 * 32 = 480
     * kb defines the amount of work done by each thread and the memory (and
     * bandwidth) needed for A and B so needs to be tuned to give maximum
     * performance.  kb = 320 gives 250GFlops/s.
     * In SPOTRF('U',...) k ~ n so 320 seems odd.
     */
    const size_t mb = 192, nb = 480, kb = 320;

    if (transB == CBlasNoTrans) {

      for (int d = 0; d < deviceCount; d++) {
        CUcontext ctx = cuHandleGetContext(handles[d]);
        CU_ERROR_CHECK(cuCtxPushCurrent(ctx));

        CU_ERROR_CHECK(cuHandleGetStream(handles[d], &stream0[d], 0));
        CU_ERROR_CHECK(cuHandleGetStream(handles[d], &stream1[d], 1));

        CUdeviceptr ptr;
        size_t pitch;
        CU_ERROR_CHECK(cuHandleMemAllocPitch(handles[d], &ptr, &pitch, max(mb, kb) * sizeof(float), max(mb, nb), sizeof(float)));

        dA0[d] = ptr; dlda0[d] = pitch / sizeof(float);
        dA1[d] = ptr + pitch * mb; dlda1[d] = pitch / sizeof(float);
        dB0[d] = ptr + pitch * mb * 2; dldb0[d] = pitch / sizeof(float);
        dB1[d] = ptr + pitch * mb * 2 + pitch * nb; dldb1[d] = pitch / sizeof(float);
        dC[d] = ptr + pitch * mb * 2 + pitch * nb * 2; dldc[d] = pitch / sizeof(float);

        CU_ERROR_CHECK(cuCtxPopCurrent(&ctx));
      }

      for (size_t j = 0; j < n; j += nb) {
        const size_t jb = min(nb, n - j);
        for (size_t i = 0; i < m; i += mb) {
          const size_t ib = min(mb, m - i);

          CUcontext ctx = cuHandleGetContext(handles[d]);
          CU_ERROR_CHECK(cuCtxPushCurrent(ctx));

          CU_ERROR_CHECK(cuMemcpyHtoD2DAsync(dC[d], dldc[d], 0, 0, C, ldc, i, j, ib, jb, sizeof(float), stream1[d]));

          CU_ERROR_CHECK(cuSgemm(handles[d], transA, transB, ib, jb, 0, zero, dA0[d], dlda0[d], dB0[d], dldb0[d], beta, dC[d], dldc[d], stream1[d]));

          for (size_t l = 0; l < k; l += kb) {
            const size_t lb = min(kb, k - l);

            CU_ERROR_CHECK(cuMemcpyHtoD2DAsync(dA0[d], dlda0[d], 0, 0, A, lda, l, i, lb, ib, sizeof(float), stream0[d]));
            CU_ERROR_CHECK(cuMemcpyHtoD2DAsync(dB0[d], dldb0[d], 0, 0, B, ldb, l, j, lb, jb, sizeof(float), stream0[d]));
            CU_ERROR_CHECK(cuSgemm(handles[d], transA, transB, ib, jb, lb, alpha, dA0[d], dlda0[d], dB0[d], dldb0[d], one, dC[d], dldc[d], stream0[d]));

            l += kb;
            if (l < k) {
              const size_t lb = min(kb, k - l);

              CU_ERROR_CHECK(cuMemcpyHtoD2DAsync(dA1[d], dlda1[d], 0, 0, A, lda, l, i, lb, ib, sizeof(float), stream1[d]));
              CU_ERROR_CHECK(cuMemcpyHtoD2DAsync(dB1[d], dldb1[d], 0, 0, B, ldb, l, j, lb, jb, sizeof(float), stream1[d]));
              CU_ERROR_CHECK(cuSgemm(handles[d], transA, transB, ib, jb, lb, alpha, dA1[d], dlda1[d], dB1[d], dldb1[d], one, dC[d], dldc[d], stream1[d]));
            }
          }

          CU_ERROR_CHECK(cuMemcpyDtoH2DAsync(C, ldc, i, j, dC[d], dldc[d], 0, 0, ib, jb, sizeof(float), NULL));

          CU_ERROR_CHECK(cuCtxPopCurrent(&ctx));
          d = (d + 1) % deviceCount;
        }
      }
    }
    else {

      for (int d = 0; d < deviceCount; d++) {
        CUcontext ctx = cuHandleGetContext(handles[d]);
        CU_ERROR_CHECK(cuCtxPushCurrent(ctx));

        CU_ERROR_CHECK(cuHandleGetStream(handles[d], &stream0[d], 0));
        CU_ERROR_CHECK(cuHandleGetStream(handles[d], &stream1[d], 1));

        CUdeviceptr ptr;
        size_t pitch;
        CU_ERROR_CHECK(cuHandleMemAllocPitch(handles[d], &ptr, &pitch, max(max(mb, nb), kb) * sizeof(float), max(max(mb, nb), kb), sizeof(float)));

        dA0[d] = ptr; dlda0[d] = pitch / sizeof(float);
        dA1[d] = ptr + pitch * mb; dlda1[d] = pitch / sizeof(float);
        dB0[d] = ptr + pitch * mb * 2; dldb0[d] = pitch / sizeof(float);
        dB1[d] = ptr + pitch * mb * 2 + pitch * kb; dldb1[d] = pitch / sizeof(float);
        dC[d] = ptr + pitch * mb * 2 + pitch * kb * 2; dldc[d] = pitch / sizeof(float);

        CU_ERROR_CHECK(cuCtxPopCurrent(&ctx));
      }

      for (size_t j = 0; j < n; j += nb) {
        const size_t jb = min(nb, n - j);
        for (size_t i = 0; i < m; i += mb) {
          const size_t ib = min(mb, m - i);

          CUcontext ctx = cuHandleGetContext(handles[d]);
          CU_ERROR_CHECK(cuCtxPushCurrent(ctx));

          CU_ERROR_CHECK(cuMemcpyHtoD2DAsync(dC[d], dldc[d], 0, 0, C, ldc, i, j, ib, jb, sizeof(float), stream1[d]));

          CU_ERROR_CHECK(cuSgemm(handles[d], transA, transB, ib, jb, 0, zero, dA0[d], dlda0[d], dB0[d], dldb0[d], beta, dC[d], dldc[d], stream1[d]));

          for (size_t l = 0; l < k; l += kb) {
            const size_t lb = min(kb, k - l);

            CU_ERROR_CHECK(cuMemcpyHtoD2DAsync(dA0[d], dlda0[d], 0, 0, A, lda, l, i, lb, ib, sizeof(float), stream0[d]));
            CU_ERROR_CHECK(cuMemcpyHtoD2DAsync(dB0[d], dldb0[d], 0, 0, B, ldb, j, l, jb, lb, sizeof(float), stream0[d]));
            CU_ERROR_CHECK(cuSgemm(handles[d], transA, transB, ib, jb, lb, alpha, dA0[d], dlda0[d], dB0[d], dldb0[d], one, dC[d], dldc[d], stream0[d]));

            l += kb;
            if (l < k) {
              const size_t lb = min(kb, k - l);

              CU_ERROR_CHECK(cuMemcpyHtoD2DAsync(dA1[d], dlda1[d], 0, 0, A, lda, l, i, lb, ib, sizeof(float), stream1[d]));
              CU_ERROR_CHECK(cuMemcpyHtoD2DAsync(dB1[d], dldb1[d], 0, 0, B, ldb, j, l, jb, lb, sizeof(float), stream1[d]));
              CU_ERROR_CHECK(cuSgemm(handles[d], transA, transB, ib, jb, lb, alpha, dA1[d], dlda1[d], dB1[d], dldb1[d], one, dC[d], dldc[d], stream1[d]));
            }
          }

          CU_ERROR_CHECK(cuMemcpyDtoH2DAsync(C, ldc, i, j, dC[d], dldc[d], 0, 0, ib, jb, sizeof(float), NULL));

          CU_ERROR_CHECK(cuCtxPopCurrent(&ctx));
          d = (d + 1) % deviceCount;
        }
      }

    }
  }

  return CUDA_SUCCESS;
}
