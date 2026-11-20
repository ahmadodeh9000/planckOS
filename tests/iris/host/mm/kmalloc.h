/* kmalloc.h (host shim) - the C library's heap stands in for the kernel's. */
#ifndef kmalloc_h__
#define kmalloc_h__
#include <stdlib.h>
#define kmalloc malloc
#define kfree free
#endif
