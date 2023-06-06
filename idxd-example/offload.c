#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "common.h"
#include <linux/idxd.h>



int main(int argc, char *argv[]) {
  void *wq_portal;
  uint64_t start;
  struct dsa_hw_desc *desc_buf;
  struct dsa_completion_record *comp_buf;
  uint64_t *(data_buf[2][BUF_SIZE]);
  int status;

  if (argc > 2) {
    printf("[ERROR] Incorrect format: sudo ./idxd_offload <[single, batch, or async]>\n");
    return 1;
  }

  // Create Descriptor and Data Buffers
  desc_buf = (struct dsa_hw_desc*)aligned_alloc(64, BUF_SIZE * sizeof(struct dsa_hw_desc));
  comp_buf = (struct dsa_completion_record*)aligned_alloc(32, BUF_SIZE * sizeof(struct dsa_completion_record));
  for (int i = 0; i < 2; i++) {
    for (int j = 0; j < BUF_SIZE; j++)
      data_buf[i][j] = (uint64_t *)malloc(XFER_SIZE);
  }


  /////////////////////////////////////////////////////////////////////////////
  // Mapping WQ Submission Portal
  start = rdtsc();

  wq_portal = map_wq();
  if (wq_portal == MAP_FAILED)
    return 1;

  printf("[time  ] mapped work queue: %lu\n\n", rdtsc() - start);
  /////////////////////////////////////////////////////////////////////////////



  // Run EXA<PLES number of offload examples
  for (int example; example < EXAMPLES; example++) {
    if (example != 0)
      printf("\n\n\n");
    printf("             Example-%02d\n", example);
    printf("======================================\n");

    // Set random values to buffers and flush them from cache
    for (int j = 0; j < BUF_SIZE; j++) {
      for (int i = 0; i < 2; i++)
        data_buf[i][j][0] = rand() % 65536;
      printf("[data  ] before-%02d: src=%04x, dst=%04x\n", j, (int)data_buf[0][j][0], (int)data_buf[1][j][0]);
      cflush((char*)data_buf[0][j], XFER_SIZE);
      cflush((char*)data_buf[1][j], XFER_SIZE);
    }
    printf("--------------------------------------\n");



    ///////////////////////////////////////////////////////////////////////////
    // Begin offloading
    if (argc == 1 || (strcmp("single", argv[1]) == 0)) {
      printf("[info  ] running %s offload\n", (argc == 1) ? "single" : argv[1]);
      status = single(data_buf, desc_buf, comp_buf, wq_portal);
    } else if (strcmp("batch", argv[1]) == 0) {
      printf("[info  ] running %s offload\n", argv[1]);
      status = batch(data_buf, desc_buf, comp_buf, wq_portal);
    } else if (strcmp("async", argv[1]) == 0) {
      printf("[info  ] running %s offload\n", argv[1]);
      status = async(data_buf, desc_buf, comp_buf, wq_portal);
    } else {
      printf("Unknown offload type: %s\n", argv[1]);
    }
    ///////////////////////////////////////////////////////////////////////////



    // Verify results
    printf("--------------------------------------\n");
    if (status != 1)
      printf("[verify] failed: %x\n", status);
    else
      printf("[verify] passed\n");

    for (int j = 0; j < BUF_SIZE; j++)
      printf("[data  ] after-%02d: src=%04x, dst=%04x\n", j, (int)data_buf[0][j][0], (int)data_buf[1][j][0]);
  }

  // Cleanup
  munmap(wq_portal, WQ_PORTAL_SIZE);
  for (int i = 0; i < 1; i++)
    for (int j = 0; j < BUF_SIZE; j++)
      free(data_buf[i][j]);
  free(desc_buf);
  free(comp_buf);

  return 0;
}
