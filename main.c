#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <float.h>
#include <limits.h>

#include "qoi.h"

void test_compress_rgb(){
    FILE* f = fopen("test_rgb.data", "r");
	fseek(f, 0l, SEEK_END);
	size_t len = ftell(f);
	unsigned char *data;
	unsigned char *out = malloc(14 + len * 2);
    rewind(f);

	data = mmap(NULL, len, PROT_READ, MAP_SHARED, fileno(f), 0);

    size_t compressed_len = qoi_compress(data, out, 3, 768, 512);
	FILE* fout = fopen("test_rgb_.qoi", "w");
    fwrite(out, compressed_len, 1, fout);
	fclose(fout);
	fclose(f);
	free(out);
}

void test_compress_rgba(){
	FILE* f = fopen("test_rgba_.data", "r");
	// FILE* f = fopen("test_rgba.data", "r");
	fseek(f, 0l, SEEK_END);
	size_t len = ftell(f);
	unsigned char *data;
	unsigned char *out = malloc(14 + len * 2);
    rewind(f);

	data = mmap(NULL, len, PROT_READ, MAP_SHARED, fileno(f), 0);

    size_t compressed_len = qoi_compress(data, out, 4, 448, 220);
    // size_t compressed_len = qoi_compress(data, out, 4, 1920, 1080);
	FILE* fout = fopen("test_rgba_.qoi", "w");
    fwrite(out, compressed_len, 1, fout);
	fclose(fout);
	fclose(f);
	free(out);
}



void test_decompress_rgba(){
	FILE* f = fopen("test_rgba.qoi", "r");
	fseek(f, 0l, SEEK_END);
	size_t len = ftell(f);
    rewind(f);

	unsigned char *out = malloc(len * 10);
	unsigned char *data = mmap(NULL, len, PROT_READ, MAP_SHARED, fileno(f), 0);

    size_t decompressed_len = qoi_decompress(data, out);
	FILE* fout = fopen("test_rgba_decompress.data", "w");
    fwrite(out, decompressed_len, 1, fout);
	fclose(fout);
	fclose(f);
	free(out);
}

void test_decompress_rgb(){
	FILE* f = fopen("test_rgb.qoi", "r");
	if( f == NULL ){
		fprintf(stderr, "COULDNT OPEN FILE\n");
		exit(-1);
	}
	fseek(f, 0l, SEEK_END);
	size_t len = ftell(f);
    rewind(f);

	unsigned char *out = malloc(len * 10);
	unsigned char *data = malloc(len);
	fread(data, 1, len, f);


	size_t decompressed_len = qoi_decompress(data, out);
	FILE* fout = fopen("test_rgb_decompress.data", "w");
	if( fout == NULL ){
		fprintf(stderr, "COULDNT OPEN FILE\n");
		exit(-1);
	}
    fwrite(out, decompressed_len, 1, fout);
	fclose(fout);
	fclose(f);
	free(out);
}


void test_compress_decompressed_rgba(){
	FILE* f = fopen("test_rgba_decompress.data", "r");
	fseek(f, 0l, SEEK_END);
	size_t len = ftell(f);
	unsigned char *data;
	unsigned char *out = malloc(14 + len * 2);
    rewind(f);

	data = mmap(NULL, len, PROT_READ, MAP_SHARED, fileno(f), 0);

    size_t compressed_len = qoi_compress(data, out, 4, 1920, 1080);
	FILE* fout = fopen("test_rgba_decompress_compressed.qoi", "w");
    fwrite(out, compressed_len, 1, fout);
	fclose(fout);
	fclose(f);
	free(out);
}


void benchmark_decompress_rgba(){
	FILE* f = fopen("test_rgba.qoi", "r");
	size_t amount = 100;
	struct timeval t1, t2;
	double time_total = 0.0;
	double time_elapsed = 0.0;
	double time_min = DBL_MAX, time_max = DBL_MIN;
	size_t decompressed_len;
	fseek(f, 0l, SEEK_END);
	size_t len = ftell(f);
    rewind(f);

	unsigned char *out = malloc(len * 10);
	unsigned char *data = mmap(NULL, len, PROT_READ, MAP_SHARED, fileno(f), 0);

	for( size_t i = 0; i < amount; i++ ){
		gettimeofday(&t1, NULL);
		decompressed_len = qoi_decompress(data, out);
		gettimeofday(&t2, NULL);
		time_elapsed = (t2.tv_sec - t1.tv_sec) + (t2.tv_usec - t1.tv_usec)/1000000.0;
		time_total += time_elapsed;

		if( time_elapsed < time_min ){
			time_min = time_elapsed;
		}
		if( time_elapsed > time_max ){
			time_max = time_elapsed;
		}


	}

	fprintf(stderr, "Total time: %fs\n", time_total);
	fprintf(stderr, "Average time: %fs\n", time_total/amount);
	fprintf(stderr, "Min time: %fs\n", time_min);
	fprintf(stderr, "Max time: %fs\n", time_max);

	FILE* fout = fopen("test_rgba_decompress.data", "w");
    fwrite(out, decompressed_len, 1, fout);
	free(out);
	fclose(fout);
	fclose(f);
}

