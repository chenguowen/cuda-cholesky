#include "blas.h"

// y(1:16) += alpha * x(1:16)
__device__ void daxpy(double alpha, const double * x, double * y) {
  y[ 0] += alpha * x[ 0]; y[ 1] += alpha * x[ 1]; y[ 2] += alpha * x[ 2]; y[ 3] += alpha * x[ 3];
  y[ 4] += alpha * x[ 4]; y[ 5] += alpha * x[ 5]; y[ 6] += alpha * x[ 6]; y[ 7] += alpha * x[ 7];
  y[ 8] += alpha * x[ 8]; y[ 9] += alpha * x[ 9]; y[10] += alpha * x[10]; y[11] += alpha * x[11];
  y[12] += alpha * x[12]; y[13] += alpha * x[13]; y[14] += alpha * x[14]; y[15] += alpha * x[15];
}

// y(1:n) += alpha * x(1:n)
__device__ void daxpy(int n, double alpha, const double * x, double * y) {
  y[ 0] += alpha * x[ 0]; if ( 1 >= n) return; y[ 1] += alpha * x[ 1]; if ( 2 >= n) return;
  y[ 2] += alpha * x[ 2]; if ( 3 >= n) return; y[ 3] += alpha * x[ 3]; if ( 4 >= n) return;
  y[ 4] += alpha * x[ 4]; if ( 5 >= n) return; y[ 5] += alpha * x[ 5]; if ( 6 >= n) return;
  y[ 6] += alpha * x[ 6]; if ( 7 >= n) return; y[ 7] += alpha * x[ 7]; if ( 8 >= n) return;
  y[ 8] += alpha * x[ 8]; if ( 9 >= n) return; y[ 9] += alpha * x[ 9]; if (10 >= n) return;
  y[10] += alpha * x[10]; if (11 >= n) return; y[11] += alpha * x[11]; if (12 >= n) return;
  y[12] += alpha * x[12]; if (13 >= n) return; y[13] += alpha * x[13]; if (14 >= n) return;
  y[14] += alpha * x[14]; if (15 >= n) return; y[15] += alpha * x[15];
}

template <CBlasUplo uplo, CBlasTranspose trans, CBlasDiag diag,
          unsigned int mb, unsigned int nb, unsigned int kb,
          unsigned int bx, unsigned int by>
