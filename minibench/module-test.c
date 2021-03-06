#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <cuda.h>
#include <string.h>
#include "error.h"
#include "flop-test.fatbin.c"

#define ITERATIONS 20

/* !!! THE COMPILER CACHE IN ~/.nv/ NEEDS TO BE DELETED BEFORE RUNNING THIS !!! */
int main() {
  int error;

  CU_ERROR_CHECK(cuInit(0));

  CUdevice device;
  CU_ERROR_CHECK(cuDeviceGet(&device, 0));

  CUcontext context;
  CU_ERROR_CHECK(cuCtxCreate(&context, CU_CTX_SCHED_AUTO, device));

  struct timeval start, stop;
  double time;
  CUmodule modules[ITERATIONS];
  CUfunction functions[ITERATIONS];

  // Time how long it takes to load and unload a module from a PTX file using
  // cuModuleLoad.
  // Time it for the first two times then for a number of times and take an
  // average:
  //   If the time to load the module a second time is less than the time to load
  //   it the first time then a PTX compiler cache is being used (and the
  //   difference in time is the time it takes to compile the PTX minus a cache
  //   miss).
  //   If the average time is less than the initial time then the module is
  //   being reference-counted.
  if ((error = gettimeofday(&start, 0)) != 0) {
    fprintf(stderr, "Unable to get start time: %s\n", strerror(error));
    return error;
  }
  CU_ERROR_CHECK(cuModuleLoad(&modules[0], "flop-test.ptx"));
  if ((error = gettimeofday(&stop, 0)) != 0) {
    fprintf(stderr, "Unable to get stop time: %s\n", strerror(error));
    return error;
  }
  time = (double)(stop.tv_sec - start.tv_sec) + ((double)(stop.tv_usec - start.tv_usec) * 1.E-6);
  fprintf(stderr, "cuModuleLoad (ptx, 1st): %.3es\n", time);

  if ((error = gettimeofday(&start, 0)) != 0) {
    fprintf(stderr, "Unable to get start time: %s\n", strerror(error));
    return error;
  }
  CU_ERROR_CHECK(cuModuleLoad(&modules[1], "flop-test.ptx"));
  if ((error = gettimeofday(&stop, 0)) != 0) {
    fprintf(stderr, "Unable to get stop time: %s\n", strerror(error));
    return error;
  }
  time = (double)(stop.tv_sec - start.tv_sec) + ((double)(stop.tv_usec - start.tv_usec) * 1.E-6);
  fprintf(stderr, "cuModuleLoad (ptx, 2nd): %.3es\n", time);

  if ((error = gettimeofday(&start, 0)) != 0) {
    fprintf(stderr, "Unable to get start time: %s\n", strerror(error));
    return error;
  }
  CU_ERROR_CHECK(cuModuleUnload(modules[0]));
  if ((error = gettimeofday(&stop, 0)) != 0) {
    fprintf(stderr, "Unable to get stop time: %s\n", strerror(error));
    return error;
  }
  time = (double)(stop.tv_sec - start.tv_sec) + ((double)(stop.tv_usec - start.tv_usec) * 1.E-6);
  fprintf(stderr, "cuModuleUnload (ptx, 2nd): %.3es\n", time);

  if ((error = gettimeofday(&start, 0)) != 0) {
    fprintf(stderr, "Unable to get start time: %s\n", strerror(error));
    return error;
  }
  CU_ERROR_CHECK(cuModuleUnload(modules[1]));
  if ((error = gettimeofday(&stop, 0)) != 0) {
    fprintf(stderr, "Unable to get stop time: %s\n", strerror(error));
    return error;
  }
  time = (double)(stop.tv_sec - start.tv_sec) + ((double)(stop.tv_usec - start.tv_usec) * 1.E-6);
  fprintf(stderr, "cuModuleUnload (ptx, 1st): %.3es\n", time);

  if ((error = gettimeofday(&start, 0)) != 0) {
    fprintf(stderr, "Unable to get start time: %s\n", strerror(error));
    return error;
  }
  for (size_t i = 0; i < ITERATIONS; i++)
    CU_ERROR_CHECK(cuModuleLoad(&modules[i], "flop-test.ptx"));
  if ((error = gettimeofday(&stop, 0)) != 0) {
    fprintf(stderr, "Unable to get stop time: %s\n", strerror(error));
    return error;
  }
  time = (double)(stop.tv_sec - start.tv_sec) + ((double)(stop.tv_usec - start.tv_usec) * 1.E-6);
  fprintf(stderr, "cuModuleLoad (ptx, mean): %.3es\n", time / (double)ITERATIONS);

  if ((error = gettimeofday(&start, 0)) != 0) {
    fprintf(stderr, "Unable to get start time: %s\n", strerror(error));
    return error;
  }
  for (size_t i = 0; i < ITERATIONS; i++)
    CU_ERROR_CHECK(cuModuleUnload(modules[i]));
  if ((error = gettimeofday(&stop, 0)) != 0) {
    fprintf(stderr, "Unable to get stop time: %s\n", strerror(error));
    return error;
  }
  time = (double)(stop.tv_sec - start.tv_sec) + ((double)(stop.tv_usec - start.tv_usec) * 1.E-6);
  fprintf(stderr, "cuModuleUnload (ptx, mean): %.3es\n\n", time / (double)ITERATIONS);

  // Time how long it takes to load and unload a module from a CUBIN file using
  // cuModuleLoad.
  // Time it for the first two times then for a number of times and take an
  // average:
  //   If the time to load the module a second time is less than the time to load
  //   it the first time then a file cache is being used (and the difference in
  // time is the time it takes to read the CUBIN minus a cache miss).
  //   If the average time is less than the initial time then the module is
  //   being reference-counted.
  if ((error = gettimeofday(&start, 0)) != 0) {
    fprintf(stderr, "Unable to get start time: %s\n", strerror(error));
    return error;
  }
  CU_ERROR_CHECK(cuModuleLoad(&modules[0], "flop-test.cubin"));
  if ((error = gettimeofday(&stop, 0)) != 0) {
    fprintf(stderr, "Unable to get stop time: %s\n", strerror(error));
    return error;
  }
  time = (double)(stop.tv_sec - start.tv_sec) + ((double)(stop.tv_usec - start.tv_usec) * 1.E-6);
  fprintf(stderr, "cuModuleLoad (cubin, 1st): %.3es\n", time);

  if ((error = gettimeofday(&start, 0)) != 0) {
    fprintf(stderr, "Unable to get start time: %s\n", strerror(error));
    return error;
  }
  CU_ERROR_CHECK(cuModuleLoad(&modules[1], "flop-test.cubin"));
  if ((error = gettimeofday(&stop, 0)) != 0) {
    fprintf(stderr, "Unable to get stop time: %s\n", strerror(error));
    return error;
  }
  time = (double)(stop.tv_sec - start.tv_sec) + ((double)(stop.tv_usec - start.tv_usec) * 1.E-6);
  fprintf(stderr, "cuModuleLoad (cubin, 2nd): %.3es\n", time);

  if ((error = gettimeofday(&start, 0)) != 0) {
    fprintf(stderr, "Unable to get start time: %s\n", strerror(error));
    return error;
  }
  CU_ERROR_CHECK(cuModuleUnload(modules[1]));
  if ((error = gettimeofday(&stop, 0)) != 0) {
    fprintf(stderr, "Unable to get stop time: %s\n", strerror(error));
    return error;
  }
  time = (double)(stop.tv_sec - start.tv_sec) + ((double)(stop.tv_usec - start.tv_usec) * 1.E-6);
  fprintf(stderr, "cuModuleUnload (cubin, 2nd): %.3es\n", time);

  if ((error = gettimeofday(&start, 0)) != 0) {
    fprintf(stderr, "Unable to get start time: %s\n", strerror(error));
    return error;
  }
  CU_ERROR_CHECK(cuModuleUnload(modules[0]));
  if ((error = gettimeofday(&stop, 0)) != 0) {
    fprintf(stderr, "Unable to get stop time: %s\n", strerror(error));
    return error;
  }
  time = (double)(stop.tv_sec - start.tv_sec) + ((double)(stop.tv_usec - start.tv_usec) * 1.E-6);
  fprintf(stderr, "cuModuleUnload (cubin, 1st): %.3es\n", time);

  if ((error = gettimeofday(&start, 0)) != 0) {
    fprintf(stderr, "Unable to get start time: %s\n", strerror(error));
    return error;
  }
  for (size_t i = 0; i < ITERATIONS; i++)
    CU_ERROR_CHECK(cuModuleLoad(&modules[i], "flop-test.cubin"));
  if ((error = gettimeofday(&stop, 0)) != 0) {
    fprintf(stderr, "Unable to get stop time: %s\n", strerror(error));
    return error;
  }
  time = (double)(stop.tv_sec - start.tv_sec) + ((double)(stop.tv_usec - start.tv_usec) * 1.E-6);
  fprintf(stderr, "cuModuleLoad (cubin, mean): %.3es\n", time / (double)ITERATIONS);

  if ((error = gettimeofday(&start, 0)) != 0) {
    fprintf(stderr, "Unable to get start time: %s\n", strerror(error));
    return error;
  }
  for (size_t i = 0; i < ITERATIONS; i++)
    CU_ERROR_CHECK(cuModuleUnload(modules[i]));
  if ((error = gettimeofday(&stop, 0)) != 0) {
    fprintf(stderr, "Unable to get stop time: %s\n", strerror(error));
    return error;
  }
  time = (double)(stop.tv_sec - start.tv_sec) + ((double)(stop.tv_usec - start.tv_usec) * 1.E-6);
  fprintf(stderr, "cuModuleUnload (cubin, mean): %.3es\n\n", time / (double)ITERATIONS);

  // Time how long it takes to load and unload a module from a FATBIN file (with
  // a matching CUBIN image inside) using cuModuleLoad.
  if ((error = gettimeofday(&start, 0)) != 0) {
    fprintf(stderr, "Unable to get start time: %s\n", strerror(error));
    return error;
  }
  CU_ERROR_CHECK(cuModuleLoad(&modules[0], "flop-test.fatbin"));
  if ((error = gettimeofday(&stop, 0)) != 0) {
    fprintf(stderr, "Unable to get stop time: %s\n", strerror(error));
    return error;
  }
  time = (double)(stop.tv_sec - start.tv_sec) + ((double)(stop.tv_usec - start.tv_usec) * 1.E-6);
  fprintf(stderr, "cuModuleLoad (cubin in fatbin, 1st): %.3es\n", time);

  if ((error = gettimeofday(&start, 0)) != 0) {
    fprintf(stderr, "Unable to get start time: %s\n", strerror(error));
    return error;
  }
  CU_ERROR_CHECK(cuModuleLoad(&modules[1], "flop-test.fatbin"));
  if ((error = gettimeofday(&stop, 0)) != 0) {
    fprintf(stderr, "Unable to get stop time: %s\n", strerror(error));
    return error;
  }
  time = (double)(stop.tv_sec - start.tv_sec) + ((double)(stop.tv_usec - start.tv_usec) * 1.E-6);
  fprintf(stderr, "cuModuleLoad (cubin in fatbin, 2nd): %.3es\n", time);

  if ((error = gettimeofday(&start, 0)) != 0) {
    fprintf(stderr, "Unable to get start time: %s\n", strerror(error));
    return error;
  }
  CU_ERROR_CHECK(cuModuleUnload(modules[1]));
  if ((error = gettimeofday(&stop, 0)) != 0) {
    fprintf(stderr, "Unable to get stop time: %s\n", strerror(error));
    return error;
  }
  time = (double)(stop.tv_sec - start.tv_sec) + ((double)(stop.tv_usec - start.tv_usec) * 1.E-6);
  fprintf(stderr, "cuModuleUnload (cubin in fatbin, 2nd): %.3es\n", time);

  if ((error = gettimeofday(&start, 0)) != 0) {
    fprintf(stderr, "Unable to get start time: %s\n", strerror(error));
    return error;
  }
  CU_ERROR_CHECK(cuModuleUnload(modules[0]));
  if ((error = gettimeofday(&stop, 0)) != 0) {
    fprintf(stderr, "Unable to get stop time: %s\n", strerror(error));
    return error;
  }
  time = (double)(stop.tv_sec - start.tv_sec) + ((double)(stop.tv_usec - start.tv_usec) * 1.E-6);
  fprintf(stderr, "cuModuleUnload (cubin in fatbin, 1st): %.3es\n", time);

  if ((error = gettimeofday(&start, 0)) != 0) {
    fprintf(stderr, "Unable to get start time: %s\n", strerror(error));
    return error;
  }
  for (size_t i = 0; i < ITERATIONS; i++)
    CU_ERROR_CHECK(cuModuleLoad(&modules[i], "flop-test.fatbin"));
  if ((error = gettimeofday(&stop, 0)) != 0) {
    fprintf(stderr, "Unable to get stop time: %s\n", strerror(error));
    return error;
  }
  time = (double)(stop.tv_sec - start.tv_sec) + ((double)(stop.tv_usec - start.tv_usec) * 1.E-6);
  fprintf(stderr, "cuModuleLoad (cubin in fatbin, mean): %.3es\n", time / (double)ITERATIONS);

  if ((error = gettimeofday(&start, 0)) != 0) {
    fprintf(stderr, "Unable to get start time: %s\n", strerror(error));
    return error;
  }
  for (size_t i = 0; i < ITERATIONS; i++)
    CU_ERROR_CHECK(cuModuleUnload(modules[i]));
  if ((error = gettimeofday(&stop, 0)) != 0) {
    fprintf(stderr, "Unable to get stop time: %s\n", strerror(error));
    return error;
  }
  time = (double)(stop.tv_sec - start.tv_sec) + ((double)(stop.tv_usec - start.tv_usec) * 1.E-6);
  fprintf(stderr, "cuModuleUnload (cubin in fatbin, mean): %.3es\n\n", time / (double)ITERATIONS);

  // Repeat the experiment with cuModuleLoadData using an embedded fatbin.
  if ((error = gettimeofday(&start, 0)) != 0) {
    fprintf(stderr, "Unable to get start time: %s\n", strerror(error));
    return error;
  }
  CU_ERROR_CHECK(cuModuleLoadData(&modules[0], imageBytes));
  if ((error = gettimeofday(&stop, 0)) != 0) {
    fprintf(stderr, "Unable to get stop time: %s\n", strerror(error));
    return error;
  }
  time = (double)(stop.tv_sec - start.tv_sec) + ((double)(stop.tv_usec - start.tv_usec) * 1.E-6);
  fprintf(stderr, "cuModuleLoadData (embedded fatbin, 1st): %.3es\n", time);

  if ((error = gettimeofday(&start, 0)) != 0) {
    fprintf(stderr, "Unable to get start time: %s\n", strerror(error));
    return error;
  }
  CU_ERROR_CHECK(cuModuleLoadData(&modules[1], imageBytes));
  if ((error = gettimeofday(&stop, 0)) != 0) {
    fprintf(stderr, "Unable to get stop time: %s\n", strerror(error));
    return error; }
  time = (double)(stop.tv_sec - start.tv_sec) + ((double)(stop.tv_usec - start.tv_usec) * 1.E-6);
  fprintf(stderr, "cuModuleLoadData (embedded fatbin), 2nd): %.3es\n", time);

  if ((error = gettimeofday(&start, 0)) != 0) {
    fprintf(stderr, "Unable to get start time: %s\n", strerror(error));
    return error;
  }
  CU_ERROR_CHECK(cuModuleUnload(modules[1]));
  if ((error = gettimeofday(&stop, 0)) != 0) {
    fprintf(stderr, "Unable to get stop time: %s\n", strerror(error));
    return error;
  }
  time = (double)(stop.tv_sec - start.tv_sec) + ((double)(stop.tv_usec - start.tv_usec) * 1.E-6);
  fprintf(stderr, "cuModuleUnload (embedded fatbin, 2nd): %.3es\n", time);

  if ((error = gettimeofday(&start, 0)) != 0) {
    fprintf(stderr, "Unable to get start time: %s\n", strerror(error));
    return error;
  }
  CU_ERROR_CHECK(cuModuleUnload(modules[0]));
  if ((error = gettimeofday(&stop, 0)) != 0) {
    fprintf(stderr, "Unable to get stop time: %s\n", strerror(error));
    return error;
  }
  time = (double)(stop.tv_sec - start.tv_sec) + ((double)(stop.tv_usec - start.tv_usec) * 1.E-6);
  fprintf(stderr, "cuModuleUnload (embedded fatbin, 1st): %.3es\n", time);

  if ((error = gettimeofday(&start, 0)) != 0) {
    fprintf(stderr, "Unable to get start time: %s\n", strerror(error));
    return error;
  }
  for (size_t i = 0; i < ITERATIONS; i++)
    CU_ERROR_CHECK(cuModuleLoadData(&modules[i], imageBytes));
  if ((error = gettimeofday(&stop, 0)) != 0) {
    fprintf(stderr, "Unable to get stop time: %s\n", strerror(error));
    return error;
  }
  time = (double)(stop.tv_sec - start.tv_sec) + ((double)(stop.tv_usec - start.tv_usec) * 1.E-6);
  fprintf(stderr, "cuModuleLoadData (embedded fatbin, mean): %.3es\n", time / (double)ITERATIONS);

  if ((error = gettimeofday(&start, 0)) != 0) {
    fprintf(stderr, "Unable to get start time: %s\n", strerror(error));
    return error;
  }
  for (size_t i = 0; i < ITERATIONS; i++)
    CU_ERROR_CHECK(cuModuleUnload(modules[i]));
  if ((error = gettimeofday(&stop, 0)) != 0) {
    fprintf(stderr, "Unable to get stop time: %s\n", strerror(error));
    return error;
  }
  time = (double)(stop.tv_sec - start.tv_sec) + ((double)(stop.tv_usec - start.tv_usec) * 1.E-6);
  fprintf(stderr, "cuModuleUnload (embedded fatbin, mean): %.3es\n\n", time / (double)ITERATIONS);

  CU_ERROR_CHECK(cuModuleLoadData(&modules[0], imageBytes));
  if ((error = gettimeofday(&start, 0)) != 0) {
    fprintf(stderr, "Unable to get start time: %s\n", strerror(error));
    return error;
  }
  for (size_t i = 0; i < ITERATIONS; i++)
    CU_ERROR_CHECK(cuModuleGetFunction(&functions[i], modules[0], "fmad"));
  if ((error = gettimeofday(&stop, 0)) != 0) {
    fprintf(stderr, "Unable to get stop time: %s\n", strerror(error));
    return error;
  }
  time = (double)(stop.tv_sec - start.tv_sec) + ((double)(stop.tv_usec - start.tv_usec) * 1.E-6);
  fprintf(stderr, "cuModuleGetFunction: %.3es\n\n", time / (double)ITERATIONS);
  CU_ERROR_CHECK(cuModuleUnload(modules[0]));

  return 0;
}