void benchmark_compress_rgba(){
	FILE* f = fopen("test_rgba.data", "r");
	fseek(f, 0l, SEEK_END);
	size_t len = ftell(f);
	unsigned char *data;
	unsigned char *out = malloc(14 + len * 2);
    rewind(f);
	size_t size;
	size_t amount = 100;
	struct timeval t1, t2;
	double time_total = 0.0;
	double time_elapsed = 0.0;
	double time_min = DBL_MAX, time_max = DBL_MIN;

	data = mmap(NULL, len, PROT_READ, MAP_SHARED, fileno(f), 0);
	for( size_t i = 0; i < amount; i++ ){
		gettimeofday(&t1, NULL);
		size = qoi_compress(data, out, 4, 1920, 1080);
		gettimeofday(&t2, NULL);
		time_elapsed = (t2.tv_sec - t1.tv_sec) + (t2.tv_usec - t1.tv_usec)/1000000.0;
		time_total += time_elapsed;

		if( time_elapsed < time_min ){
			time_min = time_elapsed;
		}
		if( time_elapsed > time_max ){
			time_max = time_elapsed;
		}
	}

	fprintf(stderr, "Total time: %fs\n", time_total);
	fprintf(stderr, "Average time: %fs\n", time_total/amount);
	fprintf(stderr, "Min time: %fs\n", time_min);
	fprintf(stderr, "Max time: %fs\n", time_max);


	FILE* fout = fopen("test_rgba.qoi", "w");
    fwrite(out, size, 1, fout);
	fclose(fout);
	fclose(f);
	free(out);
}



bool test_compress(const char* fin_name, const char *fout_name, unsigned int w, unsigned int h, unsigned char channels){
	FILE* fin = fopen(fin_name, "r");
	FILE* fout = fopen(fout_name, "w");
	unsigned char* out;
	unsigned char* in;
	size_t len;
	size_t outlen;
	struct qoi_header header;

	if( fin == NULL || fout == NULL ){
		fprintf(stderr, "Couldn't open files\n");
		fclose(fin);
		fclose(fout);
		return false;
	}

	fseek(fin, 0l, SEEK_END);
	len = ftell(fin);
	rewind(fin);

	in = mmap(NULL, len, PROT_READ, MAP_SHARED, fileno(fin), 0);
	if( in == NULL ){
		fprintf(stderr, "Mmap failure\n");
		fclose(fin);
		fclose(fout);
		return false;
	}

	header.w = w;
	header.h = h;
	header.channels = channels;

	outlen = qoi_max_compressed_image_size(header);
	// outlen = 100000000;
	out = malloc(outlen);
	if( !out ){
		fprintf(stderr, "Malloc failure\n");
		fclose(fin);
		fclose(fout);
		return false;
	}

	outlen = qoi_compress(in, out, header.channels, header.w, header.h);
	fwrite(out, 1, outlen, fout);

	munmap(in, len);
	free(out);
	fclose(fin);
	fclose(fout);
	return true;
}

bool test_decompress(const char* fin_name, const char *fout_name){
	FILE* fin = fopen(fin_name, "r");
	FILE* fout = fopen(fout_name, "w");
	unsigned char* out;
	unsigned char* in;
	size_t len;
	size_t outlen;
	struct qoi_header header;
	if( fin == NULL || fout == NULL ){
		fprintf(stderr, "Couldn't open files\n");
		fclose(fin);
		fclose(fout);
		return false;
	}

	fseek(fin, 0l, SEEK_END);
	len = ftell(fin);
	rewind(fin);

	in = mmap(NULL, len, PROT_READ, MAP_SHARED, fileno(fin), 0);
	if( in == NULL ){
		fprintf(stderr, "Mmap failure\n");
		fclose(fin);
		fclose(fout);
		return false;
	}


	outlen = qoi_decompressed_image_size(header);
	out = malloc(outlen);
	if( !out ){
		fprintf(stderr, "Malloc failure\n");
		fclose(fin);
		fclose(fout);
		return false;
	}

	outlen = qoi_decompress(in, out);

	fwrite(out, 1, outlen, fout);
	munmap(in, len);
	free(out);
	fclose(fin);
	fclose(fout);
	return true;
}



int main(){
	// test_compress_rgb();
	// test_compress_rgba();
	// test_decompress_rgb();
	// test_decompress_rgba();
	// test_compress_decompressed_rgba();
	// benchmark_compress_rgba();
	// benchmark_decompress_rgba();
	test_compress("test_rgb.data", "test_rgb.qoi", 768, 512, 3);
	// if( test_compress("test_rgb.data", "test_rgb.qoi", 768, 512, 3) ){
		// if( test_decompress("test_rgb.qoi", "test_rgb_res.data") ){
			// test_compress("test_rgb_res.data", "test_rgb_res_res.qoi", 768, 512, 3);
		// }
	// }
	// test_decompress("test_rgba.qoi", "test_rgba_res.data");
	// test_decompress("test_rgba_.qoi", "test_rgba_res_.data");
}
