/*
qoi-c: a "fast" C implementation of the qoi format
Copyright (C) 2023  atiedebee

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <endian.h>

#include "qoi.h"

typedef unsigned char ubyte;
typedef unsigned char vec4u8 __attribute__((vector_size(4))) __attribute__((aligned(4)));

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
ubyte calculate_index(union Pixel *restrict p){
    const ubyte r = 3 * p->r;
    const ubyte g = 5 * p->g;
    const ubyte b = 7 * p->b;
    const ubyte a = 11 * p->a;
    return (r + g + b + a) & 63;
}

static
bool calculate_diff_true_diff(union Pixel *restrict dest, const union Pixel *restrict true_diff){
    dest->vec = true_diff->vec + 2;
    vec4u8 bools = dest->vec < (vec4u8){4, 4, 4};
    return bools[0] & bools[1] & bools[2];
}


static
bool calculate_luma_true_diff(union Pixel *restrict dest, const union Pixel *restrict true_diff){
    dest->vec = true_diff->vec - (vec4u8){true_diff->vec[1], 0, true_diff->vec[1]} + (vec4u8){8, 32, 8};
    vec4u8 vals = dest->vec < (vec4u8){16, 64, 16};
    return vals[0] & vals[1] & vals[2];
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
    union Pixel true_diff;
    union Pixel diff_pixel;
    union Pixel luma_pixel;

    size_t pixel_counter = 0;
    size_t out_pos = 0;
    ubyte run_length = 0;
    ubyte pixels_index;

    memset(pixels, 0, sizeof(pixels));
    pixels[calculate_index(&prev_pixel)].i = prev_pixel.i;

    while(pixel_counter < pixel_count) {
        memcpy(&cur_pixel, &in[pixel_counter * 4], 4);

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
            }
            else if( cur_pixel.a == prev_pixel.a ){
                true_diff.vec = cur_pixel.vec - prev_pixel.vec;
                if( calculate_diff_true_diff(&diff_pixel, &true_diff) ){
                    write_qoi_diff(&out[out_pos], &diff_pixel);
                }
                else if( calculate_luma_true_diff(&luma_pixel, &true_diff) ){
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

    union Pixel pixels[64];
    union Pixel prev_pixel = {{0, 0, 0, 255}};
    union Pixel cur_pixel = {{0, 0, 0, 255}};
    union Pixel diff_pixel;
    union Pixel luma_pixel;
    union Pixel true_diff;

    size_t pixel_counter = 0;
    size_t out_pos = 0;
    ubyte run_length = 0;
    size_t pixels_index;

    memset(pixels, 0, sizeof(pixels));

    while(pixel_counter < pixel_count)
    {
        memcpy(&cur_pixel, &in[pixel_counter * 3], 3);
        if( pixels_equal(&cur_pixel, &prev_pixel) ){
            run_length += 1;
            if( run_length == 62 ){
                write_qoi_run(&out[out_pos], run_length);
                out_pos += 1;
                run_length = 0;
            }
        }else
        {
            if( run_length > 0 ){
                write_qoi_run(&out[out_pos], run_length);
                out_pos += 1;
                run_length = 0;
            }

            pixels_index = calculate_index(&cur_pixel);

            if( cur_pixel.i == pixels[pixels_index].i ) {
                write_qoi_index(&out[out_pos], (ubyte)pixels_index);
                out_pos += 1;
            }
            else{
                true_diff.vec = cur_pixel.vec - prev_pixel.vec;
                if( calculate_diff_true_diff(&diff_pixel, &true_diff) ){
                    write_qoi_diff(&out[out_pos], &diff_pixel);
                    out_pos += 1;
                }
                else if( calculate_luma_true_diff(&luma_pixel, &true_diff) ){
                    write_qoi_luma(&out[out_pos], &luma_pixel);
                    out_pos += 2;
                }
                else {
                    out[out_pos] = QOI_OP_RGB;
                    memcpy(&out[out_pos+1], &cur_pixel, 3);
                    out_pos += 4;
                }
            }
            prev_pixel.i = cur_pixel.i;
            pixels[pixels_index].i = cur_pixel.i;
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
        pixels_index = calculate_index(&cur_pixel);

        pixels[pixels_index].i = cur_pixel.i;
        prev_pixel.i = cur_pixel.i;
    }

    return out_index;
}


size_t qoi_compress(const unsigned char in[], unsigned char out[], unsigned char channels, unsigned int w, unsigned int h){
    struct qoi_header header;
    size_t size;

    if( in == NULL || out == NULL || channels < 3 || channels > 4 || w == 0 || h == 0 ){
        return 0;
    }

    header = write_header(out, channels, w, h);

    if( channels == 4 ){
        size = compress_image_rgba(in, out + 14, header) + 14;
    }else{
        size = compress_image_rgb(in, out + 14, header) + 14;
    }

    return size;
}


size_t qoi_decompress(const unsigned char in[], unsigned char out[]){
    struct qoi_header header;
    size_t size;

    if( in == NULL || out == NULL ){
        return 0;
    }

    header = read_header(in);

    if(!qoi_header_isvalid(header) ){
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
    if( in == NULL ){
        return (struct qoi_header){.w = 0, .h = 0};
    }
    return read_header(in);
}


bool qoi_header_isvalid(struct qoi_header h){
    return h.w != 0 && h.h != 0 && (h.channels == 3 || h.channels == 4) && memcmp(h.magic, "qoif", 4) == 0;
}
