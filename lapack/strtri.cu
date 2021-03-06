#define __DEVICE_ONLY
#include "../blas/strmm.cu"
#undef __DEVICE_ONLY

/*
 * Indexing function for upper triangular packed storage mode.  Only works when
 * i <= j otherwise generates an out-of-bounds access in shared memory and CUDA
 * will segfault.
 */
__device__ int upper(int i, int j) {
  return ((j * (j + 1)) / 2) + i;
}

/*
 * Indexing function for lower triangular packed storage mode.  Only works when
 * i >= j otherwise generates an out-of-bounds access in shared memory and CUDA
 * will segfault.
 */
template <unsigned int bx>
__device__ int lower(int i, int j) {
  return ((2 * bx - j - 1) * j) / 2 + i;
}

/**
 * In-place triangular packed unblocked triangular inverse device function.
 *
 * @param n     the size of the matrix
 * @param A     the matrix stored using upper or lower triangular packed storage
 *                mode
 * @param info  info
 *
 * A and info are expected to be in shared memory but may still work (slower) if
 * in global memory.
 */
template <CBlasUplo uplo, CBlasDiag diag, unsigned int nb, unsigned int bx>
__device__ void stpti2(int n, float * __restrict__ A, int * __restrict__ info) {
  // thread 0 is the only thread to write to info in global memory
  if (threadIdx.x == 0)
    *info = 0;  // initialise info to zero

  // Copy of diagonal element in shared memory prior to updating
  __shared__ float ajj;

  const int i = threadIdx.y * bx + threadIdx.x;

  if (uplo == CBlasUpper) {
    // Perform the triangular inverse
    // Accesses do not have to be coalesced or aligned as they would if A were
    // in global memory.  Using triangular packed storage also neatly avoids
    // bank conflicts.
    for (int j = 0; j < n; j++) {
      float temp;
      // Read current column into registers
      if (i <= j)
        temp = A[upper(i, j)];

      // Thread j calculates the diagonal element
      if (i == j) {
        if (diag == CBlasNonUnit) {
          if (temp == 0.0f)
            *info = j + 1;
          else {
            A[upper(j, j)] = 1.0f / temp;
            ajj = -A[upper(j, j)];
          }
        }
        else
          ajj = -1.0f;
      }

      __syncthreads();

      // If info != 0 return (matrix is singular)
      if (*info != 0)
        return;

      if (i < j) {
        if (diag == CBlasNonUnit)
          temp *= A[upper(i, i)];
        for (int k = i + 1; k < j; k++)
          temp += A[upper(i, k)] * A[upper(k, j)];
      }

      __syncthreads();

      if (i < j)
        A[upper(i, j)] = temp * ajj;

      __syncthreads();
    }
  }
  else {
    // Perform the triangular inverse
    // Accesses do not have to be coalesced or aligned as they would if A were
    // in global memory.  Using triangular packed storage also avoids bank
    // conflicts.
    for (int j = n - 1; j >= 0; j--) {
      float temp;
      // Read current column into registers
      if (i >= j)
        temp = A[lower<nb>(i, j)];

      // Thread j calculates the diagonal element
      if (i == j) {
        if (diag == CBlasNonUnit) {
          if (temp == 0.0f)
            *info = j + 1;
          else {
            A[lower<nb>(j, j)] = 1.0f / temp;
            ajj = -A[lower<nb>(j, j)];
          }
        }
        else
          ajj = -1.0f;
      }

      __syncthreads();

      // If info != 0 return (matrix is singular)
      if (*info != 0)
        return;

      if (i > j) {
        if (diag == CBlasNonUnit)
          temp *= A[lower<nb>(i, i)];
        for (int k = j + 1; k < i; k++)
          temp += A[lower<nb>(i, k)] * A[lower<nb>(k, j)];
      }

      __syncthreads();

      if (i > j)
        A[lower<nb>(i, j)] = temp * ajj;

      __syncthreads();
    }
  }
}

