#include "lapack.h"
#include "error.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <float.h>
#include <math.h>

int main(int argc, char * argv[]) {
  size_t n;
  int d = 0;

  if (argc < 2 || argc > 3) {
    fprintf(stderr, "Usage: %s <n>\nwhere:\n"
                    "  n  is the size of the matrix\n"
                    "  device  is the GPU to use (default 0)\n", argv[0]);
    return 1;
  }

  if (sscanf(argv[1], "%zu", &n) != 1) {
    fprintf(stderr, "Unable to parse number from '%s'\n", argv[1]);
    return 1;
  }

  if (argc > 2) {
    if (sscanf(argv[2], "%d", &d) != 1) {
      fprintf(stderr, "Unable to parse number from '%s'\n", argv[2]);
      return 2;
    }
  }

  srand(0);

  float * x;
  CUdeviceptr dx;
  size_t incx = 1;

  CU_ERROR_CHECK(cuInit(0));

  CUdevice device;
  CU_ERROR_CHECK(cuDeviceGet(&device, d));

  CUcontext context;
  CU_ERROR_CHECK(cuCtxCreate(&context, CU_CTX_SCHED_BLOCKING_SYNC, device));

  CULAPACKhandle handle;
  CU_ERROR_CHECK(cuLAPACKCreate(&handle));

  if ((x = malloc(incx * n * sizeof(float))) == NULL) {
    fputs("Unable to allocate x\n", stderr);
    return -1;
  }
  CU_ERROR_CHECK(cuMemAlloc(&dx, incx * n * sizeof(float)));

  for (size_t j = 0; j < n; j++)
    x[j * incx] = ((float)rand() + 1.0f) / (float)RAND_MAX;

  CU_ERROR_CHECK(cuMemcpyHtoD(dx, x, incx * n * sizeof(float)));

  float res;
  CU_ERROR_CHECK(cuSlogdet(handle, dx, incx, n, &res, NULL));

  float sum = 0.0f;
  float c = 0.0f;
  for (size_t j = 0; j < n; j++) {
    float y = logf(x[j * incx]) - c;
    float t = sum + y;
    c = (t - sum) - y;
    sum = t;
  }
  sum *= 2.0f;

  float diff = fabsf(sum - res);
  bool passed = (diff < 2.0f * (float)n * FLT_EPSILON);

  CUevent start, stop;
  CU_ERROR_CHECK(cuEventCreate(&start, CU_EVENT_BLOCKING_SYNC));
  CU_ERROR_CHECK(cuEventCreate(&stop, CU_EVENT_BLOCKING_SYNC));

  CU_ERROR_CHECK(cuEventRecord(start, NULL));
  for (size_t i = 0; i < 20; i++)
    CU_ERROR_CHECK(cuSlogdet(handle, dx, incx, n, &res, NULL));
  CU_ERROR_CHECK(cuEventRecord(stop, NULL));
  CU_ERROR_CHECK(cuEventSynchronize(stop));

  float time;
  CU_ERROR_CHECK(cuEventElapsedTime(&time, start, stop));
  time /= 20;
  time *= 1.e-3f;

  CU_ERROR_CHECK(cuEventDestroy(start));
  CU_ERROR_CHECK(cuEventDestroy(stop));

  const size_t bandwidth = n * sizeof(float);
  fprintf(stdout, "%.3es %.3gGB/s Error: %.3e\n%sED!\n", time,
          (float)bandwidth / (time * (float)(1 << 30)), diff, (passed) ? "PASS" : "FAIL");

  free(x);
  CU_ERROR_CHECK(cuMemFree(dx));

  CU_ERROR_CHECK(cuLAPACKDestroy(handle));

  CU_ERROR_CHECK(cuCtxDestroy(context));

  return (int)!passed;
}
