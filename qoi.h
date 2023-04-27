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

enum QOI_CHANNELS{
    QOI_RGB = (unsigned char)3, QOI_RGBA = (unsigned char)4
};
/*
 Compress an image to using the qoi format. Returns the size of the compressed image.

 NOTE: "The compression doesn't check how much space there is. The worst case scenario for an image compressed in QOI (which I don't think is ever going to be hit) is ~1.25x the image size + 28 bytes. Make whatever buffer is allocated has at least enough space for the raw image. The function returns the size of the image in bytes, so you can scale your buffer accordingly.
 The maximum space used by QOI can be acquired by calling the qoi_max_compressed_image_size(struct qoi_header) function. This requires a qoi_header that can be obtained through qoi_header_read(const char*);"
 */
extern size_t qoi_compress(const unsigned char in[], unsigned char out[], unsigned char channels, unsigned int w, unsigned int h);


/*
Decompresses a QOI image.

NOTE: Make sure you at least have "width * height * channels" bytes allocated in your out buffer.
 */
extern size_t qoi_decompress(const unsigned char in[], unsigned char out[]);


/*
 Read a qoi_header from an byte buffer.
 Check the success of the function with qoi_header_isvalid();
 */
extern struct qoi_header qoi_header_read(const unsigned char in[]);

extern size_t qoi_max_compressed_image_size(struct qoi_header h);

extern size_t qoi_decompressed_image_size(struct qoi_header h);

extern bool qoi_header_isvalid(struct qoi_header h);

#endif
