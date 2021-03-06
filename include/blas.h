#ifndef BLAS_H
#define BLAS_H

#include <stddef.h>
#include <stdbool.h>
#include <complex.h>
#include <cuda.h>

#include "cumultigpu.h"

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
void strmm(CBlasSide, CBlasUplo, CBlasTranspose, CBlasDiag,
           size_t, size_t,
           float, const float * restrict, size_t,
           float * restrict, size_t);
// In-place double precision triangular matrix multiply
void dtrmm(CBlasSide, CBlasUplo, CBlasTranspose, CBlasDiag,
           size_t, size_t,
           double, const double * restrict, size_t,
           double * restrict, size_t);
// In-place single precision complex triangular matrix multiply
void ctrmm(CBlasSide, CBlasUplo, CBlasTranspose, CBlasDiag,
           size_t, size_t,
           float complex, const float complex * restrict, size_t,
           float complex * restrict, size_t);
// In-place double precision complex triangular matrix multiply
void ztrmm(CBlasSide, CBlasUplo, CBlasTranspose, CBlasDiag,
           size_t, size_t,
           double complex, const double complex * restrict, size_t,
           double complex * restrict, size_t);

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
typedef struct __cublashandle_st * CUBLAShandle;
CUresult cuBLASCreate(CUBLAShandle *);
CUresult cuBLASDestroy(CUBLAShandle);

// Single precision rank-K update
CUresult cuSsyrk(CUBLAShandle, CBlasUplo, CBlasTranspose,
                 size_t, size_t,
                 float, CUdeviceptr, size_t,
                 float, CUdeviceptr, size_t, CUstream);
// Double precision rank-K update
CUresult cuDsyrk(CUBLAShandle, CBlasUplo, CBlasTranspose,
                 size_t, size_t,
                 double, CUdeviceptr, size_t,
                 double, CUdeviceptr, size_t, CUstream);

// Single precision complex hermitian rank-K update
CUresult cuCherk(CUBLAShandle, CBlasUplo, CBlasTranspose,
                 size_t, size_t,
                 float, CUdeviceptr, size_t,
                 float, CUdeviceptr, size_t, CUstream);
