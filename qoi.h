#ifndef QOI_H
#define QOI_H
#include <stddef.h>


extern size_t qoi_compress(const unsigned char* in, unsigned char* out, unsigned int channels, size_t w, size_t h);

#endif
