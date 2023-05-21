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

#define XFER_SIZE  4096



int main() {
  void *wq_portal;
	uint64_t *src, *dst;
  int retry = 0;
  uint64_t start, end, offload_start;

	src = (uint64_t*)calloc(1, XFER_SIZE);
	dst = (uint64_t*)calloc(1, XFER_SIZE);
	src[0] = 2468;
	dst[0] = 1357;
  printf("[data  ] before: src=%4lu, dst=%4lu\n", src[0], dst[0]);
  printf("--------------------------------------\n");



  // Mapping WQ Submission Portal
  start = rdtsc();
  wq_portal = map_wq();
  if (wq_portal == MAP_FAILED)
    return 1;
  end = rdtsc();
  printf("[time ] mapped work queue: %lu\n", end - start);



  // Descriptor Preparation
  struct dsa_completion_record comp __attribute__((aligned(32)));
  struct dsa_hw_desc desc = { };
  start = rdtsc();
  desc.opcode = DSA_OPCODE_MEMMOVE;
  desc.flags = IDXD_OP_FLAG_RCR | IDXD_OP_FLAG_CRAV;
  desc.xfer_size = XFER_SIZE;
  desc.src_addr = (uintptr_t)src;
  desc.dst_addr = (uintptr_t)dst;
  comp.status = 0;
  desc.completion_addr = (uintptr_t)&comp;
  end = rdtsc();
  printf("[time ] descriptor preparation: %lu\n", end - start);



  // Descriptor Submission
  start = offload_start = rdtsc();
	_mm_sfence();
	movdir64b(wq_portal, &desc);
  end = rdtsc();
  printf("[time ] submission: %lu\n", end - start);



  // Check for Completion
  start = rdtsc();
  while (comp.status == 0 && retry++ < MAX_COMP_RETRY) {
    umonitor(&comp);
    if (comp.status == 0) {
      uint64_t delay = __rdtsc() + UMWAIT_DELAY;
      umwait(UMWAIT_STATE_C0_1, delay);
    }
  }
  end = rdtsc();
  printf("[time ] waiting: %lu\n", end - start);



  // Print Results
  printf("--------------------------------------\n");
  printf("[time ] full offload: %lu\n", end - offload_start);

  if (comp.status == 1)
    printf("[verify] passed\n");
  else
    printf("[verify] failed: %x\n", comp.status);

  printf("[result] after: src=%4lu, dst=%4lu\n", src[0], dst[0]);
  munmap(wq_portal, WQ_PORTAL_SIZE);
}
