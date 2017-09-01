#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <zstd.h>

#include <fcntl.h>
#include "ldm.h"
#include "zstd.h"

// #define DECOMPRESS_AND_VERIFY

/* Compress file given by fname and output to oname.
 * Returns 0 if successful, error code otherwise.
 *
 * This adds a header from LDM_writeHeader to the beginning of the output.
 *
 * This might seg fault if the compressed size is > the decompress
 * size due to the mmapping and output file size allocated to be the input size
 * The compress function should check before writing or buffer writes.
 */
static int compress(const char *fname, const char *oname) {
  int fdin, fdout;
  struct stat statbuf;
  char *src, *dst;
  size_t maxCompressedSize, compressedSize;

  struct timeval tv1, tv2;
  double timeTaken;


  /* Open the input file. */
  if ((fdin = open(fname, O_RDONLY)) < 0) {
    perror("Error in file opening");
    return 1;
  }

  /* Open the output file. */
  if ((fdout = open(oname, O_RDWR | O_CREAT | O_TRUNC, (mode_t)0600)) < 0) {
    perror("Can't create output file");
    return 1;
  }

  /* Find the size of the input file. */
  if (fstat (fdin, &statbuf) < 0) {
    perror("Fstat error");
    return 1;
  }

  maxCompressedSize = (statbuf.st_size + LDM_HEADER_SIZE);

  // Handle case where compressed size is > decompressed size.
  // TODO: The compress function should check before writing or buffer writes.
  maxCompressedSize += statbuf.st_size / 255;

  ftruncate(fdout, maxCompressedSize);

  /* mmap the input file. */
  if ((src = mmap(0, statbuf.st_size, PROT_READ,  MAP_SHARED, fdin, 0))
          == (caddr_t) - 1) {
      perror("mmap error for input");
      return 1;
  }

  /* mmap the output file. */
  if ((dst = mmap(0, maxCompressedSize, PROT_READ | PROT_WRITE,
                  MAP_SHARED, fdout, 0)) == (caddr_t) - 1) {
      perror("mmap error for output");
      return 1;
  }

  gettimeofday(&tv1, NULL);

  compressedSize = LDM_HEADER_SIZE +
      LDM_compress(src, statbuf.st_size,
                   dst + LDM_HEADER_SIZE, maxCompressedSize);

  gettimeofday(&tv2, NULL);

  // Write the header.
  LDM_writeHeader(dst, compressedSize, statbuf.st_size);

  // Truncate file to compressedSize.
  ftruncate(fdout, compressedSize);

  printf("%25s : %10lu -> %10lu - %s \n", fname,
         (size_t)statbuf.st_size, (size_t)compressedSize, oname);
  printf("Compression ratio: %.2fx --- %.1f%%\n",
         (double)statbuf.st_size / (double)compressedSize,
         (double)compressedSize / (double)(statbuf.st_size) * 100.0);

  timeTaken = (double) (tv2.tv_usec - tv1.tv_usec) / 1000000 +
              (double) (tv2.tv_sec - tv1.tv_sec),

  printf("Total compress time = %.3f seconds, Average scanning speed: %.3f MB/s\n",
         timeTaken,
         ((double)statbuf.st_size / (double) (1 << 20)) / timeTaken);

  // Close files.
  close(fdin);
  close(fdout);
  return 0;
}

#ifdef DECOMPRESS_AND_VERIFY
/* Decompress file compressed using LDM_compress.
 * The input file should have the LDM_HEADER followed by payload.
 * Returns 0 if succesful, and an error code otherwise.
 */
