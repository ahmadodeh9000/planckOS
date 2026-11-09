/*
 * common.h - basic types and OS-wide constants.
 *
 * Every header includes this. The <std*.h> headers used here come with the
 * compiler and need no C library.
 */
#ifndef common_h__
#define common_h__

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define OS_NAME     "planckOS"
#define OS_VERSION  "1.0.0"
#define KERNEL_NAME "Iris"

#endif
