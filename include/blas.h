#ifndef BLAS_H
#define BLAS_H

#include <stddef.h>
#include <stdbool.h>
#include <complex.h>
#include <cuda.h>

#include "cuthread.h"

// nvcc uses __restrict__ instead of C99's restrict keyword
// CUDA Programming Guide v4.1 Appendix B.2.4
#ifdef __CUDACC__
#define restrict __restrict__
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * BLAS enums - castable to char for compatibility with Fortran BLAS/LAPACK.
 */
typedef enum { CBlasNoTrans = 'N', CBlasTrans = 'T', CBlasConjTrans = 'C' } CBlasTranspose;
typedef enum { CBlasLower = 'L', CBlasUpper = 'U' } CBlasUplo;
typedef enum { CBlasLeft = 'L', CBlasRight = 'R' } CBlasSide;
typedef enum { CBlasNonUnit = 'N', CBlasUnit = 'U' } CBlasDiag;

/**
 * Prefixes:
 *      <none>: Hand coded to run on the CPU using arguments in system memory
 *          cu: Hand coded to run on the GPU using arguments in graphics memory
 *  cuMultiGPU: Hand coded to run on the CPU and multiple GPUs using arguments in system memory
 */

/** Error function */
typedef void (*xerbla_t)(const char *, long);
extern xerbla_t xerbla;
#define XERBLA(info) \
  do { \
    if (xerbla != NULL) \
      xerbla(__func__, info); \
  } while (false)

/** My CPU implementations */
// Single precision rank-K update
void ssyrk(CBlasUplo, CBlasTranspose,
           size_t, size_t,
           float, const  float * restrict, size_t,
           float,  float * restrict, size_t);
// Double precision rank-K update
void dsyrk(CBlasUplo, CBlasTranspose,
           size_t, size_t,
           double, const double * restrict, size_t,
           double, double * restrict, size_t);

// Single precision complex hermitian rank-K update
void cherk(CBlasUplo, CBlasTranspose,
           size_t, size_t,
           float, const  float complex * restrict, size_t,
           float,  float complex * restrict, size_t);
// Double precision complex hermitian rank-K update
void zherk(CBlasUplo, CBlasTranspose,
           size_t, size_t,
           double, const double complex * restrict, size_t,
           double, double complex * restrict, size_t);

// Single precision matrix-multiply
void sgemm(CBlasTranspose, CBlasTranspose,
           size_t, size_t, size_t,
           float, const float * restrict, size_t, const float * restrict, size_t,
           float, float * restrict, size_t);
// Double precision matrix-multiply
void dgemm(CBlasTranspose, CBlasTranspose,
           size_t, size_t, size_t,
           double, const double * restrict, size_t, const double * restrict, size_t,
           double, double * restrict, size_t);
// Single precision complex matrix multiply
void cgemm(CBlasTranspose, CBlasTranspose,
           size_t, size_t, size_t,
           float complex, const float complex * restrict, size_t, const float complex * restrict, size_t,
           float complex, float complex * restrict, size_t);
// Double precision complex matrix multiply
void zgemm(CBlasTranspose, CBlasTranspose,
           size_t, size_t, size_t,
           double complex, const double complex * restrict, size_t, const double complex * restrict, size_t,
           double complex, double complex * restrict, size_t);

// In-place single precision triangular matrix multiply
#define strmm(side, uplo, trans, diag, m, n, alpha, A, lda, B, ldb) \
        strmm2(side, uplo, trans, diag, m, n, alpha, A, lda, B, ldb, B, ldb)
// In-place double precision triangular matrix multiply
#define dtrmm(side, uplo, trans, diag, m, n, alpha, A, lda, B, ldb) \
        dtrmm2(side, uplo, trans, diag, m, n, alpha, A, lda, B, ldb, B, ldb)
// In-place single precision complex triangular matrix multiply
#define ctrmm(side, uplo, trans, diag, m, n, alpha, A, lda, B, ldb) \
        ctrmm2(side, uplo, trans, diag, m, n, alpha, A, lda, B, ldb, B, ldb)
// In-place double precision complex triangular matrix multiply
#define ztrmm(side, uplo, trans, diag, m, n, alpha, A, lda, B, ldb) \
       ztrmm2(side, uplo, trans, diag, m, n, alpha, A, lda, B, ldb, B, ldb)