__global__ void dtrmm2L(int m, int n,
                        double alpha, const double * __restrict__ A, int lda, const double * __restrict__ B, int ldb,
                        double * __restrict__ X, int ldx) {

  const int bi = blockIdx.x * mb;       // Starting row of block of X
  const int bj = blockIdx.y * nb;       // Starting column of block of X
  int ti = threadIdx.y * bx + threadIdx.x;
  int tj = 0;
  if (trans != CBlasNoTrans) {
    tj = 16 * (ti / mb);
    ti = ti % mb;
  }

  if (trans == CBlasNoTrans) {
    A += (uplo == CBlasUpper) ? bi * lda + bi + ti : bi + ti;
    B += (uplo == CBlasUpper) ? (bj + threadIdx.y) * ldb + bi + threadIdx.x
                              : (bj + threadIdx.y) * ldb + threadIdx.x;
  }
  else {
    A += (uplo == CBlasUpper) ? (bi + threadIdx.y) * lda + threadIdx.x
                              : (bi + threadIdx.y) * lda + bi + threadIdx.x;
    B += (uplo == CBlasUpper) ? (bj + threadIdx.y) * ldb + threadIdx.x
                              : (bj + threadIdx.y) * ldb + bi + threadIdx.x;
  }
  X += (bj + tj) * ldx + bi + ti;

  __shared__ double a[mb][kb + 1];
  __shared__ double b[kb][nb];

  double x[] = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
                 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };

  // For Upper/NoTrans and Lower/Trans process diagonal first
  if (uplo == CBlasUpper && trans == CBlasNoTrans ||
      uplo == CBlasLower && trans != CBlasNoTrans) {
    int k = min(m - bi, mb);
    int l = 0;
    while (k > 0) {
      if (trans != CBlasNoTrans) {
#pragma unroll
        for (int i = 0; i < mb; i += by)
          a[i + threadIdx.y][threadIdx.x] = A[i * lda];
        A += kb;
      }

#pragma unroll
      for (int j = 0; j < nb; j += by)
        b[threadIdx.x][j + threadIdx.y] = B[j * ldb];

      __syncthreads();

      if (k < kb) break;

      if (diag == CBlasNonUnit) {
#pragma unroll
        for (int ll = 0; ll < kb; ll++) {
          if (ti <= l++)
            daxpy((trans == CBlasNoTrans) ? A[0]  :  a[ti][ll],
                  (trans == CBlasNoTrans) ? b[ll] : &b[ll][tj], x);
          if (trans == CBlasNoTrans)
            A += lda;
        }
      }
      else {
#pragma unroll
        for (int ll = 0; ll < kb; ll++) {
          if (ti == l)
            daxpy(1.0, (trans == CBlasNoTrans) ? b[ll] : &b[ll][tj], x);
          else if (ti < l)
            daxpy((trans == CBlasNoTrans) ? A[0]  :  a[ti][ll],
                  (trans == CBlasNoTrans) ? b[ll] : &b[ll][tj], x);
          if (trans == CBlasNoTrans)
            A += lda;
          l++;
        }
      }

      __syncthreads();

      B += kb;
      k -= kb;
    }

    if (diag == CBlasNonUnit) {
      for (int ll = 0; ll < k; ll++) {
        if (ti <= l++)
          daxpy((trans == CBlasNoTrans) ? A[0]  :  a[ti][ll],
                (trans == CBlasNoTrans) ? b[ll] : &b[ll][tj], x);
        if (trans == CBlasNoTrans)
          A += lda;
      }
    }
    else {
      for (int ll = 0; ll < k; ll++) {
        if (ti == l)
          daxpy(1.0, (trans == CBlasNoTrans) ? b[ll] : &b[ll][tj], x);
        else if (ti < l)
          daxpy((trans == CBlasNoTrans) ? A[0]  :  a[ti][ll],
                (trans == CBlasNoTrans) ? b[ll] : &b[ll][tj], x);
        if (trans == CBlasNoTrans)
          A += lda;
        l++;
      }
    }

    __syncthreads();
  }

  // Process non-diagonal blocks as for DGEMM
  int k = (trans == CBlasNoTrans) ? ((uplo == CBlasUpper) ? m - bi - mb : bi)
                                  : ((uplo == CBlasUpper) ? bi : m - bi - mb);
  while (k > 0) {
    if (trans != CBlasNoTrans) {
#pragma unroll
      for (int i = 0; i < mb; i += by)
        a[i + threadIdx.y][threadIdx.x] = A[i * lda];
      A += kb;
    }

#pragma unroll
    for (int j = 0; j < nb; j += by)
      b[threadIdx.x][j + threadIdx.y] = B[j * ldb];

    __syncthreads();

    if (k < kb) break;

    if (trans == CBlasNoTrans) {
#pragma unroll
      for (int l = 0; l < kb; l++) {
        daxpy(A[0], b[l], x);
        A += lda;
      }
    }
    else {
#pragma unroll
      for (int l = 0; l < kb; l++)
        daxpy(a[ti][l], &b[l][tj], x);
    }

    __syncthreads();

    B += kb;
    k -= kb;
  }

  if (trans == CBlasNoTrans) {
    for (int l = 0; l < k; l++) {
      daxpy(A[0], b[l], x);
      A += lda;
    }
  }
  else {
    for (int l = 0; l < k; l++)
      daxpy(a[ti][l], &b[l][tj], x);
  }

  // For Upper/Trans and Lower/NoTrans process diagonal last
  if (uplo == CBlasUpper && trans != CBlasNoTrans ||
      uplo == CBlasLower && trans == CBlasNoTrans) {

    __syncthreads();

    int k = min(m - bi, mb);
    int l = 0;
    while (k > 0) {
      if (trans != CBlasNoTrans) {
#pragma unroll
        for (int i = 0; i < mb; i += by)
          a[i + threadIdx.y][threadIdx.x] = A[i * lda];
        A += kb;
      }

#pragma unroll
      for (int j = 0; j < nb; j += by)
        b[threadIdx.x][j + threadIdx.y] = B[j * ldb];

      __syncthreads();

      if (k < kb) break;

      if (diag == CBlasNonUnit) {
#pragma unroll
        for (int ll = 0; ll < kb; ll++) {
          if (ti >= l++)
            daxpy((trans == CBlasNoTrans) ? A[0]  :  a[ti][ll],
                  (trans == CBlasNoTrans) ? b[ll] : &b[ll][tj], x);
          if (trans == CBlasNoTrans)
            A += lda;
        }
      }
      else {
#pragma unroll
        for (int ll = 0; ll < kb; ll++) {
          if (ti == l)
            daxpy(1.0, (trans == CBlasNoTrans) ? b[ll] : &b[ll][tj], x);
          else if (ti > l)
            daxpy((trans == CBlasNoTrans) ? A[0]  :  a[ti][ll],
                  (trans == CBlasNoTrans) ? b[ll] : &b[ll][tj], x);
          if (trans == CBlasNoTrans)
            A += lda;
          l++;
        }
      }

      __syncthreads();

      B += kb;
      k -= kb;
    }

    if (diag == CBlasNonUnit) {
      for (int ll = 0; ll < k; ll++) {
        if (ti >= l++)
          daxpy((trans == CBlasNoTrans) ? A[0]  :  a[ti][ll],
                (trans == CBlasNoTrans) ? b[ll] : &b[ll][tj], x);
        if (trans == CBlasNoTrans)
          A += lda;
      }
    }
    else {
      for (int ll = 0; ll < k; ll++) {
        if (ti == l)
          daxpy(1.0, (trans == CBlasNoTrans) ? b[ll] : &b[ll][tj], x);
        else if (ti > l)
          daxpy((trans == CBlasNoTrans) ? A[0]  :  a[ti][ll],
                (trans == CBlasNoTrans) ? b[ll] : &b[ll][tj], x);
        if (trans == CBlasNoTrans)
          A += lda;
        l++;
      }
    }
  }

  n -= bj + tj;
  m -= bi + ti;
  if (n <= 0 || m <= 0) return;
  X[0] = alpha * x[ 0]; if ( 1 >= n) return; X += ldx;
  X[0] = alpha * x[ 1]; if ( 2 >= n) return; X += ldx;
  X[0] = alpha * x[ 2]; if ( 3 >= n) return; X += ldx;
  X[0] = alpha * x[ 3]; if ( 4 >= n) return; X += ldx;
  X[0] = alpha * x[ 4]; if ( 5 >= n) return; X += ldx;
  X[0] = alpha * x[ 5]; if ( 6 >= n) return; X += ldx;
  X[0] = alpha * x[ 6]; if ( 7 >= n) return; X += ldx;
  X[0] = alpha * x[ 7]; if ( 8 >= n) return; X += ldx;
  X[0] = alpha * x[ 8]; if ( 9 >= n) return; X += ldx;
  X[0] = alpha * x[ 9]; if (10 >= n) return; X += ldx;
  X[0] = alpha * x[10]; if (11 >= n) return; X += ldx;
  X[0] = alpha * x[11]; if (12 >= n) return; X += ldx;
  X[0] = alpha * x[12]; if (13 >= n) return; X += ldx;
  X[0] = alpha * x[13]; if (14 >= n) return; X += ldx;
  X[0] = alpha * x[14]; if (15 >= n) return; X += ldx;
  X[0] = alpha * x[15];
}

