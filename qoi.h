#ifndef QOI_H
#define QOI_H
#include <stdint.h>
#include <stddef.h>


extern size_t qoi_compress(const unsigned char* in, unsigned char* out, unsigned int channels, size_t w, size_t h);
extern size_t qoi_decompress(const unsigned char* in, unsigned char* out);

#endif