// Out of place single precision triangular matrix multiply
void strmm2(CBlasSide, CBlasUplo, CBlasTranspose, CBlasDiag,
            size_t, size_t,
            float, const float * restrict, size_t, const float * restrict, size_t,
            float * restrict, size_t);
// Out of place double precision triangular matrix multiply
void dtrmm2(CBlasSide, CBlasUplo, CBlasTranspose, CBlasDiag,
            size_t, size_t,
            double, const double * restrict, size_t, const double * restrict, size_t,
            double * restrict, size_t);
// Out of place single precision complex triangular matrix multiply
void ctrmm2(CBlasSide, CBlasUplo, CBlasTranspose, CBlasDiag,
            size_t, size_t,
            float complex, const float complex * restrict, size_t, const float complex * restrict, size_t,
            float complex * restrict, size_t);
// Out of place double precision complex triangular matrix multiply
void ztrmm2(CBlasSide, CBlasUplo, CBlasTranspose, CBlasDiag,
            size_t, size_t,
            double complex, const double complex * restrict, size_t, const double complex * restrict, size_t,
            double complex * restrict, size_t);

// Single precision triangular solve
void strsm(CBlasSide, CBlasUplo, CBlasTranspose, CBlasDiag,
           size_t, size_t,
           float, const float * restrict, size_t,
           float * restrict, size_t);
// Double precision triangular solve
void dtrsm(CBlasSide, CBlasUplo, CBlasTranspose, CBlasDiag,
           size_t, size_t,
           double, const double * restrict, size_t,
           double * restrict, size_t);
// Single precision complex triangular solve
void ctrsm(CBlasSide, CBlasUplo, CBlasTranspose, CBlasDiag,
           size_t, size_t,
           float complex, const float complex * restrict, size_t,
           float complex * restrict, size_t);
// Double precision complex triangular solve
void ztrsm(CBlasSide, CBlasUplo, CBlasTranspose, CBlasDiag,
           size_t, size_t,
           double complex, const double complex * restrict, size_t,
           double complex * restrict, size_t);

/** My GPU implementations */
// Single precision rank-K update
CUresult cuSsyrk(CUmodule, CBlasUplo, CBlasTranspose,
                 size_t, size_t,
                 float, CUdeviceptr, size_t,
                 float, CUdeviceptr, size_t, CUstream);
// Double precision rank-K update
CUresult cuDsyrk(CUmodule, CBlasUplo, CBlasTranspose,
                 size_t, size_t,
                 double, CUdeviceptr, size_t,
                 double, CUdeviceptr, size_t, CUstream);

// Single precision complex hermitian rank-K update
CUresult cuCherk(CUmodule, CBlasUplo, CBlasTranspose,
                 size_t, size_t,
                 float, CUdeviceptr, size_t,
                 float, CUdeviceptr, size_t, CUstream);
// Double precision complex hermitian rank-K update
CUresult cuZherk(CUmodule, CBlasUplo, CBlasTranspose,
                 size_t, size_t,
                 double, CUdeviceptr, size_t,
                 double, CUdeviceptr, size_t, CUstream);

// In-place single precision matrix multiply
#define cuSgemm(module, transA, transB, m, n, k, alpha, A, lda, B, ldb, beta, C, ldc, stream) \
        cuSgemm2(module, transA, transB, m, n, k, alpha, A, lda, B, ldb, beta, C, ldc, C, ldc, stream)
// In-place double precision matrix multiply
#define cuDgemm(module, transA, transB, m, n, k, alpha, A, lda, B, ldb, beta, C, ldc, stream) \
        cuDgemm2(module, transA, transB, m, n, k, alpha, A, lda, B, ldb, beta, C, ldc, C, ldc, stream)
// In-place single precision complex matrix multiply
#define cuCgemm(module, transA, transB, m, n, k, alpha, A, lda, B, ldb, beta, C, ldc, stream) \
        cuCgemm2(module, transA, transB, m, n, k, alpha, A, lda, B, ldb, beta, C, ldc, C, ldc, stream)
// In-place double precision complex matrix multiply
#define cuZgemm(module, transA, transB, m, n, k, alpha, A, lda, B, ldb, beta, C, ldc, stream) \
        cuZgemm2(module, transA, transB, m, n, k, alpha, A, lda, B, ldb, beta, C, ldc, C, ldc, stream)

