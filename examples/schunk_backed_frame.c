/*
  Copyright (C) 2018  Francesc Alted
  http://blosc.org
  License: BSD 3-Clause (see LICENSE.txt)

  Example program demonstrating use of the Blosc filter from C code.

  To compile this program:

  $ gcc schunk_backed_frame.c -o schunk_backed_frame -lblosc

  To run:

  $ ./schunk_backed_frame
  Blosc version info: 2.0.0a6.dev ($Date:: 2018-05-18 #$)
  Compression ratio: 381.5 MB -> 9.5 MB (40.2x)
  Compression time: 0.619 s, 616.3 MB/s
  Decompression time: 0.247 s, 1541.4 MB/s
  Successful roundtrip data <-> schunk (frame based) !

 */

#include <stdio.h>
#include <assert.h>
#include <blosc.h>

#define KB  1024.
#define MB  (1024*KB)
#define GB  (1024*MB)

#define CHUNKSIZE (1000 * 1000)
#define NCHUNKS 100
#define NTHREADS 4


int main() {
  static int32_t data[CHUNKSIZE];
  static int32_t data_dest[CHUNKSIZE];
  size_t isize = CHUNKSIZE * sizeof(int32_t);
  int dsize = 0;
  int64_t nbytes, cbytes;
  blosc2_schunk* schunk;
  int i, nchunk;
  int nchunks;
  blosc_timestamp_t last, current;
  double ttotal;

  for (i = 0; i < CHUNKSIZE; i++) {
    data[i] = i;
  }

  printf("Blosc version info: %s (%s)\n",
         BLOSC_VERSION_STRING, BLOSC_VERSION_DATE);

  // Create a simple in-memory frame
  blosc2_frame* frame = &(blosc2_frame) {
          .sdata = NULL,
          .fname = NULL,
          .len = 0,
          .maxlen = 0,
  };

  /* Create a super-chunk container */
  schunk = blosc2_new_schunk((blosc2_cparams) {
          .typesize = sizeof(int32_t),
          .filters[BLOSC_MAX_FILTERS - 1] = BLOSC_SHUFFLE,
          .compcode = BLOSC_LZ4,
          .clevel = 9,
          .nthreads = NTHREADS,
  }, (blosc2_dparams) {
          .nthreads = NTHREADS,
  }, frame);

  blosc_set_timestamp(&last);
  for (nchunk = 1; nchunk <= NCHUNKS; nchunk++) {
    for (i = 0; i < CHUNKSIZE; i++) {
      data[i] = i * nchunk;
    }
    nchunks = blosc2_schunk_append_buffer(schunk, data, isize);
    assert(nchunks == nchunk);
  }
  /* Gather some info */
  nbytes = schunk->nbytes;
  cbytes = schunk->cbytes;
  blosc_set_timestamp(&current);
  ttotal = blosc_elapsed_secs(last, current);
  printf("Compression ratio: %.1f MB -> %.1f MB (%.1fx)\n",
         nbytes / MB, cbytes / MB, (1. * nbytes) / cbytes);
  printf("Compression time: %.3g s, %.1f MB/s\n",
         ttotal, nbytes / (ttotal * MB));

  /* Retrieve and decompress the chunks (0-based count) */
  blosc_set_timestamp(&last);
  for (nchunk = 0; nchunk < NCHUNKS; nchunk++) {
    dsize = blosc2_schunk_decompress_chunk(schunk, nchunk, data_dest, isize);
    if (dsize < 0) {
      printf("Decompression error.  Error code: %d\n", dsize);
      return dsize;
    }
  }
  blosc_set_timestamp(&current);
  ttotal = blosc_elapsed_secs(last, current);
  printf("Decompression time: %.3g s, %.1f MB/s\n",
         ttotal, nbytes / (ttotal * MB));

  /* Check integrity of the first chunk */
  blosc2_schunk_decompress_chunk(schunk, 0, data_dest, isize);
  for (i = 0; i < CHUNKSIZE; i++) {
    if (data_dest[i] != (float)i) {
      printf("Decompressed data differs from original %d, %d!\n",
             i, data_dest[i]);
      return -1;
    }
  }

  printf("Successful roundtrip data <-> schunk (frame based) !\n");

  /* Free resources */
  /* Destroy the super-chunk */
  blosc2_free_schunk(schunk);

  return 0;
}
