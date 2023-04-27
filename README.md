qoi implementation in C. Optimized using gcc vector extensions

### Building
This isn't and won't be available as a standalone program.

To get the shared object:
> make shared

To get the base object file (located in bin):
> make

To test compression on some (2) images:
> make test
> sh testimages.sh
> ./a.out

Clean the testimages afterwards using:
> sh remove_testimages.sh

### Benchmarks

|System Used |                                  |
|------------|----------------------------------|
|CPU         | Ryzen 7 pro 6850U                |
|RAM         | 16GB DDR5 6400 MT/s              |
|OS          | OpenSUSE TW 2022 12 02           |
|GCC         | gcc (SUSE Linux) 12.2.1 20221020 |

RGBA done with https://animecorner.me/wp-content/uploads/2023/03/Frieren.png
The file size used to calculate the MB/s is the size of the RAW data (7.9MB)
Each benchmark is done by running the compression part in a loop 100 times.


compress (RGBA):

|  | -O2               |      | -O3               |      |
|--|-------------------|------|-------------------|------|
|  | Time Average (ms) | MB/s | Time Average (ms) | MB/s |
|--|-------------------|------|-------------------|------|
|1 | 13.80             |  572 | 10.70             |  738 |
|2 |  9.26             |  853 |  9.02             |  875 |
|3 |  9.05             |  872 |  8.32             |  949 |
|4 |  7.67             | 1029 |  7.80             | 1012 |


1. base
2. replaced __builtin_add_overflow in calculate_diff with vector addition and subtraction
3. changed calculate_luma to partially use vectors
4. replaced comparisons with integer comparison in union (for some reason also reduced filesize from 2.6 MB to 1.8 MB)



decompress (RGBA):

|  | -O2               |      | -O3               |      |
|--|-------------------|------|-------------------|------|
|  | Time Average (ms) | MB/s | Time Average (ms) | MB/s |
|--|-------------------|------|-------------------|------|
|1 |                   |      | 10.20             |  774 |
|2 |  9.67             |  816 |  9.26             |  853 |
|3 |  6.54             | 1207 |  6.21             | 1272 |

1. base
2. Removed pixelcounter from if else statements. And only put it in the run length decode. Add 1 to it at the end of the loop
3. Replaced 'channels' variable with a constant (4) because I'll split the function up in an RGB and RGBA one anyways
