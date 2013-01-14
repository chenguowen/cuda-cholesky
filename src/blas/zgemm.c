#include "blas.h"
#include "error.h"
#include <stdio.h>
#include "handle.h"

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

static const double complex zero = 0.0 + 0.0 * I;
static const double complex one = 1.0 + 0.0 * I;

void zgemm(CBlasTranspose transA, CBlasTranspose transB,
           size_t m, size_t n, size_t k,
           double complex alpha, const double complex * restrict A, size_t lda, const double complex * restrict B, size_t ldb,
           double complex beta, double complex * restrict C, size_t ldc) {
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
            register double complex temp = alpha * B[j * ldb + l];
            for (size_t i = 0; i < m; i++)
              C[j * ldc + i] += temp * A[l * lda + i];
          }
        }
      }
    }
    else if (transA == CBlasConjTrans) {
#pragma omp parallel for
      for (size_t j = 0; j < n; j++) {
        for (size_t i = 0; i < m; i++) {
          register double complex temp = zero;
          for (size_t l = 0; l < k; l++)
            temp += conj(A[i * lda + l]) * B[j * ldb + l];
          if (beta == zero)
            C[j * ldc + i] = alpha * temp;
          else
            C[j * ldc + i] = alpha * temp + beta * C[j * ldc + i];
        }
      }
    }
    else {
#pragma omp parallel for
      for (size_t j = 0; j < n; j++) {
        for (size_t i = 0; i < m; i++) {
          register double complex temp = zero;
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
  else if (transB == CBlasConjTrans) {
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
            register double complex temp = alpha * conj(B[l * ldb + j]);
            for (size_t i = 0; i < m; i++)
              C[j * ldc + i] += temp * A[l * lda + i];
          }
        }
      }
    }
    else if (transA == CBlasConjTrans) {
#pragma omp parallel for
      for (size_t j = 0; j < n; j++) {
        for (size_t i = 0; i < m; i++) {
          register double complex temp = zero;
          for (size_t l = 0; l < k; l++)
            temp += conj(A[i * lda + l]) * conj(B[l * ldb + j]);
          if (beta == zero)
            C[j * ldc + i] = alpha * temp;
          else
            C[j * ldc + i] = alpha * temp + beta * C[j * ldc + i];
        }
      }
    }
    else {
#pragma omp parallel for
      for (size_t j = 0; j < n; j++) {
        for (size_t i = 0; i < m; i++) {
          register double complex temp = zero;
          for (size_t l = 0; l < k; l++)
            temp += A[i * lda + l] * conj(B[l * ldb + j]);
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
            register double complex temp = alpha * B[l * ldb + j];
            for (size_t i = 0; i < m; i++)
              C[j * ldc + i] += temp * A[l * lda + i];
          }
        }
      }
    }
    else if (transA == CBlasConjTrans) {
#pragma omp parallel for
      for (size_t j = 0; j < n; j++) {
        for (size_t i = 0; i < m; i++) {
          register double complex temp = zero;
          for (size_t l = 0; l < k; l++)
            temp += conj(A[i * lda + l]) * B[l * ldb + j];
          if (beta == zero)
            C[j * ldc + i] = alpha * temp;
          else
            C[j * ldc + i] = alpha * temp + beta * C[j * ldc + i];
        }
      }
    }
    else {
#pragma omp parallel for
      for (size_t j = 0; j < n; j++) {
        for (size_t i = 0; i < m; i++) {
          register double complex temp = zero;
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

CUresult cuZgemm2(CUmodule module,
                  CBlasTranspose transA, CBlasTranspose transB,
                  size_t m, size_t n, size_t k,
                  double complex alpha, CUdeviceptr A, size_t lda, CUdeviceptr B, size_t ldb,
                  double complex beta, CUdeviceptr C, size_t ldc, CUdeviceptr D, size_t ldd,
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

  unsigned int mb, nb, kb, bx, by;
  char name[95];

  if (transA == CBlasNoTrans) {
    mb = 64; nb =  4; kb = 16;
    bx = (transB == CBlasNoTrans) ? 16 :  4;
    by = (transB == CBlasNoTrans) ?  4 : 16;
    snprintf(name, 90, "_Z6zgemmNIL14CBlasTranspose%dELj64ELj4ELj16ELj%uELj%uEEv7double2S1_PKS1_S3_S3_PS1_iiiiiii", transB, bx, by);
  }
  else {
    mb =  8;
    nb = (transB == CBlasNoTrans) ?  8 : 16;
    kb = (transB == CBlasNoTrans) ?  4 :  8;
    bx = (transB == CBlasNoTrans) ?  4 :  8;
    by =  8;
    snprintf(name, 95, "_Z6zgemmTIL14CBlasTranspose%dELS0_%dELj8ELj%uELj%uELj%uELj8EEv7double2S1_PKS1_S3_S3_PS1_iiiiiii", transA, transB, nb, kb, bx);
  }

  CUfunction function;
  CU_ERROR_CHECK(cuModuleGetFunction(&function, module, name));

  void * params[] = { &alpha, &beta, &A, &B, &C, &D, &lda, &ldb, &ldc, &ldd, &m, &n, &k };

  CU_ERROR_CHECK(cuLaunchKernel(function, (unsigned int)(m + mb - 1) / mb, (unsigned int)(n + nb - 1) / nb, 1,
                                bx, by, 1, 0, stream, params, NULL));

  return CUDA_SUCCESS;
}

struct zgemm_args {
  struct multigpu_blas_plan * plan;
  const double complex * A, * B;
  double complex * C;
  size_t m, n, k, lda, ldb, ldc;
  double complex alpha, beta;
  CBlasTranspose transA, transB;
};

static CUresult background_zgemm(const void * a) {
  struct zgemm_args * args = (struct zgemm_args *)a;
  struct multigpu_blas_plan * plan = args->plan;

  const size_t mb = (args->transA == CBlasNoTrans) ? ZGEMM_N_MB : ((args->transB == CBlasNoTrans) ? ZGEMM_CN_MB : ZGEMM_CC_MB);
  const size_t nb = (args->transA == CBlasNoTrans) ? ZGEMM_N_NB : ((args->transB == CBlasNoTrans) ? ZGEMM_CN_NB : ZGEMM_CC_NB);
  const size_t kb = (args->transA == CBlasNoTrans) ? ZGEMM_N_KB : ((args->transB == CBlasNoTrans) ? ZGEMM_CN_KB : ZGEMM_CC_KB);

  CUdeviceptr A0 = plan->A;
  CUdeviceptr A1 = plan->A + plan->lda * mb;
  const size_t lda = plan->lda / sizeof(double complex);
  CUdeviceptr B0 = plan->B;
  CUdeviceptr B1 = plan->B + plan->ldb * ((args->transB == CBlasNoTrans) ? nb : kb);
  const size_t ldb = plan->ldb / sizeof(double complex);
  CUdeviceptr C = plan->C;
  const size_t ldc = plan->ldc / sizeof(double complex);

  // Copy C onto the device using the compute stream
  CU_ERROR_CHECK(cuMemcpyHtoD2DAsync(C, ldc, 0, 0,
                                     args->C, args->ldc, 0, 0,
                                     args->m, args->n, sizeof(double complex), plan->compute));

  // Perform C *= beta on the compute stream to ensure C has finished copying
  CU_ERROR_CHECK(cuZgemm(plan->zgemm, CBlasNoTrans, CBlasNoTrans,
                         args->m, args->n, 0,
                         zero, 0, ldc, 0, 0,
                         args->beta, C, ldc, plan->compute));

  // Can exit early if alpha * op(A) * op(B) will evaluate to zero
  if (args->alpha != zero && args->k > 0) {

    // Perform C += alpha * op(A) * op(B)
    if (args->transB == CBlasNoTrans) {
      if (args->transA == CBlasNoTrans) {
        // Copy A and B onto the device asynchronously on the same stream as C
        const size_t lb = min(args->k, kb);
        CU_ERROR_CHECK(cuMemcpyHtoD2DAsync(A0, lda, 0, 0,
                                           args->A, args->lda, 0, 0,
                                           args->m, lb, sizeof(double complex), plan->compute));
        CU_ERROR_CHECK(cuMemcpyHtoD2DAsync(B0, ldb, 0, 0,
                                           args->B, args->ldb, 0, 0,
                                           lb, args->n, sizeof(double complex), plan->compute));

        for (size_t l = 0; l < args->k; l += kb) {
          // Compute C on the same stream as the copies to ensure they have finished first
          CU_ERROR_CHECK(cuZgemm(plan->zgemm, args->transA, args->transB,
                                 args->m, args->n, min(args->k - l, kb),
                                 args->alpha, A0, lda, B0, ldb,
                                 one, C, ldc, plan->compute));

          // If there is more work to do
          if (l + kb < args->k) {
            const size_t lb = min(args->k - l - kb, kb);
            // Copy the next blocks of A and B on the opposite stream from the zgemm
            CU_ERROR_CHECK(cuMemcpyHtoD2DAsync(A1, lda, 0, 0,
                                               args->A, args->lda, 0, l + kb,
                                               args->m, lb, sizeof(double complex), plan->copy));
            CU_ERROR_CHECK(cuMemcpyHtoD2DAsync(B1, ldb, 0, 0,
                                               args->B, args->ldb, l + kb, 0,
                                               lb, args->n, sizeof(double complex), plan->copy));

            // Swap the streams and pointers so that the compute starts after the copy
            CUstream stream = plan->compute; plan->compute = plan->copy; plan->copy = stream;
            CUdeviceptr ptr = A0; A0 = A1; A1 = ptr;
            ptr = B0; B0 = B1; B1 = ptr;
          }
        }
      }
      else {
        // Copy A and B onto the device asynchronously on the same stream as C
        const size_t lb = min(args->k, kb);
        CU_ERROR_CHECK(cuMemcpyHtoD2DAsync(A0, lda, 0, 0,
                                           args->A, args->lda, 0, 0,
                                           lb, args->m, sizeof(double complex), plan->compute));
        CU_ERROR_CHECK(cuMemcpyHtoD2DAsync(B0, ldb, 0, 0,
                                           args->B, args->ldb, 0, 0,
                                           lb, args->n, sizeof(double complex), plan->compute));

        for (size_t l = 0; l < args->k; l += kb) {
          // Compute C on the same stream as the copies to ensure they have finished first
          CU_ERROR_CHECK(cuZgemm(plan->zgemm, args->transA, args->transB,
                                 args->m, args->n, min(args->k - l, kb),
                                 args->alpha, A0, lda, B0, ldb,
                                 one, C, ldc, plan->compute));

          // If there is more work to do
          if (l + kb < args->k) {
            const size_t lb = min(args->k - l - kb, kb);
            // Copy the next blocks of A and B on the opposite stream from the zgemm
            CU_ERROR_CHECK(cuMemcpyHtoD2DAsync(A1, lda, 0, 0,
                                               args->A, args->lda, l + kb, 0,
                                               lb, args->m, sizeof(double complex), plan->copy));
            CU_ERROR_CHECK(cuMemcpyHtoD2DAsync(B1, ldb, 0, 0,
                                               args->B, args->ldb, l + kb, 0,
                                               lb, args->n, sizeof(double complex), plan->copy));

            // Swap the streams and pointers so that the compute starts after the copy
            CUstream stream = plan->compute; plan->compute = plan->copy; plan->copy = stream;
            CUdeviceptr ptr = A0; A0 = A1; A1 = ptr;
            ptr = B0; B0 = B1; B1 = ptr;
          }
        }
      }
    }
    else {
      if (args->transA == CBlasNoTrans) {
        // Copy A and B onto the device asynchronously on the same stream as C
        const size_t lb = min(args->k, kb);
        CU_ERROR_CHECK(cuMemcpyHtoD2DAsync(A0, lda, 0, 0,
                                           args->A, args->lda, 0, 0,
                                           args->m, lb, sizeof(double complex), plan->compute));
        CU_ERROR_CHECK(cuMemcpyHtoD2DAsync(B0, ldb, 0, 0,
                                           args->B, args->ldb, 0, 0,
                                           args->n, lb, sizeof(double complex), plan->compute));

        for (size_t l = 0; l < args->k; l += kb) {
          // Compute C on the same stream as the copies to ensure they have finished first
          CU_ERROR_CHECK(cuZgemm(plan->zgemm, args->transA, args->transB,
                                 args->m, args->n, min(args->k - l, kb),
                                 args->alpha, A0, lda, B0, ldb,
                                 one, C, ldc, plan->compute));

          // If there is more work to do
          if (l + kb < args->k) {
            const size_t lb = min(args->k - l - kb, kb);
            // Copy the next blocks of A and B on the opposite stream from the zgemm
            CU_ERROR_CHECK(cuMemcpyHtoD2DAsync(A1, lda, 0, 0,
                                               args->A, args->lda, 0, l + kb,
                                               args->m, lb, sizeof(double complex), plan->copy));
            CU_ERROR_CHECK(cuMemcpyHtoD2DAsync(B1, ldb, 0, 0,
                                               args->B, args->ldb, 0, l + kb,
                                               args->n, lb, sizeof(double complex), plan->copy));

            // Swap the streams and pointers so that the compute starts after the copy
            CUstream stream = plan->compute; plan->compute = plan->copy; plan->copy = stream;
            CUdeviceptr ptr = A0; A0 = A1; A1 = ptr;
            ptr = B0; B0 = B1; B1 = ptr;
          }
        }
      }
      else {
        // Copy A and B onto the device asynchronously on the same stream as C
        const size_t lb = min(args->k, kb);
        CU_ERROR_CHECK(cuMemcpyHtoD2DAsync(A0, lda, 0, 0,
                                           args->A, args->lda, 0, 0,
                                           lb, args->m, sizeof(double complex), plan->compute));
        CU_ERROR_CHECK(cuMemcpyHtoD2DAsync(B0, ldb, 0, 0,
                                           args->B, args->ldb, 0, 0,
                                           args->n, lb, sizeof(double complex), plan->compute));

        for (size_t l = 0; l < args->k; l += kb) {
          // Compute C on the same stream as the copies to ensure they have finished first
          CU_ERROR_CHECK(cuZgemm(plan->zgemm, args->transA, args->transB,
                                 args->m, args->n, min(args->k - l, kb),
                                 args->alpha, A0, lda, B0, ldb,
                                 one, C, ldc, plan->compute));

          // If there is more work to do
          if (l + kb < args->k) {
            const size_t lb = min(args->k - l - kb, kb);
            // Copy the next blocks of A and B on the opposite stream from the zgemm
            CU_ERROR_CHECK(cuMemcpyHtoD2DAsync(A1, lda, 0, 0,
                                               args->A, args->lda, l + kb, 0,
                                               lb, args->m, sizeof(double complex), plan->copy));
            CU_ERROR_CHECK(cuMemcpyHtoD2DAsync(B1, ldb, 0, 0,
                                               args->B, args->ldb, 0, l + kb,
                                               args->n, lb, sizeof(double complex), plan->copy));

            // Swap the streams and pointers so that the compute starts after the copy
            CUstream stream = plan->compute; plan->compute = plan->copy; plan->copy = stream;
            CUdeviceptr ptr = A0; A0 = A1; A1 = ptr;
            ptr = B0; B0 = B1; B1 = ptr;
          }
        }
      }
    }
  }

  // Copy C back onto the host on the compute stream
  CU_ERROR_CHECK(cuMemcpyDtoH2DAsync(args->C, args->ldc, 0, 0, C, ldc, 0, 0,
                                     args->m, args->n, sizeof(double complex), plan->compute));

  return CUDA_SUCCESS;
}

CUresult cuMultiGPUZgemm(CUmultiGPUBlasHandle handle,
                         CBlasTranspose transA, CBlasTranspose transB,
                         size_t m, size_t n, size_t k,
                         double complex alpha, const double complex * restrict A, size_t lda,
                         const double complex * restrict B, size_t ldb,
                         double complex beta, double complex * restrict C, size_t ldc) {
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

  const size_t mb = (transA == CBlasNoTrans) ? ZGEMM_N_MB : ((transB == CBlasNoTrans) ? ZGEMM_CN_MB : ZGEMM_CC_MB);
  const size_t nb = (transA == CBlasNoTrans) ? ZGEMM_N_NB : ((transB == CBlasNoTrans) ? ZGEMM_CN_NB : ZGEMM_CC_NB);

  if (m < mb && n < nb) {
    zgemm(transA, transB, m, n, k, alpha, A, lda, B, ldb, beta, C, ldc);
    return CUDA_SUCCESS;
  }

  int task = 0, nTasks = (int)(((m + mb - 1) / mb) * ((n + nb - 1) / nb));
  CUtask tasks[nTasks];

  int ctx = 0;
  int nCtxs = cuMultiGPUGetContextCount(handle->mGPU);

  struct zgemm_args args = { .transA = transA, .transB = transB,
                             .k = k,
                             .alpha = alpha, .lda = lda, .ldb = ldb,
                             .beta = beta, .ldc = ldc };

  if (transB == CBlasNoTrans) {
    if (transA == CBlasNoTrans) {
      for (size_t j = 0; j < n; j += nb) {
        args.n = min(n - j, nb);
        for (size_t i = 0; i < m; i += mb) {
          args.m = min(m - i, mb);
          args.A = &A[i];
          args.B = &B[j * ldb];
          args.C = &C[j * ldc + i];
          args.plan = &handle->plans[ctx];
          CU_ERROR_CHECK(cuTaskCreate(&tasks[task], background_zgemm, &args, sizeof(struct zgemm_args)));
          CU_ERROR_CHECK(cuMultiGPURunTask(handle->mGPU, ctx++, tasks[task++]));
          if (ctx == nCtxs)
            ctx = 0;
        }
      }
    }
    else {
      for (size_t j = 0; j < n; j += nb) {
        args.n = min(n - j, nb);
        for (size_t i = 0; i < m; i += mb) {
          args.m = min(m - i, mb);
          args.A = &A[i * lda];
          args.B = &B[j * ldb];
          args.C = &C[j * ldc + i];
          args.plan = &handle->plans[ctx];
          CU_ERROR_CHECK(cuTaskCreate(&tasks[task], background_zgemm, &args, sizeof(struct zgemm_args)));
          CU_ERROR_CHECK(cuMultiGPURunTask(handle->mGPU, ctx++, tasks[task++]));
          if (ctx == nCtxs)
            ctx = 0;
        }
      }
    }
  }
  else {
    if (transA == CBlasNoTrans) {
      for (size_t j = 0; j < n; j += nb) {
        args.n = min(n - j, nb);
        for (size_t i = 0; i < m; i += mb) {
          args.m = min(m - i, mb);
          args.A = &A[i];
          args.B = &B[j];
          args.C = &C[j * ldc + i];
          args.plan = &handle->plans[ctx];
          CU_ERROR_CHECK(cuTaskCreate(&tasks[task], background_zgemm, &args, sizeof(struct zgemm_args)));
          CU_ERROR_CHECK(cuMultiGPURunTask(handle->mGPU, ctx++, tasks[task++]));
          if (ctx == nCtxs)
            ctx = 0;
        }
      }
    }
    else {
      for (size_t j = 0; j < n; j += nb) {
        args.n = min(n - j, nb);
        for (size_t i = 0; i < m; i += mb) {
          args.m = min(m - i, mb);
          args.A = &A[i * lda];
          args.B = &B[j];
          args.C = &C[j * ldc + i];
          args.plan = &handle->plans[ctx];
          CU_ERROR_CHECK(cuTaskCreate(&tasks[task], background_zgemm, &args, sizeof(struct zgemm_args)));
          CU_ERROR_CHECK(cuMultiGPURunTask(handle->mGPU, ctx++, tasks[task++]));
          if (ctx == nCtxs)
            ctx = 0;
        }
      }
    }
  }

  CUresult result;
  for (task = 0; task < nTasks; task++)
    CU_ERROR_CHECK(cuTaskDestroy(tasks[task], &result));

  return result;
}