template <CBlasUplo uplo, CBlasDiag diag, unsigned int bx>
__global__ void strti2(const float * A, float * B, int * info, int lda, int ldb, int n) {
  // info parameter cached in shared memory for fast access by all threads in the block
  __shared__ int sinfo;

  /*
   * For efficient data reuse A needs to be cached in shared memory.  In order
   * to get maximum instruction throughput 64 threads are needed but this would
   * use all 16384 bytes (64 * 64 * sizeof(float)) of shared memory to store A.
   * Triangular packed storage mode is therefore used to store only the
   * triangle of A being updated using 8320 bytes((64 * (64 + 1)) / 2 * sizeof(float))
   * of shared memory.
   * Since this is only ever going to be run using one thread block shared
   * memory and register use can be higher than when trying to fit multiple
   * thread blocks onto each multiprocessor.
   */
  __shared__ float a[(bx * (bx + 1)) / 2];

  if (uplo == CBlasUpper) {
    // Read upper triangle of A into shared memory
    #pragma unroll
    for (int j = 0; j < n; j++) {
      if (threadIdx.x <= j)
        a[upper(threadIdx.x, j)] = A[j * lda + threadIdx.x];
    }

    __syncthreads();

    // Perform the triangular inverse using the packed device function
    stpti2<CBlasUpper, diag, bx, bx>(n, a, &sinfo);

    // Write info back to global memory
    if (threadIdx.x == 0)
      *info = sinfo;

    // Write the upper triangle of A back to B in global memory
    for (int j = 0; j < n; j++) {
      if (threadIdx.x <= j)
        B[j * ldb + threadIdx.x] = a[upper(threadIdx.x, j)];
    }
  }
  else {
    // Read lower triangle of A into shared memory
    #pragma unroll
    for (int j = 0; j < n; j++) {
      if (threadIdx.x >= j)
        a[lower<bx>(threadIdx.x, j)] = A[j * lda + threadIdx.x];
    }

    __syncthreads();

    // Perform the triangular inverse using the packed device function
    stpti2<CBlasLower, diag, bx, bx>(n, a, &sinfo);

    // Write info back to global memory
    if (threadIdx.x == 0)
      *info = sinfo;

    // Write the lower triangle of A back to B in global memory
    for (int j = 0; j < n; j++) {
      if (threadIdx.x >= j)
        B[j * ldb + threadIdx.x] = a[lower<bx>(threadIdx.x, j)];
    }
  }
}

template <CBlasUplo uplo,
          unsigned int mb, unsigned int nb, unsigned int kb,
          unsigned int bx, unsigned int by>
__global__ void strtimm2(float * __restrict__ A, float * __restrict__ B,
                         int * __restrict__ info, int lda, int ldb, int j, int jb, int n) {
  // info parameter cached in shared memory for fast access by all threads in the block
  __shared__ int sinfo;

  /*
   * For efficient data reuse A needs to be cached in shared memory.  In order
   * to get maximum instruction throughput 64 threads are needed but this would
   * use all 16384 bytes (64 * 64 * sizeof(float)) of shared memory to store A.
   * Triangular packed storage mode is therefore used to store only the
   * triangle of A being updated using 8320 bytes((64 * (64 + 1)) / 2 * sizeof(float))
   * of shared memory.
   * Since this is only ever going to be run using one thread block shared
   * memory and register use can be higher than when trying to fit multiple
   * thread blocks onto each multiprocessor.
   */
  __shared__ float a[(nb * (nb + 1)) / 2];

  const int i = threadIdx.y * bx + threadIdx.x;

  if (uplo == CBlasUpper) {
    if (blockIdx.y == gridDim.y - 1) {
      if (blockIdx.x == 0) {
        // Read upper triangle of A into shared memory
        #pragma unroll
        for (int k = 0; k < jb; k++) {
          if (i <= k)
            a[upper(i, k)] = A[(j + k) * lda + j + i];
        }

        __syncthreads();

        // Perform the triangular inverse using the packed device function
        stpti2<CBlasUpper, CBlasNonUnit, nb, bx>(jb, a, &sinfo);

        // Write info back to global memory
        if (i == 0)
          *info = sinfo;

        // Write the upper triangle of A back to global memory
        for (int k = 0; k < jb; k++) {
          if (i <= k)
            A[(j + k) * lda + j + i] = a[upper(i, j)];
        }
      }
    }
    else
      strmm2LUN<CBlasNonUnit, mb, nb, kb, bx, by>(j, jb, 1.0f, A, lda, &A[j * lda], lda, B, ldb);
  }
  else {
    if (blockIdx.y == gridDim.y - 1) {
      if (blockIdx.x == 0) {
        // Read lower triangle of A into shared memory
        #pragma unroll
        for (int k = 0; k < jb; k++) {
          if (i >= k)
            a[lower<bx>(i, k)] = A[(j + k) * lda + j + i];
        }

        __syncthreads();

        // Perform the triangular inverse using the packed device function
        stpti2<CBlasLower, CBlasNonUnit, nb, bx>(jb, a, &sinfo);

        // Write info back to global memory
        if (i == 0)
          *info = sinfo;

        // Write the lower triangle of A back to global memory
        for (int k = 0; k < n; k++) {
          if (i >= k)
            A[(j + k) * lda + j + i] = a[lower<bx>(i, k)];
        }
      }
    }
    else
      strmm2LLN<CBlasNonUnit, mb, nb, kb, bx, by>(n - j - jb, jb, 1.0f,
                                                  &A[(j + jb) * lda + j + jb], lda,
                                                  &A[j * lda + j + jb], lda, B, ldb);
  }
}