template <CBlasUplo uplo, CBlasTranspose trans, CBlasDiag diag,
          unsigned int mb, unsigned int nb, unsigned int kb,
          unsigned int bx, unsigned int by>
__global__ void dtrmm2R(int m, int n,
                        double alpha, const double * __restrict__ A, int lda, const double * __restrict__ B, int ldb,
                        double * __restrict__ X, int ldx) {

  const int bi = blockIdx.x * mb;       // Starting row of block of X
  const int bj = blockIdx.y * nb;       // Starting column of block of X
  int ti = threadIdx.y * bx + threadIdx.x;

  if (trans == CBlasNoTrans) {
    A += (uplo == CBlasUpper) ? (bj + threadIdx.y) * lda + threadIdx.x
                              : (bj + threadIdx.y) * lda + bj + threadIdx.x;
    B += (uplo == CBlasUpper) ? bi + ti : bj * ldb + bi + ti;
  }
  else {
    A += (uplo == CBlasUpper) ? (bj + threadIdx.y) * lda + bj + threadIdx.x
                              : threadIdx.y * lda + bj + threadIdx.x;
    B += (uplo == CBlasUpper) ? bj * ldb + bi + ti : bi + ti;
  }
  X += bj * ldx + bi + ti;

  __shared__ double a[kb][nb];

  double x[] = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0,
                 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };

  // For Upper/Trans and Lower/NoTrans process diagonal first
  if (uplo == CBlasUpper && trans != CBlasNoTrans ||
      uplo == CBlasLower && trans == CBlasNoTrans) {
    int k = min(n - bj, nb);
    while (k > 0) {
      if (trans == CBlasNoTrans) {
#pragma unroll
        for (int j = 0; j < nb; j += by)
          a[threadIdx.x][j + threadIdx.y] =
            (diag != CBlasNonUnit && threadIdx.x == j + threadIdx.y) ? 1.0 : A[j * lda];
      }
      else {
#pragma unroll
        for (int l = 0; l < kb; l += by)
          a[l + threadIdx.y][threadIdx.x] =
            (diag != CBlasNonUnit && threadIdx.x == l + threadIdx.y) ? 1.0 : A[l * lda];
      }

      __syncthreads();

      if (k < kb) break;

// #pragma unroll
//       for (int ll = 0; ll < kb; ll++) {
//         daxpy(ll + 1, B[0], a[ll], x);
        daxpy( 1, B[0], a[ 0], x); B += ldb;
        daxpy( 2, B[0], a[ 1], x); B += ldb;
        daxpy( 3, B[0], a[ 2], x); B += ldb;
        daxpy( 4, B[0], a[ 3], x); B += ldb;
        daxpy( 5, B[0], a[ 4], x); B += ldb;
        daxpy( 6, B[0], a[ 5], x); B += ldb;
        daxpy( 7, B[0], a[ 6], x); B += ldb;
        daxpy( 8, B[0], a[ 7], x); B += ldb;
        daxpy( 9, B[0], a[ 8], x); B += ldb;
        daxpy(10, B[0], a[ 9], x); B += ldb;
        daxpy(11, B[0], a[10], x); B += ldb;
        daxpy(12, B[0], a[11], x); B += ldb;
        daxpy(13, B[0], a[12], x); B += ldb;
        daxpy(14, B[0], a[13], x); B += ldb;
        daxpy(15, B[0], a[14], x); B += ldb;
        daxpy(16, B[0], a[15], x); B += ldb;
//         B += ldb;
//       }

      __syncthreads();

      A += (trans == CBlasNoTrans) ? kb : kb * lda;
      k -= kb;
    }

    for (int ll = 0; ll < k; ll++) {
      daxpy(ll + 1, B[0], a[ll], x);
      B += ldb;
    }

    __syncthreads();
  }

  // Process non-diagonal blocks as for DGEMM
  int k = (trans == CBlasNoTrans) ? ((uplo == CBlasUpper) ? bj : n - bj - nb)
                                  : ((uplo == CBlasUpper) ? n - bj - nb : bj);
  while (k > 0) {
    if (trans == CBlasNoTrans) {
#pragma unroll
      for (int j = 0; j < nb; j += by)
        a[threadIdx.x][j + threadIdx.y] = A[j * lda];
    }
    else {
#pragma unroll
      for (int l = 0; l < kb; l += by)
        a[l + threadIdx.y][threadIdx.x] = A[l * lda];
    }

    __syncthreads();

    if (k < kb) break;

#pragma unroll
    for (int l = 0; l < kb; l++) {
      daxpy(B[0], a[l], x);
      B += ldb;
    }

    __syncthreads();

    A += (trans == CBlasNoTrans) ? kb : kb * lda;
    k -= kb;
  }

  for (int l = 0; l < k; l++) {
    daxpy(B[0], a[l], x);
    B += ldb;
  }

  // For Upper/NoTrans and Lower/Trans process diagonal last
  if (uplo == CBlasUpper && trans == CBlasNoTrans ||
      uplo == CBlasLower && trans != CBlasNoTrans) {

    __syncthreads();

    int k = min(n - bj, nb);
    while (k > 0) {
      if (trans == CBlasNoTrans) {
#pragma unroll
        for (int j = 0; j < nb; j += by)
          a[threadIdx.x][j + threadIdx.y] =
            (diag != CBlasNonUnit && threadIdx.x == j + threadIdx.y) ? 1.0 : A[j * lda];
      }
      else {
#pragma unroll
        for (int l = 0; l < kb; l += by)
          a[l + threadIdx.y][threadIdx.x] =
            (diag != CBlasNonUnit && threadIdx.x == l + threadIdx.y) ? 1.0 : A[l * lda];
      }

      __syncthreads();

      if (k < kb) break;

// #pragma unroll
//       for (int ll = 0; ll < kb; ll++) {
//         daxpy(nb - ll, B[0], &a[ll][ll], &x[ll]);
        daxpy(16, B[0], &a[ 0][ 0], &x[ 0]); B += ldb;
        daxpy(15, B[0], &a[ 1][ 1], &x[ 1]); B += ldb;
        daxpy(14, B[0], &a[ 2][ 2], &x[ 2]); B += ldb;
        daxpy(13, B[0], &a[ 3][ 3], &x[ 3]); B += ldb;
        daxpy(12, B[0], &a[ 4][ 4], &x[ 4]); B += ldb;
        daxpy(11, B[0], &a[ 5][ 5], &x[ 5]); B += ldb;
        daxpy(10, B[0], &a[ 6][ 6], &x[ 6]); B += ldb;
        daxpy( 9, B[0], &a[ 7][ 7], &x[ 7]); B += ldb;
        daxpy( 8, B[0], &a[ 8][ 8], &x[ 8]); B += ldb;
        daxpy( 7, B[0], &a[ 9][ 9], &x[ 9]); B += ldb;
        daxpy( 6, B[0], &a[10][10], &x[10]); B += ldb;
        daxpy( 5, B[0], &a[11][11], &x[11]); B += ldb;
        daxpy( 4, B[0], &a[12][12], &x[12]); B += ldb;
        daxpy( 3, B[0], &a[13][13], &x[13]); B += ldb;
        daxpy( 2, B[0], &a[14][14], &x[14]); B += ldb;
        daxpy( 1, B[0], &a[15][15], &x[15]); B += ldb;
//         B += ldb;
//       }

      __syncthreads();

      A += (trans == CBlasNoTrans) ? kb : kb * lda;
      k -= kb;
    }

//     for (int ll = 0; ll < k; ll++) {
//       daxpy(nb - ll, B[0], &a[ll][ll], &x[ll]);
//       B += ldb;
//     }
    if (k > 0) { daxpy(16, B[0], &a[ 0][ 0], &x[ 0]); B += ldb;
    if (k > 1) { daxpy(15, B[0], &a[ 1][ 1], &x[ 1]); B += ldb;
    if (k > 2) { daxpy(14, B[0], &a[ 2][ 2], &x[ 2]); B += ldb;
    if (k > 3) { daxpy(13, B[0], &a[ 3][ 3], &x[ 3]); B += ldb;
    if (k > 4) { daxpy(12, B[0], &a[ 4][ 4], &x[ 4]); B += ldb;
    if (k > 5) { daxpy(11, B[0], &a[ 5][ 5], &x[ 5]); B += ldb;
    if (k > 6) { daxpy(10, B[0], &a[ 6][ 6], &x[ 6]); B += ldb;
    if (k > 7) { daxpy( 9, B[0], &a[ 7][ 7], &x[ 7]); B += ldb;
    if (k > 8) { daxpy( 8, B[0], &a[ 8][ 8], &x[ 8]); B += ldb;
    if (k > 9) { daxpy( 7, B[0], &a[ 9][ 9], &x[ 9]); B += ldb;
    if (k >10) { daxpy( 6, B[0], &a[10][10], &x[10]); B += ldb;
    if (k >11) { daxpy( 5, B[0], &a[11][11], &x[11]); B += ldb;
    if (k >12) { daxpy( 4, B[0], &a[12][12], &x[12]); B += ldb;
    if (k >13) { daxpy( 3, B[0], &a[13][13], &x[13]); B += ldb;
    if (k >14) { daxpy( 2, B[0], &a[14][14], &x[14]); B += ldb;
    if (k >15) { daxpy( 1, B[0], &a[15][15], &x[15]); }}}}}}}}}}}}}}}}
  }

  n -= bj;
  m -= bi + ti;
  if (n <= 0 || m <= 0) return;
  X[0] = alpha * x[ 0]; if ( 1 >= n) return; X += ldx;
  X[0] = alpha * x[ 1]; if ( 2 >= n) return; X += ldx;
  X[0] = alpha * x[ 2]; if ( 3 >= n) return; X += ldx;
  X[0] = alpha * x[ 3]; if ( 4 >= n) return; X += ldx;
  X[0] = alpha * x[ 4]; if ( 5 >= n) return; X += ldx;
  X[0] = alpha * x[ 5]; if ( 6 >= n) return; X += ldx;
  X[0] = alpha * x[ 6]; if ( 7 >= n) return; X += ldx;
  X[0] = alpha * x[ 7]; if ( 8 >= n) return; X += ldx;
  X[0] = alpha * x[ 8]; if ( 9 >= n) return; X += ldx;
  X[0] = alpha * x[ 9]; if (10 >= n) return; X += ldx;
  X[0] = alpha * x[10]; if (11 >= n) return; X += ldx;
  X[0] = alpha * x[11]; if (12 >= n) return; X += ldx;
  X[0] = alpha * x[12]; if (13 >= n) return; X += ldx;
  X[0] = alpha * x[13]; if (14 >= n) return; X += ldx;
  X[0] = alpha * x[14]; if (15 >= n) return; X += ldx;
  X[0] = alpha * x[15];
}

