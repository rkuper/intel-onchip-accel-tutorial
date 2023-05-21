#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <linux/idxd.h>
#include <accel-config/libaccel_config.h>
#include <x86intrin.h>
#include <linux/limits.h>
#include "util.h"

#define BATCH_SIZE    4
#define XFER_SIZE  4096
#define BUF_SIZE      4



int main() {
  void *wq_portal;
	uint64_t *(data_buf[3][BUF_SIZE]);
  struct dsa_hw_desc *desc_buf;
  struct dsa_completion_record *comp_buf;
  int retry = 0;
  uint64_t start, end, offload_start;



  // Create Descriptor and Data Buffers
  for (int j = 0; j < BUF_SIZE; j++) {
    for (int i = 0; i < 3; i++) {
      data_buf[i][j] = (uint64_t*)calloc(1, XFER_SIZE);
      if (i == 0)
        data_buf[i][j][0] = (j + 1) * (j + 1);
    }
    printf("[data  ] before: (desc-%02d) src=%4lu, dst=%4lu\n", j, data_buf[0][j][0], data_buf[1][j][0]);
  }
  desc_buf = (struct dsa_hw_desc*)aligned_alloc(64, BATCH_SIZE * sizeof(struct dsa_hw_desc));
  comp_buf = (struct dsa_completion_record*)aligned_alloc(32, BATCH_SIZE * sizeof(struct dsa_completion_record));
  printf("--------------------------------------\n");



  // Mapping WQ Submission Portal
  start = rdtsc();
  wq_portal = map_wq();
  if (wq_portal == MAP_FAILED)
    return 1;
  end = rdtsc();
  printf("[time ] mapped work queue: %lu\n", end - start);



  // Descriptor Preparation
  start = rdtsc();
  for (int i = 0; i < BATCH_SIZE; i++) {
    comp_buf[i].status          = 0;
    desc_buf[i].opcode          = DSA_OPCODE_MEMMOVE;
    desc_buf[i].flags           = IDXD_OP_FLAG_RCR | IDXD_OP_FLAG_CRAV;
    desc_buf[i].xfer_size       = XFER_SIZE;
    desc_buf[i].src_addr        = (uintptr_t)data_buf[0][i];
    desc_buf[i].dst_addr        = (uintptr_t)data_buf[1][i];
    desc_buf[i].completion_addr = (uintptr_t)&(comp_buf[i]);
  }

  // Prepare batch descriptor
  struct dsa_completion_record batch_comp __attribute__((aligned(32)));
  struct dsa_hw_desc batch_desc = { };
  batch_comp.status          = 0;
  batch_desc.opcode          = DSA_OPCODE_BATCH;
  batch_desc.flags           = IDXD_OP_FLAG_RCR | IDXD_OP_FLAG_CRAV;
  batch_desc.desc_count      = BATCH_SIZE;
  batch_desc.desc_list_addr  = (uint64_t)&(desc_buf[0]);
  batch_desc.completion_addr = (uintptr_t)&batch_comp;

  end = rdtsc();
  printf("[time ] descriptor preparation: %lu\n", end - start);



  // Descriptor Submission
  start = offload_start = rdtsc();
	_mm_sfence();
	movdir64b(wq_portal, &batch_desc);
  end = rdtsc();
  printf("[time ] submission: %lu\n", end - start);



  // Check for Completion
  start = rdtsc();
  while (batch_comp.status == 0 && retry++ < MAX_COMP_RETRY) {
    umonitor(&batch_comp);
    if (batch_comp.status == 0) {
      uint64_t delay = __rdtsc() + UMWAIT_DELAY;
      umwait(UMWAIT_STATE_C0_1, delay);
    }
  }
  end = rdtsc();
  printf("[time ] waiting: %lu\n", end - start);



  // Print Results
  printf("--------------------------------------\n");
  printf("[time ] full offload: %lu\n", end - offload_start);

  if (batch_comp.status == 1)
    printf("[verify] passed\n");
  else
    printf("[verify] failed: %x\n", batch_comp.status);

  munmap(wq_portal, WQ_PORTAL_SIZE);

  for (int j = 0; j < BUF_SIZE; j++) {
    printf("[data  ] after: (desc-%02d) src=%4lu, dst=%4lu\n", j, data_buf[0][j][0], data_buf[1][j][0]);
    for (int i = 0; i < 3; i++) {
      free(data_buf[i][j]);
    }
  }
  free(desc_buf);
  free(comp_buf);
}