#ifndef __DEVICE_ONLY
template __global__ void strti2<CBlasUpper, CBlasUnit, 64>(const float * __restrict__, float * __restrict__, int * __restrict__, int, int, int);
template __global__ void strti2<CBlasUpper, CBlasNonUnit, 64>(const float * __restrict__, float * __restrict__, int * __restrict__, int, int, int);
template __global__ void strti2<CBlasLower, CBlasUnit, 64>(const float * __restrict__, float * __restrict__, int * __restrict__, int, int, int);
template __global__ void strti2<CBlasLower, CBlasNonUnit, 64>(const float * __restrict__, float * __restrict__, int * __restrict__, int, int, int);

template __global__ void strtimm2<CBlasUpper, 64, 16, 16, 16, 4>(float * __restrict__, float * __restrict__, int * __restrict__, int, int, int, int, int);
template __global__ void strtimm2<CBlasLower, 64, 16, 16, 16, 4>(float * __restrict__, float * __restrict__, int * __restrict__, int, int, int, int, int);
#endif

#if 0
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>
#include <float.h>
#include <math.h>

#define CUDA_ERROR_CHECK(call) \
  do { \
    cudaError_t error = (call); \
    if (error != cudaSuccess) { \
      fprintf(stderr, "CUDA Runtime error in %s (%s:%d): %s\n", __func__, __FILE__, __LINE__, cudaGetErrorString(error)); \
      return error; \
    } \
  } while (false)

#define xerbla(info) \
  fprintf(stderr, "On entry to %s parameter %d had an invalid value\n", __func__, (info))

extern "C" void strti2_(const char *, const char *, const int *, float *, const int *, int *);
static inline void strti2(CBlasUplo uplo, CBlasDiag diag, int n, float * A, int lda, int * info) {
  if (uplo == CBlasUpper) {
    if (diag == CBlasNonUnit)
      strti2<CBlasUpper, CBlasNonUnit, 64><<<1,64>>>(A, A, info, lda, lda, n);
    else
      strti2<CBlasUpper, CBlasUnit, 64><<<1,64>>>(A, A, info, lda, lda, n);
  }
  else {
    if (diag == CBlasNonUnit)
      strti2<CBlasLower, CBlasNonUnit, 64><<<1,64>>>(A, A, info, lda, lda, n);
    else
      strti2<CBlasLower, CBlasUnit, 64><<<1,64>>>(A, A, info, lda, lda, n);
  }
}

static int cond(int, float, float *, size_t);

