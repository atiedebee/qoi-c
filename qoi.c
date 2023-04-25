#include <endian.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "qoi.h"

typedef unsigned char ubyte;
typedef unsigned char vec4u8 __attribute__((vector_size(4)));
typedef signed char vec4s8 __attribute__((vector_size(4)));
typedef uint16_t vec4u16 __attribute__((vector_size(8)));
typedef int16_t vec4i16 __attribute__((vector_size(8)));

/*
benchmarks

== System Used ==
CPU         Ryzen 7 pro 6850U
RAM         16GB DDR5 6400 MT/s
OS          OpenSUSE TW 2022 12 02
GCC         gcc (SUSE Linux) 12.2.1 20221020

RGBA done with https://animecorner.me/wp-content/uploads/2023/03/Frieren.png
The file size used to calculate the MB/s is the size of the RAW data (7.9MB)
Each benchmark is done by running the compression part in a loop 100 times.


compress (RGBA):
  | -O2                      | -O3
--+-------------------+------+-------------------+------+
  | Time Average (ms) | MB/s | Time Average (ms) | MB/s |
--+-------------------+------+-------------------+------+
1 | 13.80             |  572 | 10.70             |  738 |
2 |  9.26             |  853 |  9.02             |  875 |
3 |  9.05             |  872 |  8.32             |  949 |
4 |  7.67             | 1029 |  7.80             | 1012 |


1. base
2. replaced __builtin_add_overflow in calculate_diff with vector addition and subtraction
3. changed calculate_luma to partially use vectors
4. replaced comparisons with integer comparison in union (for some reason also reduced filesize from 2.6 MB to 1.8 MB)



decompress (RGBA):
  | -O2                      | -O3
--+-------------------+------+-------------------+------+
  | Time Average (ms) | MB/s | Time Average (ms) | MB/s |
--+-------------------+------+-------------------+------+
1 |                   |      | 10.20             |  774 |
2 |  9.67             |  816 |  9.26             |  853 |
3 |  6.54             | 1207 |  6.21             | 1272 |

1. base
2. Removed pixelcounter from if else statements. And only put it in the run length decode. Add 1 to it at the end of the loop
3. Replaced 'channels' variable with a constant (4) because I'll split the function up in an RGB and RGBA one anyways
*/


union Pixel{
    struct{
        ubyte r;
        ubyte g;
        ubyte b;
        ubyte a;
    };
    vec4u8 vec;
    uint32_t i;
};


enum QOI_OPS {
    QOI_OP_RGB = 0xFE,
    QOI_OP_RGBA = 0xFF,
    QOI_OP_INDEX = 0x00,
    QOI_OP_DIFF = 0x01<<6,
    QOI_OP_LUMA = 0x02<<6,
    QOI_OP_RUN = 0x03<<6,
};


static
uint32_t header_get_w(struct qoi_header h){
    return be32toh(h.w);
}

static
uint32_t header_get_h(struct qoi_header h){
    return be32toh(h.h);
}

static
struct qoi_header write_header(ubyte *out, ubyte channels, size_t w, size_t h){
    struct qoi_header header = {
        .magic = "qoif",
        .w = htobe32(w),
        .h = htobe32(h),
        .channels = channels,
        .colorspace = 0
    };
    memcpy(out, &header, 14);
    return header;
}


static
struct qoi_header read_header(const ubyte *in){
    struct qoi_header header;
    memcpy(&header, in, 14);
    header.w = be32toh(header.w);
    header.h = be32toh(header.h);
    return header;
}


static
void write_qoi_run(ubyte* out, ubyte amount){
    *out = QOI_OP_RUN;
    *out |= (amount - 1);
}


static
void write_qoi_index(ubyte* out, ubyte index){
    *out = QOI_OP_INDEX;
    *out |= index;
}


static
void write_qoi_diff(ubyte* out, union Pixel *p){
    *out = QOI_OP_DIFF;
    *out |= p->r<<4 | p->g<<2 | p->b;
}


static
void write_qoi_luma(ubyte* out, union Pixel *p){
    *out = QOI_OP_LUMA;
    *out |= p->g;
    *(out+1) = (p->r << 4) | p->b;
}


static
size_t calculate_index(union Pixel *restrict p){
    const uint16_t r = 3 * (uint16_t)p->r;
    const uint16_t g = 5 * (uint16_t)p->g;
    const uint16_t b = 7 * (uint16_t)p->b;
    const uint16_t a = 11 * (uint16_t)p->a;
    return (r + g + b + a) & 63;
}


static
size_t calculate_index_rgb(union Pixel *restrict p){
    return (p->r * 3 + p->g * 5 + p->b * 7 + p->a * 11)&63;
}


