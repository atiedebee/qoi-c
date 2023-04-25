#ifndef QOI_H
#define QOI_H
#include <stdint.h>
#include <stddef.h>


struct qoi_header{
     char magic[4];
    uint32_t w;
    uint32_t h;
    uint8_t channels;
    uint8_t colorspace;
};


extern size_t qoi_compress(const unsigned char* in, unsigned char* out, unsigned char channels, uint32_t w, uint32_t h);
extern size_t qoi_decompress(const unsigned char* in, unsigned char* out);

extern struct qoi_header qoi_header_read(const unsigned char* in);
extern size_t qoi_max_compressed_image_size(struct qoi_header h);
extern size_t qoi_decompressed_image_size(struct qoi_header h);
extern bool qoi_header_valid(struct qoi_header h);

#endif
