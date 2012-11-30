#ifndef ERROR_H
#define ERROR_H

#include <stdbool.h>
#include <cuda.h>

// Expand and stringify argument
#define STRINGx(x) #x
#define STRING(x) STRINGx(x)

/**
 * CUDA Driver API error handler function type.
 *
 * @param call      the function call that threw the error.
 * @param function  the calling function where the error occurred.
 * @param file      the file the error occurred in.
 * @param line      the line number the error occurred on.
 * @param error     the integer error code.
 */
typedef void (*CUerrorHandler)(const char *, const char *, const char *, int,
                               CUresult, const char * (*)(CUresult));

/**
 * CUDA Driver API error handler (may be NULL).
 */
extern CUerrorHandler cuErrorHandler;

/**
 * CUDA Driver API error string function.
 *
 * @param error  the CUDA driver API error code.
 * @return a human-readable description of the error that occurred.
 */
const char * cuGetErrorString(CUresult);

#define CU_ERROR_CHECK(call) \
  do { \
    CUresult __error__; \
    if ((__error__ = (call)) != CUDA_SUCCESS) { \
      if (cuErrorHandler != NULL) \
        cuErrorHandler(STRING(call), __func__, __FILE__, __LINE__, __error__); \
      return __error__; \
    } \
  } while (false)


#endif