template void dtrmm2L<CBlasUpper, CBlasNoTrans, CBlasUnit,    64, 16, 16, 16,  4>(int, int, double, const double * __restrict__, int, const double * __restrict__, int, double * __restrict__, int);
template void dtrmm2L<CBlasUpper, CBlasNoTrans, CBlasNonUnit, 64, 16, 16, 16,  4>(int, int, double, const double * __restrict__, int, const double * __restrict__, int, double * __restrict__, int);
template void dtrmm2L<CBlasUpper, CBlasTrans,   CBlasUnit,    32, 32,  8,  8,  8>(int, int, double, const double * __restrict__, int, const double * __restrict__, int, double * __restrict__, int);
template void dtrmm2L<CBlasUpper, CBlasTrans,   CBlasNonUnit, 32, 32,  8,  8,  8>(int, int, double, const double * __restrict__, int, const double * __restrict__, int, double * __restrict__, int);
template void dtrmm2L<CBlasLower, CBlasNoTrans, CBlasUnit,    64, 16, 16, 16,  4>(int, int, double, const double * __restrict__, int, const double * __restrict__, int, double * __restrict__, int);
template void dtrmm2L<CBlasLower, CBlasNoTrans, CBlasNonUnit, 64, 16, 16, 16,  4>(int, int, double, const double * __restrict__, int, const double * __restrict__, int, double * __restrict__, int);
template void dtrmm2L<CBlasLower, CBlasTrans,   CBlasUnit,    32, 32,  8,  8,  8>(int, int, double, const double * __restrict__, int, const double * __restrict__, int, double * __restrict__, int);
template void dtrmm2L<CBlasLower, CBlasTrans,   CBlasNonUnit, 32, 32,  8,  8,  8>(int, int, double, const double * __restrict__, int, const double * __restrict__, int, double * __restrict__, int);
template void dtrmm2R<CBlasUpper, CBlasNoTrans, CBlasUnit,    64, 16, 16, 16,  4>(int, int, double, const double * __restrict__, int, const double * __restrict__, int, double * __restrict__, int);
template void dtrmm2R<CBlasUpper, CBlasNoTrans, CBlasNonUnit, 64, 16, 16, 16,  4>(int, int, double, const double * __restrict__, int, const double * __restrict__, int, double * __restrict__, int);
template void dtrmm2R<CBlasUpper, CBlasTrans,   CBlasUnit,    64, 16, 16, 16,  4>(int, int, double, const double * __restrict__, int, const double * __restrict__, int, double * __restrict__, int);
template void dtrmm2R<CBlasUpper, CBlasTrans,   CBlasNonUnit, 64, 16, 16, 16,  4>(int, int, double, const double * __restrict__, int, const double * __restrict__, int, double * __restrict__, int);
template void dtrmm2R<CBlasLower, CBlasNoTrans, CBlasUnit,    64, 16, 16, 16,  4>(int, int, double, const double * __restrict__, int, const double * __restrict__, int, double * __restrict__, int);
template void dtrmm2R<CBlasLower, CBlasNoTrans, CBlasNonUnit, 64, 16, 16, 16,  4>(int, int, double, const double * __restrict__, int, const double * __restrict__, int, double * __restrict__, int);
template void dtrmm2R<CBlasLower, CBlasTrans,   CBlasUnit,    64, 16, 16, 16,  4>(int, int, double, const double * __restrict__, int, const double * __restrict__, int, double * __restrict__, int);
template void dtrmm2R<CBlasLower, CBlasTrans,   CBlasNonUnit, 64, 16, 16, 16,  4>(int, int, double, const double * __restrict__, int, const double * __restrict__, int, double * __restrict__, int);