// Double precision complex hermitian rank-K update
CUresult cuZherk(CUBLAShandle, CBlasUplo, CBlasTranspose,
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
CUresult cuSgemm2(CUBLAShandle, CBlasTranspose, CBlasTranspose,
                  size_t, size_t, size_t,
                  float, CUdeviceptr, size_t, CUdeviceptr, size_t,
                  float, CUdeviceptr, size_t, CUdeviceptr, size_t, CUstream);
// Out of place double precision matrix multiply
CUresult cuDgemm2(CUBLAShandle, CBlasTranspose, CBlasTranspose,
                  size_t, size_t, size_t,
                  double, CUdeviceptr, size_t, CUdeviceptr, size_t,
                  double, CUdeviceptr, size_t, CUdeviceptr, size_t, CUstream);
// Out of place single precision complex matrix multiply
CUresult cuCgemm2(CUBLAShandle, CBlasTranspose, CBlasTranspose,
                  size_t, size_t, size_t,
                  float complex, CUdeviceptr, size_t, CUdeviceptr, size_t,
                  float complex, CUdeviceptr, size_t, CUdeviceptr, size_t, CUstream);
// Out of place double precision complex matrix multiply
CUresult cuZgemm2(CUBLAShandle, CBlasTranspose, CBlasTranspose,
                  size_t, size_t, size_t,
                  double complex, CUdeviceptr, size_t, CUdeviceptr, size_t,
                  double complex, CUdeviceptr, size_t, CUdeviceptr, size_t, CUstream);

// Out of place single precision triangular matrix multiply
CUresult cuStrmm2(CUBLAShandle, CBlasSide, CBlasUplo, CBlasTranspose, CBlasDiag,
                  size_t, size_t,
                  float, CUdeviceptr, size_t, CUdeviceptr, size_t,
                  CUdeviceptr, size_t, CUstream);
// Out of place double precision triangular matrix multiply
CUresult cuDtrmm2(CUBLAShandle, CBlasSide, CBlasUplo, CBlasTranspose, CBlasDiag,
                  size_t, size_t,
                  double, CUdeviceptr, size_t, CUdeviceptr, size_t,
                  CUdeviceptr, size_t, CUstream);
// Out of place single precision complex triangular matrix multiply
CUresult cuCtrmm2(CUBLAShandle, CBlasSide, CBlasUplo, CBlasTranspose, CBlasDiag,
                  size_t, size_t,
                  float complex, CUdeviceptr, size_t, CUdeviceptr, size_t,
                  CUdeviceptr, size_t, CUstream);
// Out of place double precision complex triangular matrix multiply
CUresult cuZtrmm2(CUBLAShandle, CBlasSide, CBlasUplo, CBlasTranspose, CBlasDiag,
                  size_t, size_t,
                  double complex, CUdeviceptr, size_t, CUdeviceptr, size_t,
                  CUdeviceptr, size_t, CUstream);

// Single precision triangular matrix solve
CUresult cuStrsm(CUBLAShandle, CBlasSide, CBlasUplo, CBlasTranspose, CBlasDiag,
                 size_t, size_t,
                 float, CUdeviceptr, size_t,
                 CUdeviceptr, size_t, CUstream);
// Double precision triangular matrix solve
CUresult cuDtrsm(CUBLAShandle, CBlasSide, CBlasUplo, CBlasTranspose, CBlasDiag,
                 size_t, size_t,
                 double, CUdeviceptr, size_t,
                 CUdeviceptr, size_t, CUstream);
// Single precision complex triangular matrix solve
CUresult cuCtrsm(CUBLAShandle, CBlasSide, CBlasUplo, CBlasTranspose, CBlasDiag,
                 size_t, size_t,
                 float complex, CUdeviceptr, size_t,
                 CUdeviceptr, size_t, CUstream);
// Double precision complex triangular matrix solve
CUresult cuZtrsm(CUBLAShandle, CBlasSide, CBlasUplo, CBlasTranspose, CBlasDiag,
                 size_t, size_t,
                 double complex, CUdeviceptr, size_t,
                 CUdeviceptr, size_t, CUstream);

/** My MultiGPU/Hybrid implementations */
// MultiGPU handle
typedef struct __cumultigpublashandle_st * CUmultiGPUBLAShandle;
CUresult cuMultiGPUBLASCreate(CUmultiGPUBLAShandle *, CUmultiGPU);
CUresult cuMultiGPUBLASDestroy(CUmultiGPUBLAShandle);
CUresult cuMultiGPUBLASSynchronize(CUmultiGPUBLAShandle);

// Single precision rank-K update
CUresult cuMultiGPUSsyrk(CUmultiGPUBLAShandle,
                         CBlasUplo, CBlasTranspose,
                         size_t, size_t,
                         float, const float * restrict, size_t,
                         float, float * restrict, size_t);
// Double precision rank-K update
CUresult cuMultiGPUDsyrk(CUmultiGPUBLAShandle,
                         CBlasUplo, CBlasTranspose,
                         size_t, size_t,
                         double, const double * restrict, size_t,
                         double, double * restrict, size_t);
// Single precision complex hermitian rank-K update
CUresult cuMultiGPUCherk(CUmultiGPUBLAShandle,
                         CBlasUplo, CBlasTranspose,
                         size_t, size_t,
                         float, const float complex * restrict, size_t,
                         float, float complex * restrict, size_t);
// Double precision complex hermitian rank-K update
CUresult cuMultiGPUZherk(CUmultiGPUBLAShandle,
                         CBlasUplo, CBlasTranspose,
                         size_t, size_t,
                         double, const double complex * restrict, size_t,
                         double, double complex * restrict, size_t);

// Single precision matrix multiply
CUresult cuMultiGPUSgemm(CUmultiGPUBLAShandle,
                         CBlasTranspose, CBlasTranspose,
                         size_t, size_t, size_t,
                         float, const float * restrict, size_t, const float * restrict, size_t,
                         float, float * restrict, size_t);
// Double precision matrix multiply
CUresult cuMultiGPUDgemm(CUmultiGPUBLAShandle,
                         CBlasTranspose, CBlasTranspose,
                         size_t, size_t, size_t,
                         double, const double * restrict, size_t, const double * restrict, size_t,
                         double, double * restrict, size_t);
// Single precision complex matrix multiply
CUresult cuMultiGPUCgemm(CUmultiGPUBLAShandle,
                         CBlasTranspose, CBlasTranspose,
                         size_t, size_t, size_t,
                         float complex, const float complex * restrict, size_t, const float complex * restrict, size_t,
                         float complex,  float complex * restrict, size_t);
// Double precision complex matrix multiply
CUresult cuMultiGPUZgemm(CUmultiGPUBLAShandle,
                         CBlasTranspose, CBlasTranspose,
                         size_t, size_t, size_t,
                         double complex, const double complex * restrict, size_t, const double complex * restrict, size_t,
                         double complex, double complex * restrict, size_t);

// Single precision triangular matrix multiply
CUresult cuMultiGPUStrmm(CUmultiGPUBLAShandle,
                         CBlasSide, CBlasUplo, CBlasTranspose, CBlasDiag,
                         size_t, size_t,
                         float, const float * restrict, size_t,
                         float * restrict, size_t);
// Double precision triangular matrix multiply
CUresult cuMultiGPUDtrmm(CUmultiGPUBLAShandle,
                         CBlasSide, CBlasUplo, CBlasTranspose, CBlasDiag,
                         size_t, size_t,
                         double, const double * restrict, size_t,
                         double * restrict, size_t);
// Single precision complex triangular matrix multiply
CUresult cuMultiGPUCtrmm(CUmultiGPUBLAShandle,
                         CBlasSide, CBlasUplo, CBlasTranspose, CBlasDiag,
                         size_t, size_t,
                         float complex, const float complex * restrict, size_t,
                         float complex * restrict, size_t);
// Double precision complex triangular matrix multiply
CUresult cuMultiGPUZtrmm(CUmultiGPUBLAShandle,
                         CBlasSide, CBlasUplo, CBlasTranspose, CBlasDiag,
                         size_t, size_t,
                         double complex, const double complex * restrict, size_t,
                         double complex * restrict, size_t);

// Single precision triangular solve
CUresult cuMultiGPUStrsm(CUmultiGPUBLAShandle,
                         CBlasSide, CBlasUplo, CBlasTranspose, CBlasDiag,
                         size_t, size_t,
                         float, const float * restrict, size_t,
                         float * restrict, size_t);
// Double precision triangular solve
CUresult cuMultiGPUDtrsm(CUmultiGPUBLAShandle,
                         CBlasSide, CBlasUplo, CBlasTranspose, CBlasDiag,
                         size_t, size_t,
                         double, const double * restrict, size_t,
                         double * restrict, size_t);
// Single precision complex triangular solve
CUresult cuMultiGPUCtrsm(CUmultiGPUBLAShandle,
                         CBlasSide, CBlasUplo, CBlasTranspose, CBlasDiag,
                         size_t, size_t,
                         float complex, const float complex * restrict, size_t,
                         float complex * restrict, size_t);
// Double precision complex triangular solve
CUresult cuMultiGPUZtrsm(CUmultiGPUBLAShandle,
                         CBlasSide, CBlasUplo, CBlasTranspose, CBlasDiag,
                         size_t, size_t,
                         double complex, const double complex * restrict, size_t,
                         double complex * restrict, size_t);

#ifdef __cplusplus
}
#endif

#ifdef __CUDACC__
#undef restrict
#endif

#endif