static
bool calculate_diff(union Pixel *restrict dest, union Pixel *restrict prev, union Pixel *restrict cur){
    dest->vec = cur->vec - prev->vec + 2;
    if( dest->r < 4 && dest->g < 4 && dest->b < 4 ){
        return true;
    }
    return false;
}


__attribute__((unused))
static
bool calculate_diff_clean(union Pixel *restrict dest, union Pixel *restrict prev, union Pixel *restrict cur){
    const int16_t r = (int16_t)cur->r - (int16_t)prev->r;
    const int16_t g = (int16_t)cur->g - (int16_t)prev->g;
    const int16_t b = (int16_t)cur->b - (int16_t)prev->b;
    if( r >= -2 && r <= 1 &&
        g >= -2 && g <= 1 &&
        b >= -2 && b <= 1
    ){
        dest->r = (ubyte)(r + 2);
        dest->g = (ubyte)(g + 2);
        dest->b = (ubyte)(b + 2);
        return true;
    }
    return false;
}


static
bool calculate_luma(union Pixel *restrict dest, union Pixel *restrict prev, union Pixel *restrict cur){
    const int16_t dg = (int16_t)cur->g - (int16_t)prev->g;

    dest->vec = cur->vec - prev->vec - (ubyte)dg + 8;

    if( dg >= -32 && dg <= 31 && dest->r < 16 && dest->b < 16 ){
        dest->g = (ubyte)(dg + 32);
        return true;
    }
    return false;
}


__attribute__((unused))
static
bool calculate_luma_clean(union Pixel *restrict dest, union Pixel *restrict prev, union Pixel *restrict cur){
    const int16_t dg = (int16_t)cur->g - (int16_t)prev->g;

    if( dg >= -32 && dg <= 31 ){
        const int16_t dr_dg = ((int16_t)cur->r - (int16_t)prev->r) - dg;
        const int16_t db_dg = ((int16_t)cur->b - (int16_t)prev->b) - dg;;
        if( dr_dg >= -8 && dr_dg <= 7 &&
            db_dg >= -8 && db_dg <= 7
        ){
            dest->r = (ubyte)(dr_dg + 8);
            dest->g = (ubyte)(dg + 32);
            dest->b = (ubyte)(db_dg + 8);
            return true;
        }
    }
    return false;
}


static
bool pixels_equal(const union Pixel *a, const union Pixel *b){
    return a->i == b->i;
}


static
size_t compress_image_rgba(const ubyte* in, ubyte* out, struct qoi_header settings){
    const size_t pixel_count = header_get_w(settings) * header_get_h(settings);

    union Pixel pixels[64];
    union Pixel cur_pixel;
    union Pixel prev_pixel = {{0, 0, 0, 255}};
    union Pixel diff_pixel;
    union Pixel luma_pixel;

    size_t pixel_counter = 0;
    size_t out_pos = 0;
    ubyte run_length = 0;
    size_t pixels_index;

    memset(pixels, 0, sizeof(pixels));
    pixels[calculate_index_rgb(&prev_pixel)].i = prev_pixel.i;

    while(pixel_counter < pixel_count) {
        memcpy(&cur_pixel, &in [pixel_counter*4], 4);

        if( pixels_equal(&cur_pixel, &prev_pixel) ){
            run_length += 1;
            if( run_length == 62 ){
                write_qoi_run(&out[out_pos], run_length);
                out_pos += 1;
                run_length = 0;
            }
        }
        else{
            if( run_length > 0 ){
                write_qoi_run(&out[out_pos], run_length);
                out_pos += 1;
                run_length = 0;
            }

            pixels_index = calculate_index(&cur_pixel);

            if( pixels_equal(&cur_pixel, &pixels[pixels_index]) ){
                write_qoi_index(&out[out_pos], pixels_index);
                // out_pos += 1;
            }
            else if( cur_pixel.a == prev_pixel.a ){
                if( calculate_diff(&diff_pixel, &prev_pixel, &cur_pixel) ){
                    write_qoi_diff(&out[out_pos], &diff_pixel);
                    // out_pos += 1;
                }
                else if( calculate_luma(&luma_pixel, &prev_pixel, &cur_pixel) ){
                    write_qoi_luma(&out[out_pos], &luma_pixel);
                    out_pos += 1;
                }
                else{
                    out[out_pos] = QOI_OP_RGB;
                    memcpy(&out[out_pos+1], &cur_pixel, 3);
                    out_pos += 3;
                }
            }
            else{
                out[out_pos] = QOI_OP_RGBA;
                memcpy(&out[out_pos+1], &cur_pixel, 4);
                out_pos += 4;
            }
            out_pos += 1;
            pixels[pixels_index].i = cur_pixel.i;
            prev_pixel.i = cur_pixel.i;
        }
        pixel_counter += 1;
    }

    if( run_length > 0 ){
        write_qoi_run(&out[out_pos], run_length);
        out_pos += 1;
    }

    memset(&out[out_pos], 0, 7);
    out[out_pos + 7] = 1;
    out_pos += 8;
    return out_pos;
}


