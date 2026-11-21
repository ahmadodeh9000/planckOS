#include "util.h"


int pcl_util_strlen(const char* src) {
    const char* p = src;

    while (*p) 
        p++;
    
    return (p - src);
}