// Out of place single precision matrix multiply
CUresult cuSgemm2(CUmodule, CBlasTranspose, CBlasTranspose,
                  size_t, size_t, size_t,
                  float, CUdeviceptr, size_t, CUdeviceptr, size_t,
                  float, CUdeviceptr, size_t, CUdeviceptr, size_t, CUstream);
// Out of place double precision matrix multiply
CUresult cuDgemm2(CUmodule, CBlasTranspose, CBlasTranspose,
                  size_t, size_t, size_t,
                  double, CUdeviceptr, size_t, CUdeviceptr, size_t,
                  double, CUdeviceptr, size_t, CUdeviceptr, size_t, CUstream);
// Out of place single precision complex matrix multiply
CUresult cuCgemm2(CUmodule, CBlasTranspose, CBlasTranspose,
                  size_t, size_t, size_t,
                  float complex, CUdeviceptr, size_t, CUdeviceptr, size_t,
                  float complex, CUdeviceptr, size_t, CUdeviceptr, size_t, CUstream);
// Out of place double precision complex matrix multiply
CUresult cuZgemm2(CUmodule, CBlasTranspose, CBlasTranspose,
                  size_t, size_t, size_t,
                  double complex, CUdeviceptr, size_t, CUdeviceptr, size_t,
                  double complex, CUdeviceptr, size_t, CUdeviceptr, size_t, CUstream);

// Out of place single precision triangular matrix multiply
CUresult cuStrmm2(CUmodule, CBlasSide, CBlasUplo, CBlasTranspose, CBlasDiag,
                  size_t, size_t,
                  float, CUdeviceptr, size_t, CUdeviceptr, size_t,
                  CUdeviceptr, size_t, CUstream);
// Out of place double precision triangular matrix multiply
CUresult cuDtrmm2(CUmodule, CBlasSide, CBlasUplo, CBlasTranspose, CBlasDiag,
                  size_t, size_t,
                  double, CUdeviceptr, size_t, CUdeviceptr, size_t,
                  CUdeviceptr, size_t, CUstream);
// Out of place single precision complex triangular matrix multiply
CUresult cuCtrmm2(CUmodule, CBlasSide, CBlasUplo, CBlasTranspose, CBlasDiag,
                  size_t, size_t,
                  float complex, CUdeviceptr, size_t, CUdeviceptr, size_t,
                  CUdeviceptr, size_t, CUstream);
// Out of place double precision complex triangular matrix multiply
CUresult cuZtrmm2(CUmodule, CBlasSide, CBlasUplo, CBlasTranspose, CBlasDiag,
                  size_t, size_t,
                  double complex, CUdeviceptr, size_t, CUdeviceptr, size_t,
                  CUdeviceptr, size_t, CUstream);

// Single precision triangular matrix solve
CUresult cuStrsm(CUmodule, CBlasSide, CBlasUplo, CBlasTranspose, CBlasDiag,
                 size_t, size_t,
                 float, CUdeviceptr, size_t,
                 CUdeviceptr, size_t, CUstream);
// Double precision triangular matrix solve
CUresult cuDtrsm(CUmodule, CBlasSide, CBlasUplo, CBlasTranspose, CBlasDiag,
                 size_t, size_t,
                 double, CUdeviceptr, size_t,
                 CUdeviceptr, size_t, CUstream);
// Single precision complex triangular matrix solve
CUresult cuCtrsm(CUmodule, CBlasSide, CBlasUplo, CBlasTranspose, CBlasDiag,
                 size_t, size_t,
                 float complex, CUdeviceptr, size_t,
                 CUdeviceptr, size_t, CUstream);
// Double precision complex triangular matrix solve
CUresult cuZtrsm(CUmodule, CBlasSide, CBlasUplo, CBlasTranspose, CBlasDiag,
                 size_t, size_t,
                 double complex, CUdeviceptr, size_t,
                 CUdeviceptr, size_t, CUstream);

/** My MultiGPU/Hybrid implementations */
// Single precision rank-K update
CUresult cuMultiGPUSsyrk(CUthread *, int,
                         CBlasUplo, CBlasTranspose,
                         size_t, size_t,
                         float, const float * restrict, size_t,
                         float, float * restrict, size_t);
// Double precision rank-K update
CUresult cuMultiGPUDsyrk(CUthread *, int,
                         CBlasUplo, CBlasTranspose,
                         size_t, size_t,
                         double, const double * restrict, size_t,
                         double, double * restrict, size_t);

