#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <float.h>
#include <limits.h>

#include "qoi.h"

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

unsigned char *ftobuff(const char* fin_name, size_t *len){
	FILE* fin = fopen(fin_name, "r");
	if( fin == NULL ){
		fprintf(stderr, "Couldn't open file %s\n", fin_name);
		if(fin != NULL) fclose(fin);
		return NULL;
	}

	fseek(fin, 0l, SEEK_END);
	*len = ftell(fin);
	rewind(fin);

	unsigned char* in = mmap(NULL, *len, PROT_READ, MAP_SHARED, fileno(fin), 0);
	if( in == NULL ){
		fprintf(stderr, "Mmap failure\n");
		fclose(fin);
		return NULL;
	}
    fclose(fin);
	return in;
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
		if(fin != NULL) fclose(fin);
		if(fout != NULL)fclose(fout);
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
		if(fin != NULL) fclose(fin);
		if(fout != NULL) fclose(fout);
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


#define IMAGES 3
int main(){
    int i = 0;
    double time_total = 0.0;
    double time_elapsed;
    double min = DBL_MAX, max = 0.0;
    struct timeval t1, t2;

    int image_counter = 0;
	int runs[] = {100, 10, 20};
	// int runs[] = {1, 1, 1};
    const unsigned int w[] = {1920, 5120, 2000};
    const unsigned int h[] = {1080, 2880, 1335};
    const unsigned char channels[] = {QOI_RGBA, QOI_RGBA, QOI_RGB};
    const char* finname[] = {"Frieren.rgba", "rgba_big.rgba", "forest.rgb"};
    const char* foutname[] = {"Frieren.qoi", "rgba_big.qoi", "forest.qoi"};
    const char* foutdecompressedname[] = {"Frieren_d.rgba", "rgba_big_d.rgba", "forest_d.rgb"};

    for( image_counter = 0; image_counter < IMAGES; image_counter++ ){
        time_total = 0.0;
        min = DBL_MAX;
        max = 0.0;


        size_t in_len;
        unsigned char* in = ftobuff(finname[image_counter], &in_len);
        if( in == NULL ){
            return -1;
        }
        unsigned char* out = malloc(in_len * 2);
        if( out == NULL ){
            return -1;
        }
        size_t out_len = w[image_counter] * h[image_counter] * channels[image_counter];

        FILE* fout = fopen(foutname[image_counter], "w");
        if( fout == NULL ){
            fprintf(stderr, "Couldnt open %s\n", foutname[image_counter]);
            return -1;
        }

        for( i = 0; i < runs[image_counter]; i++ ){
            gettimeofday(&t1, NULL);

            out_len = qoi_compress(in, out, channels[image_counter], w[image_counter], h[image_counter]);

            gettimeofday(&t2, NULL);

            time_elapsed = (t2.tv_sec  - t1.tv_sec) + ((t2.tv_usec - t1.tv_usec)/1000000.0);
            time_total += time_elapsed;

            if( time_elapsed < min ){
                min = time_elapsed;
            }
            if( time_elapsed > max ){
                max = time_elapsed;
            }
        }

        fwrite(out, 1, out_len, fout);

        fprintf(stderr, "===== IMAGE %d: %s =====\n", image_counter + 1, finname[image_counter] );
        fprintf(stderr, "==== Average ====\n %f seconds\n", time_total / runs[image_counter] );
        fprintf(stderr, "==== Total ====\n %f seconds\n", time_total);
        fprintf(stderr, "==== Min ====\n %f seconds\n", min);
        fprintf(stderr, "==== Max ====\n %f seconds\n", max);
        fprintf(stderr, "==== MB/s ====\n %f\n", ((w[image_counter] * h[image_counter] * channels[image_counter])/(time_total/runs[image_counter])) / 1000000.0);
        fprintf(stderr, "==== Pixels/s ====\n %f\n", ((w[image_counter] * h[image_counter])/(time_total/runs[image_counter])));
        fprintf(stderr, "==== Size ====\n %f MB\n\n\n", (w[image_counter] * h[image_counter] * channels[image_counter]) / 1000000.0 );
        fclose(fout);
        free(out);
        munmap(in, in_len);
    }


    for( image_counter = 0; image_counter < IMAGES; image_counter++ ){
        time_total = 0.0;
        min = DBL_MAX;
        max = 0.0;


        size_t in_len;
        unsigned char* in = ftobuff(foutname[image_counter], &in_len);
        if( in == NULL ){
            return -1;
        }
        unsigned char* out = malloc(in_len * 20);
        if( out == NULL ){
            return -1;
        }
        size_t out_len;

        FILE* fout = fopen(foutdecompressedname[image_counter], "w");
        if( fout == NULL ){
            fprintf(stderr, "Couldnt open %s\n", foutname[image_counter]);
            return -1;
        }

        for( i = 0; i < runs[image_counter]; i++ ){
            gettimeofday(&t1, NULL);

            out_len = qoi_decompress(in, out);

            gettimeofday(&t2, NULL);

            time_elapsed = (t2.tv_sec  - t1.tv_sec) + ((t2.tv_usec - t1.tv_usec)/1000000.0);
            time_total += time_elapsed;

            if( time_elapsed < min ){
                min = time_elapsed;
            }
            if( time_elapsed > max ){
                max = time_elapsed;
            }
        }

        fwrite(out, 1, out_len, fout);

        fprintf(stderr, "===== IMAGE %d: %s =====\n", image_counter + 1, finname[image_counter] );
        fprintf(stderr, "==== Average ====\n %f seconds\n", time_total / runs[image_counter] );
        fprintf(stderr, "==== Total ====\n %f seconds\n", time_total);
        fprintf(stderr, "==== Min ====\n %f seconds\n", min);
        fprintf(stderr, "==== Max ====\n %f seconds\n", max);
        fprintf(stderr, "==== MB/s ====\n %f\n", ((w[image_counter] * h[image_counter] * channels[image_counter])/(time_total/runs[image_counter])) / 1000000.0);
        fprintf(stderr, "==== Pixels/s ====\n %f\n", ((w[image_counter] * h[image_counter])/(time_total/runs[image_counter])));
        fprintf(stderr, "==== Size ====\n %f MB\n\n\n", (w[image_counter] * h[image_counter] * channels[image_counter]) / 1000000.0 );
        fclose(fout);
        free(out);
        munmap(in, in_len);
    }

    return 0;
}
