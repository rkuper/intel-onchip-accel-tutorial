#include <stdio.h>
#include <string.h>
#include "common.h"

#define OFFLOADS      3
#define XFER_SIZE  4096
#define BATCH_SIZE    4
#define BUF_SIZE      4



int main(int argc, char *argv[]) {

  if (argc < 3) {
    if (argc == 1 || (strcmp("single", argv[1]) == 0))
      single(OFFLOADS, XFER_SIZE, BUF_SIZE);
    else if (strcmp("batch", argv[1]) == 0)
      batch(OFFLOADS, XFER_SIZE, BATCH_SIZE, BUF_SIZE);
    else if (strcmp("async", argv[1]) == 0)
      async(OFFLOADS, XFER_SIZE, BUF_SIZE);
    else
      printf("Unknown offload type: %s\n", argv[1]);
  }

  return 0;
}