static
size_t compress_image_rgb(const ubyte* in, ubyte* out, struct qoi_header settings){
    const size_t pixel_count = header_get_w(settings) * header_get_h(settings);

    union Pixel pixels[64] = {0};
    union Pixel prev_pixel = {{0, 0, 0, 255}};
    union Pixel cur_pixel = {.i = prev_pixel.i};
    union Pixel diff_pixel;
    union Pixel luma_pixel;

    size_t pixel_counter = 0;
    size_t out_pos = 0;
    ubyte run_length = 0;
    size_t pixels_index;

    memset(pixels, 0, sizeof(pixels));

    while(pixel_counter < pixel_count)
    {
        memcpy(&cur_pixel, &in[pixel_counter * 3], 3);


        if( memcmp(&cur_pixel, &prev_pixel, 3) == 0 ){
            run_length += 1;
            if( run_length == 62 ){
                write_qoi_run(&out[out_pos], run_length);
                out_pos += 1;
                run_length = 0;
            }
        }
        else{
            if( run_length > 0 ){
                write_qoi_run(&out[out_pos], run_length);
                out_pos += 1;
                run_length = 0;
            }

            pixels_index = calculate_index_rgb(&cur_pixel);

            if( memcmp(&cur_pixel, &pixels[pixels_index], 3) == 0 ){
                write_qoi_index(&out[out_pos], pixels_index);
                out_pos += 1;
            }
            else if( calculate_diff(&diff_pixel, &prev_pixel, &cur_pixel) ){
                write_qoi_diff(&out[out_pos], &diff_pixel);
                out_pos += 1;
            }
            else if( calculate_luma(&luma_pixel, &prev_pixel, &cur_pixel) ){
                write_qoi_luma(&out[out_pos], &luma_pixel);
                out_pos += 2;
            }
            else{
                out[out_pos] = QOI_OP_RGB;
                memcpy(&out[out_pos+1], &cur_pixel, 3);
                out_pos += 4;
            }
            pixels[pixels_index].i = cur_pixel.i;
            prev_pixel.i = cur_pixel.i;
        }

        pixel_counter += 1;
    }
    if( run_length > 0 ){
        write_qoi_run(&out[out_pos], run_length);
        out_pos += 1;
        run_length = 0;
    }

    memset(&out[out_pos], 0, 7);
    out[out_pos + 7] = 1;
    out_pos += 8;
    return out_pos;
}


static
size_t decompress_image_rgba(const ubyte* in, ubyte* out, struct qoi_header header){
    const size_t pixel_count = header.w * header.h;
    size_t pixel_counter = 0;

    union Pixel pixels[64];
    union Pixel prev_pixel = {{0, 0, 0, 255}};
    union Pixel diff_pixel = {{0, 0, 0, 0}};
    union Pixel cur_pixel;


    size_t in_index = 0;
    size_t out_index = 0;

    size_t pixels_index;
    size_t run_length;
    ubyte b;

    memset(pixels, 0, sizeof(pixels));


    while( pixel_counter < pixel_count ){
        b = in[in_index];

        if(b == QOI_OP_RGB){
            memcpy(&out[out_index], &in[in_index+1], 3);
            out[out_index+3] = prev_pixel.a;
            memcpy(&cur_pixel, &out[out_index], 4);

            out_index += 4;
            in_index += 4;
        }
        else if(b == QOI_OP_RGBA){
            memcpy(&out[out_index], &in[in_index+1], 4);
            memcpy(&cur_pixel, &out[out_index], 4);

            out_index += 4;
            in_index += 5;
        }
        else if((b & QOI_OP_RUN) == QOI_OP_RUN){
            run_length = (b & 63) + 1;
            pixel_counter += run_length - 1;

            while(run_length--){
                memcpy(&out[out_index], &prev_pixel, 4);
                out_index += 4;
            }

            in_index += 1;
        }
        else if( (b & QOI_OP_LUMA) == QOI_OP_LUMA){
            diff_pixel.g = (b & 63) - 32;
            diff_pixel.r = ((in[in_index + 1]>>4)& 0x0F) + diff_pixel.g - 8;
            diff_pixel.b = (in[in_index + 1] & 0x0F)     + diff_pixel.g - 8;

            cur_pixel.vec = prev_pixel.vec + diff_pixel.vec;

            memcpy(&out[out_index], &cur_pixel, 4);

            out_index += 4;
            in_index += 2;
        }
        else if( (b & QOI_OP_DIFF) == QOI_OP_DIFF){
            diff_pixel.r = (b>>4)&3;
            diff_pixel.g = (b>>2)&3;
            diff_pixel.b = b&3;
            cur_pixel.vec = prev_pixel.vec + diff_pixel.vec + (vec4u8){-2, -2, -2, 0};

            memcpy(&out[out_index], &cur_pixel, 4);

            out_index += 4;
            in_index += 1;
        }
        else{
            pixels_index = b & 63;
            memcpy(&out[out_index], &pixels[pixels_index], 4);
            cur_pixel.i = pixels[pixels_index].i;

            out_index += 4;
            in_index += 1;
        }

        pixel_counter += 1;
        pixels_index = calculate_index(&cur_pixel);

        pixels[pixels_index].i = cur_pixel.i;
        prev_pixel.i = cur_pixel.i;
    }

    return out_index;
}