int main(int argc, char * argv[]) {
  CBlasUplo uplo;
  CBlasDiag diag;
  int n;

  if (argc != 4) {
    fprintf(stderr, "Usage %s <uplo> <diag> <n>\n"
                    "where:\n"
                    "  <uplo>  is 'U' or 'u' for CBlasUpper or 'L' or 'l' for CBlasLower\n"
                    "  <diag>  is 'U' or 'u' for CBlasUnit or 'N' or 'n' for CBlasNonUnit\n"
                    "  <n>     is the size of the matrix\n", argv[0]);
    return -1;
  }

  char u;
  if (sscanf(argv[1], "%c", &u) != 1) {
    fprintf(stderr, "Unable to parse character from '%s'\n", argv[1]);
    return 1;
  }
  switch (u) {
    case 'u': case 'U': uplo = CBlasUpper; break;
    case 'l': case 'L': uplo = CBlasLower; break;
    default: fprintf(stderr, "Unknown uplo '%c'\n", u); return 1;
  }

  char d;
  if (sscanf(argv[2], "%c", &d) != 1) {
    fprintf(stderr, "Unable to parse character from '%s'\n", argv[2]);
    return 2;
  }
  switch (d) {
    case 'u': case 'U': diag = CBlasUnit; break;
    case 'n': case 'N': diag = CBlasNonUnit; break;
    default: fprintf(stderr, "Unknown diag '%c'\n", u); return 1;
  }

  if (sscanf(argv[3], "%d", &n) != 1) {
    fprintf(stderr, "Unable to parse integer from '%s'\n", argv[3]);
    return 3;
  }

  float * A, * dA, * refA;
  size_t lda = (n + 3) & ~3, dlda;
  int * dinfo;
  if ((A = (float *)malloc(lda * n * sizeof(float))) == NULL) {
    fprintf(stderr, "Failed to allocate A\n");
    return -1;
  }
  if ((refA = (float *)malloc(lda * n * sizeof(float))) == NULL) {
    fprintf(stderr, "Failed to allocate refA\n");
    return -2;
  }
  CUDA_ERROR_CHECK(cudaMallocPitch((void **)&dA, &dlda, n * sizeof(float), n));
  CUDA_ERROR_CHECK(cudaMalloc((void **)&dinfo, sizeof(int)));
  dlda /= sizeof(float);

  cond(n, 2.0f, A, lda);

  for (int j = 0; j < n; j++)
    memcpy(&refA[j * lda], &A[j * lda], n * sizeof(float));

  CUDA_ERROR_CHECK(cudaMemcpy2D(dA, dlda * sizeof(float), A, lda * sizeof(float), n * sizeof(float), n, cudaMemcpyHostToDevice));

  for (int i = 0; i < n; i++) {
    for (int j = 0; j < n; j++)
      fprintf(stderr, "%15.6f", A[j * lda + i]);
    fprintf(stderr, "\n");
  }

  int info = 0, refInfo = 0;
  strti2_((const char *)&uplo, (const char *)&diag, &n, refA, (const int *)&lda, &refInfo);
  strti2(uplo, diag, n, dA, dlda, dinfo);
  CUDA_ERROR_CHECK(cudaMemcpy2D(A, lda * sizeof(float), dA, dlda * sizeof(float), n * sizeof(float), n, cudaMemcpyDeviceToHost));
  CUDA_ERROR_CHECK(cudaMemcpy(&info, dinfo, sizeof(int), cudaMemcpyDeviceToHost));

  fprintf(stderr, "\n");
  for (int i = 0; i < n; i++) {
    for (int j = 0; j < n; j++)
      fprintf(stderr, "%15.6f", refA[j * lda + i]);
    fprintf(stderr, "\n");
  }

  fprintf(stderr, "\n");
  for (int i = 0; i < n; i++) {
    for (int j = 0; j < n; j++)
      fprintf(stderr, "%15.6f", A[j * lda + i]);
    fprintf(stderr, "\n");
  }

  float error = 0.0f;
  for (int j = 0; j < n; j++) {
    for (int i = 0; i < n; i++) {
      float diff = fabsf(refA[j * lda + i] - A[j * lda + i]);
      if (diff > error)
        error = diff;
    }
  }

  fprintf(stdout, "Info = %d, refInfo = %d, Error = %6.3e\n", info, refInfo, error);

  free(A);
  free(refA);
  CUDA_ERROR_CHECK(cudaFree(dA));
  CUDA_ERROR_CHECK(cudaFree(dinfo));

  return info;
}

static int cond(int n, float c, float * A, size_t lda) {
  int info = 0;
  if (n < 2)
    info = -1;
  else if (c < 1.0)
    info = -2;
  else if (lda < n)
    info = -4;
  if (info != 0) {
    xerbla(-info);
    return info;
  }

  float * u, * v, * w;
  size_t offset = (n + 3u) & ~3u;

  if ((u = (float *)malloc(3 * offset * sizeof(float))) == NULL)
    return 1;

  v = &u[offset];
  w = &v[offset];

  // Initialise A as a diagonal matrix whose diagonal consists of numbers from
  // [1,c] with 1 and c chosen at least once (here in the top left)
  for (int j = 0; j < n; j++) {
    for (int i = 0; i < n; i++)
      A[j * lda + i] = 0.0f;
  }

  A[0] = 1.0f;
  A[lda + 1] = c;
  for (int j = 2; j < n; j++)
    A[j * lda + j] = ((float) rand() / (float)RAND_MAX) * (c - 1.0f) + 1.0f;

  float t = 0.0f, s = 0.0f;
  for (int j = 0; j < n; j++) {
    // u is a random vector
    u[j] = (float)rand() / (float)RAND_MAX;
    // v = Au
    v[j] = A[j * lda + j] * u[j];
    // t = 2/u'u
    t += u[j] * u[j];
    // s = t^2 u'v / 2
    s += u[j] * v[j];
  }
  t = 2.0f / t;
  s = t * t * s / 2.0f;

  // w = tv - su
  for (int j = 0; j < n; j++)
    w[j] = t * v[j] - s * u[j];

  // A -= uw' + wu'
  for (int j = 0; j < n; j++) {
    for (int i = 0; i < n; i++)
      A[j * lda + i] -= u[i] * w[j] + w[i] * u[j];
  }

  free(u);

  return 0;
}
#endif