static int decompress(const char *fname, const char *oname) {
  int fdin, fdout;
  struct stat statbuf;
  char *src, *dst;
  U64 compressedSize, decompressedSize;
  size_t outSize;

  /* Open the input file. */
  if ((fdin = open(fname, O_RDONLY)) < 0) {
    perror("Error in file opening");
    return 1;
  }

  /* Open the output file. */
  if ((fdout = open(oname, O_RDWR | O_CREAT | O_TRUNC, (mode_t)0600)) < 0) {
    perror("Can't create output file");
    return 1;
  }

  /* Find the size of the input file. */
  if (fstat (fdin, &statbuf) < 0) {
    perror("Fstat error");
    return 1;
  }

  /* mmap the input file. */
  if ((src = mmap(0, statbuf.st_size, PROT_READ,  MAP_SHARED, fdin, 0))
          == (caddr_t) - 1) {
      perror("mmap error for input");
      return 1;
  }

  /* Read the header. */
  LDM_readHeader(src, &compressedSize, &decompressedSize);

  ftruncate(fdout, decompressedSize);

  /* mmap the output file */
  if ((dst = mmap(0, decompressedSize, PROT_READ | PROT_WRITE,
                  MAP_SHARED, fdout, 0)) == (caddr_t) - 1) {
      perror("mmap error for output");
      return 1;
  }

  outSize = LDM_decompress(
      src + LDM_HEADER_SIZE, statbuf.st_size - LDM_HEADER_SIZE,
      dst, decompressedSize);
  printf("Ret size out: %zu\n", outSize);

  close(fdin);
  close(fdout);
  return 0;
}

/* Compare two files.
 * Returns 0 iff they are the same.
 */
static int compare(FILE *fp0, FILE *fp1) {
  int result = 0;
  while (result == 0) {
    char b0[1024];
    char b1[1024];
    const size_t r0 = fread(b0, 1, sizeof(b0), fp0);
    const size_t r1 = fread(b1, 1, sizeof(b1), fp1);

    result = (int)r0 - (int)r1;

    if (0 == r0 || 0 == r1) break;

    if (0 == result) result = memcmp(b0, b1, r0);
  }
  return result;
}

/* Verify the input file is the same as the decompressed file. */
static int verify(const char *inpFilename, const char *decFilename) {
  FILE *inpFp, *decFp;

  if ((inpFp = fopen(inpFilename, "rb")) == NULL) {
    perror("Could not open input file\n");
    return 1;
  }

  if ((decFp = fopen(decFilename, "rb")) == NULL) {
    perror("Could not open decompressed file\n");
    return 1;
  }

  printf("verify : %s <-> %s\n", inpFilename, decFilename);
  {
    const int cmp = compare(inpFp, decFp);
    if(0 == cmp) {
      printf("verify : OK\n");
    } else {
      printf("verify : NG\n");
      return 1;
    }
  }

	fclose(decFp);
	fclose(inpFp);
  return 0;
}
#endif

int main(int argc, const char *argv[]) {
  const char * const exeName = argv[0];
  char inpFilename[256] = { 0 };
  char ldmFilename[256] = { 0 };
  char decFilename[256] = { 0 };

  if (argc < 2) {
    printf("Wrong arguments\n");
    printf("Usage:\n");
    printf("%s FILE\n", exeName);
    return 1;
  }

  snprintf(inpFilename, 256, "%s", argv[1]);
  snprintf(ldmFilename, 256, "%s.ldm", argv[1]);
  snprintf(decFilename, 256, "%s.ldm.dec", argv[1]);

 	printf("inp = [%s]\n", inpFilename);
	printf("ldm = [%s]\n", ldmFilename);
	printf("dec = [%s]\n", decFilename);

  /* Compress */
  {
    if (compress(inpFilename, ldmFilename)) {
        printf("Compress error\n");
        return 1;
    }
  }

#ifdef DECOMPRESS_AND_VERIFY
  /* Decompress */
  {
    struct timeval tv1, tv2;
    gettimeofday(&tv1, NULL);
    if (decompress(ldmFilename, decFilename)) {
        printf("Decompress error\n");
        return 1;
    }
    gettimeofday(&tv2, NULL);
    printf("Total decompress time = %f seconds\n",
          (double) (tv2.tv_usec - tv1.tv_usec) / 1000000 +
          (double) (tv2.tv_sec - tv1.tv_sec));
  }
  /* verify */
  if (verify(inpFilename, decFilename)) {
    printf("Verification error\n");
    return 1;
  }
#endif
  return 0;
}