static
size_t decompress_image_rgb(const ubyte* in, ubyte* out, struct qoi_header header){
    const size_t pixel_count = header.w * header.h;
    size_t pixel_counter = 0;

    union Pixel pixels[64];
    union Pixel prev_pixel = {{0, 0, 0, 255}};
    union Pixel diff_pixel = {{0, 0, 0, 0}};
    union Pixel cur_pixel = prev_pixel;

    size_t in_index = 0;
    size_t out_index = 0;

    size_t pixels_index;
    size_t run_length;
    ubyte b;

    memset(pixels, 0, sizeof(pixels));

    while( pixel_counter < pixel_count ){
        b = in[in_index];

        if( b == QOI_OP_RGB ){
            memcpy(&out[out_index], &in[in_index+1], 3);
            memcpy(&cur_pixel, &out[out_index], 3);

            out_index += 3;
            in_index += 4;
        }
        else if((b & QOI_OP_RUN) == QOI_OP_RUN){
            run_length = (b & 63) + 1;
            pixel_counter += run_length - 1;

            while(run_length--){
                memcpy(&out[out_index], &prev_pixel, 3);
                out_index += 3;
            }

            in_index += 1;
        }
        else if( (b & QOI_OP_LUMA) == QOI_OP_LUMA){
            diff_pixel.g = (b & 63) - 32;
            diff_pixel.r = ((in[in_index + 1]>>4)& 0x0F) + diff_pixel.g - 8;
            diff_pixel.b = (in[in_index + 1] & 0x0F)     + diff_pixel.g - 8;

            cur_pixel.vec = prev_pixel.vec + diff_pixel.vec;

            memcpy(&out[out_index], &cur_pixel, 3);

            out_index += 3;
            in_index += 2;
        }
        else if( (b & QOI_OP_DIFF) == QOI_OP_DIFF){
            diff_pixel.r = (b>>4)&3;
            diff_pixel.g = (b>>2)&3;
            diff_pixel.b = b&3;
            cur_pixel.vec = prev_pixel.vec + diff_pixel.vec + (vec4u8){-2, -2, -2, 0};

            memcpy(&out[out_index], &cur_pixel, 3);

            out_index += 3;
            in_index += 1;
        }
        else{// QOI_OP_INDEX
            pixels_index = b;
            memcpy(&out[out_index], &pixels[pixels_index], 3);
            cur_pixel.i = pixels[pixels_index].i;

            out_index += 3;
            in_index += 1;
        }

        pixel_counter += 1;
        pixels_index = calculate_index_rgb(&cur_pixel);

        pixels[pixels_index].i = cur_pixel.i;
        prev_pixel.i = cur_pixel.i;
    }

    return out_index;
}


size_t qoi_compress(const unsigned char* in, unsigned char* out, unsigned char channels, unsigned int w, unsigned int h){
    struct qoi_header header = write_header(out, channels, w, h);
    size_t size;

    if( channels == 4 ){
        size = compress_image_rgba(in, out + 14, header) + 14;
    }else{
        size = compress_image_rgb(in, out + 14, header) + 14;
    }

    return size;
}


size_t qoi_decompress(const unsigned char* in, unsigned char* out){
    struct qoi_header header = read_header(in);
    size_t size;

    if(!qoi_header_valid(header) ){
        fprintf(stderr, "Invalid header\n");
        return 0;
    }

    if( header.channels == 3 ){
        size = decompress_image_rgb(in + 14, out, header);
    }else{
        size = decompress_image_rgba(in + 14, out, header);
    }
    return size;
}


size_t qoi_max_compressed_image_size(struct qoi_header h){
    return h.w*h.h*(h.channels+1) + 14 + 8;
}


size_t qoi_decompressed_image_size(struct qoi_header h){
    return h.w * h.h * h.channels;
}


struct qoi_header qoi_header_read(const unsigned char* in){
    return read_header(in);
}


bool qoi_header_valid(struct qoi_header h){
    return h.w != 0 && h.h != 0 && (h.channels == 3 || h.channels == 4) && memcmp(h.magic, "qoif", 4) == 0;
}
