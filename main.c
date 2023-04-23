#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>


#include "qoi.h"


int main(){
    FILE* f = fopen("test_rgb.data", "r");
    // FILE* f = fopen("test.data", "r");
	fseek(f, 0l, SEEK_END);
	size_t len = ftell(f);
	unsigned char *data;
	unsigned char *out = malloc(14 + len * 2);
    rewind(f);

	data = mmap(NULL, len, PROT_READ, MAP_SHARED, fileno(f), 0);


    size_t compressed_len = qoi_compress(data, out, 3, 256, 64);
    // size_t compressed_len = qoi_compress(data, out, 4, 1920, 1080);
	FILE* fout = fopen("test.qoi", "w");
    fwrite(out, compressed_len, 1, fout);
}