// Single precision complex hermitian rank-K update
CUresult cuMultiGPUCherk(CUthread *, int,
                         CBlasUplo, CBlasTranspose,
                         size_t, size_t,
                         float, const float complex * restrict, size_t,
                         float, float complex * restrict, size_t);
// Double precision complex hermitian rank-K update
CUresult cuMultiGPUZherk(CUthread *, int,
                         CBlasUplo, CBlasTranspose,
                         size_t, size_t,
                         double, const double complex * restrict, size_t,
                         double, double complex * restrict, size_t);

// Single precision matrix multiply
CUresult cuMultiGPUSgemm(CUthread *, int,
                         CBlasTranspose, CBlasTranspose,
                         size_t, size_t, size_t,
                         float, const float * restrict, size_t, const float * restrict, size_t,
                         float, float * restrict, size_t);
// Double precision matrix multiply
CUresult cuMultiGPUDgemm(CUthread *, int,
                         CBlasTranspose, CBlasTranspose,
                         size_t, size_t, size_t,
                         double, const double * restrict, size_t, const double * restrict, size_t,
                         double, double * restrict, size_t);
// Single precision complex matrix multiply
CUresult cuMultiGPUCgemm(CUthread *, int,
                         CBlasTranspose, CBlasTranspose,
                         size_t, size_t, size_t,
                         float complex, const float complex * restrict, size_t, const float complex * restrict, size_t,
                         float complex,  float complex * restrict, size_t);
// Double precision complex matrix multiply
CUresult cuMultiGPUZgemm(CUthread *, int,
                         CBlasTranspose, CBlasTranspose,
                         size_t, size_t, size_t,
                         double complex, const double complex * restrict, size_t, const double complex * restrict, size_t,
                         double complex, double complex * restrict, size_t);

// Single precision triangular matrix multiply
CUresult cuMultiGPUStrmm(CUthread *, int,
                         CBlasSide, CBlasUplo, CBlasTranspose, CBlasDiag,
                         size_t, size_t,
                         float, const float * restrict, size_t,
                         float * restrict, size_t);
// Double precision triangular matrix multiply
CUresult cuMultiGPUDtrmm(CUthread *, int,
                         CBlasSide, CBlasUplo, CBlasTranspose, CBlasDiag,
                         size_t, size_t,
                         double, const double * restrict, size_t,
                         double * restrict, size_t);
// Single precision complex triangular matrix multiply
CUresult cuMultiGPUCtrmm(CUthread *, int,
                         CBlasSide, CBlasUplo, CBlasTranspose, CBlasDiag,
                         size_t, size_t,
                         float complex, const float complex * restrict, size_t,
                         float complex * restrict, size_t);
// Double precision complex triangular matrix multiply
CUresult cuMultiGPUZtrmm(CUthread *, int,
                         CBlasSide, CBlasUplo, CBlasTranspose, CBlasDiag,
                         size_t, size_t,
                         double complex, const double complex * restrict, size_t,
                         double complex * restrict, size_t);

// Single precision triangular solve
CUresult cuMultiGPUStrsm(CUthread *, int,
                         CBlasSide, CBlasUplo, CBlasTranspose, CBlasDiag,
                         size_t, size_t,
                         float, const float * restrict, size_t,
                         float * restrict, size_t);
// Double precision triangular solve
CUresult cuMultiGPUDtrsm(CUthread *, int,
                         CBlasSide, CBlasUplo, CBlasTranspose, CBlasDiag,
                         size_t, size_t,
                         double, const double * restrict, size_t,
                         double * restrict, size_t);
// Single precision complex triangular solve
CUresult cuMultiGPUCtrsm(CUthread *, int,
                         CBlasSide, CBlasUplo, CBlasTranspose, CBlasDiag,
                         size_t, size_t,
                         float complex, const float complex * restrict, size_t,
                         float complex * restrict, size_t);
// Double precision complex triangular solve
CUresult cuMultiGPUZtrsm(CUthread *, int,
                         CBlasSide, CBlasUplo, CBlasTranspose, CBlasDiag,
                         size_t, size_t,
                         double complex, const double complex * restrict, size_t,
                         double complex * restrict, size_t);

#ifdef __CUDACC__
#undef restrict
#endif

#ifdef __cplusplus
}
#endif

#endif
