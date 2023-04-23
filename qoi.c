#include <endian.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "qoi.h"

typedef unsigned char ubyte;
typedef unsigned char vec4u8 __attribute__((vector_size(4)));
typedef uint16_t vec4u16 __attribute__((vector_size(8)));

/*
benchmarks
  | -O2                      | -O3
--+-------------------+------+-------------------+------+
  | Time Average (ms) | MB/s | Time Average (ms) | MB/s |
--+-------------------+------+-------------------+------+
1 | 1380              |  572 | 1070              |  738 |
2 |  926              |  853 |  902              |  875 |
3 |  905              |  872 |  832              |  949 |
4 |  767              | 1029 |  780              | 1012 |


1. base
2. replaced __builtin_add_overflow in calculate_diff with vector addition and subtraction
3. changed calculate_luma to partially use vectors
4. replaced comparisons with integer comparison in union (for some reason also reduced filesize from 2.6 MB to 1.8 MB)

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


struct qoi_header{
    char magic[4];
    uint32_t w;
    uint32_t h;
    uint8_t channels;
    uint8_t colorspace;
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
struct qoi_header read_header(ubyte *out){
    struct qoi_header header;
    memcpy(&header, out, 14);
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
    *(out+1) = p->r<<4 | p->b;
}


static
size_t calculate_index(union Pixel *restrict p){
    const uint16_t r = 3 * (uint16_t)p->r;
    const uint16_t g = 5 * (uint16_t)p->g;
    const uint16_t b = 7 * (uint16_t)p->b;
    const uint16_t a = 11 * (uint16_t)p->a;
    return (r + g + b + a)&63;
}

static
size_t calculate_index_rgb(union Pixel *restrict p){
    const uint16_t r = 3 * (uint16_t)p->r;
    const uint16_t g = 5 * (uint16_t)p->g;
    const uint16_t b = 7 * (uint16_t)p->b;
    return (r + g + b)&63;
}


static
bool calculate_diff(union Pixel *restrict dest, union Pixel *restrict prev, union Pixel *restrict cur){
#if defined(__x86_64__)
    dest->vec = cur->vec - prev->vec + 2;
#else
    __builtin_sub_overflow(cur->r , prev->r, &dest->r);
    __builtin_sub_overflow(cur->g , prev->g, &dest->g);
    __builtin_sub_overflow(cur->b , prev->b, &dest->b);

    __builtin_add_overflow(dest->r , 2, &dest->r);
    __builtin_add_overflow(dest->g , 2, &dest->g);
    __builtin_add_overflow(dest->b , 2, &dest->b);
#endif
    if( dest->r < 4 && dest->g < 4 && dest->b < 4 ){
        return true;
    }
    return false;
}


static
bool calculate_luma(union Pixel *restrict dest, union Pixel *restrict prev, union Pixel *restrict cur){
    const int16_t dg = (int16_t)cur->g - (int16_t)prev->g;

    if( dg >= -32 && dg <= 31 ){
        dest->vec = cur->vec - prev->vec - (ubyte)dg + 8;

        // int16_t dr_dg = ((int16_t)cur->r - (int16_t)prev->r) - dg + 8;
        // int16_t db_dg = ((int16_t)cur->b - (int16_t)prev->b) - dg + 8;
        // if( dr_dg >= 0 && dr_dg < 16 && db_dg >= 0 && db_dg < 16 ){
        if( dest->r < 16 && dest->b < 16 ){
            // dest->r = (ubyte)dr_dg;
            dest->g = (ubyte)(dg + 32);
            // dest->b = (ubyte)db_dg;
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
    const size_t pixel_count = be32toh(settings.w) * be32toh(settings.h);

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

    while(pixel_counter < pixel_count) {
        memcpy(&cur_pixel, in + pixel_counter*4, 4);
        pixel_counter += 1;

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
            else if( prev_pixel.a == cur_pixel.a ){
                out[out_pos] = QOI_OP_RGB;
                memcpy(&out[out_pos+1], &cur_pixel, 3);
                out_pos += 4;
            }
            else{
                out[out_pos] = QOI_OP_RGBA;
                memcpy(&out[out_pos+1], &cur_pixel, 4);
                out_pos += 5;
            }
            memcpy(&pixels[pixels_index], &cur_pixel, 4);
            memcpy(&prev_pixel, &cur_pixel, 4);
        }

    }

    memset(&out[out_pos], 0, 7);
    out[out_pos + 7] = 1;

    return out_pos;
}




static
size_t compress_image_rgb(const ubyte* in, ubyte* out, struct qoi_header settings){
    const size_t pixel_count = be32toh(settings.w) * be32toh(settings.h);

    union Pixel pixels[64];
    union Pixel cur_pixel = {{0, 0, 0, 255}};
    union Pixel prev_pixel = {{0, 0, 0, 255}};
    union Pixel diff_pixel;
    union Pixel luma_pixel;

    size_t pixel_counter = 0;
    size_t out_pos = 0;
    ubyte run_length = 0;
    size_t pixels_index;

    memset(pixels, 0, sizeof(pixels));

    while(pixel_counter < pixel_count) {
        memcpy(&cur_pixel, in + pixel_counter*3, 3);
        pixel_counter += 1;

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
            pixels_index = calculate_index_rgb(&cur_pixel);


            if( pixels_equal(&cur_pixel, &pixels[pixels_index]) ){
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
            memcpy(&pixels[pixels_index], &cur_pixel, 3);
            memcpy(&prev_pixel, &cur_pixel, 3);
        }

    }
    memset(&out[out_pos], 0, 7);
    out[out_pos + 7] = 1;

    return out_pos;
}


size_t qoi_compress(const unsigned char* in, unsigned char* out, unsigned int channels, size_t w, size_t h){
    struct qoi_header header = write_header(out, channels, w, h);
    size_t size;

    if( channels == 4 ){
        size = compress_image_rgba(in, out + 14, header) + 14;
    }else{
        size = compress_image_rgb(in, out + 14, header) + 14;
    }

    return size;
}



size_t decompress_image(const ubyte* in, ubyte* out, struct qoi_header header){
    const size_t channels = header.channels;
    const size_t pixel_count = header.w * header.h;
    size_t pixel_index = 0;

    union Pixel pixels[64];
    union Pixel prev_pixel = {{0, 0, 0, 255}};
    union Pixel diff_pixel = {{0, 0, 0, 0}};
    union Pixel temp_pixel;


    size_t in_index = 0;
    size_t out_index = 0;

    size_t pixels_index;
    size_t run_length;
    ubyte b;

    memset(pixels, 0, sizeof(pixels));

    while( pixel_index < pixel_count ){
        b = in[in_index];


        if( (b & QOI_OP_INDEX) == QOI_OP_INDEX ){
            pixels_index = b & 63;
            memcpy(&out[out_index], &pixels[pixels_index], channels);
            out_index += channels;
            in_index += 1;
        }
        else if(b == QOI_OP_RGB){
            memcpy(&out[out_index], &in[in_index+1], 3);
            if( channels == 4 ) out[out_index+3] = prev_pixel.a;
            out_index += channels;
            in_index += 4;
        }
        else if(b == QOI_OP_RGBA){
            memcpy(&out[out_index], &in[in_index+1], 4);
            out_index += 4;
            in_index += 5;
        }
        else if( (b & QOI_OP_LUMA) == QOI_OP_LUMA){
            diff_pixel.g = (b & 0x0F) - 32;
            diff_pixel.r = (in[in_index + 1] & 0xF0) - 8 + diff_pixel.g;
            diff_pixel.b = (in[in_index + 1] & 0x0F) - 8 + diff_pixel.g;
            temp_pixel.vec = diff_pixel.vec + prev_pixel.vec;

            memcpy(&out[out_index], &temp_pixel, channels);

            out_index += channels;
            in_index += 2;
        }
        else if( (b & QOI_OP_DIFF) == QOI_OP_DIFF){
            diff_pixel.r = (b>>4)&3;
            diff_pixel.g = (b>>2)&3;
            diff_pixel.b = b&3;
            temp_pixel.vec = prev_pixel.vec + diff_pixel.vec - 2;

            memcpy(&out[out_index], &temp_pixel, channels);
            out_index += channels;
            pixel_index += 1;
            in_index += 1;
        }
        else if( (b & QOI_OP_RUN) == QOI_OP_RUN){
            run_length = (b & 63)+ 1;
            pixel_index += run_length;
            out_index += channels * run_length;
            while(run_length--){
                memcpy(&out[out_index], &prev_pixel, channels);
            }

            in_index += 1;
        }

    }



    return out_index;
}



size_t decompress(const unsigned char* in, unsigned char* out){
    struct qoi_header header = read_header(out);
    size_t size = 14 + decompress_image(in, out, header);

    return size;
}
