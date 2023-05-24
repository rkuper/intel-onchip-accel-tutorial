#include <stdio.h>
#include <stdint.h>
#include "common.h"



void single(int offloads, int xfer_size) {
  void *wq_portal;
	uint64_t *src, *dst;
  int retry = 0;
  uint64_t start, end, offload_start;

	src = (uint64_t*)malloc(xfer_size);
	dst = (uint64_t*)malloc(xfer_size);



  /////////////////////////////////////////////////////////////////////////////
  // Mapping WQ Submission Portal
  start = rdtsc();
  wq_portal = map_wq();
  if (wq_portal == MAP_FAILED)
    return;
  end = rdtsc();
  printf("[time  ] mapped work queue: %lu\n\n", end - start);
  /////////////////////////////////////////////////////////////////////////////



  // Run offloads number of offload examples
  for (int offload = 0; offload < offloads; offload++) {
    if (offload != 0)
      printf("\n\n\n");
    printf("             Offload-%02d\n", offload);
    printf("======================================\n");

    // Set random values to buffers
  	src[0] = rand() % 65536;
  	dst[0] = 0;
    printf("[data  ] before: src=0x%04x, dst=0x%04x\n", (int)src[0], (int)dst[0]);
    cflush((char*)src, xfer_size);
    cflush((char*)dst, xfer_size);
    printf("--------------------------------------\n");



    ///////////////////////////////////////////////////////////////////////////
    // Descriptor Preparation
    struct dsa_completion_record comp __attribute__((aligned(32)));
    struct dsa_hw_desc desc = { };
    start = rdtsc();
    desc.opcode = DSA_OPCODE_MEMMOVE;
    desc.flags = IDXD_OP_FLAG_RCR | IDXD_OP_FLAG_CRAV;
    desc.xfer_size = xfer_size;
    desc.src_addr = (uintptr_t)src;
    desc.dst_addr = (uintptr_t)dst;
    comp.status = 0;
    desc.completion_addr = (uintptr_t)&comp;
    end = rdtsc();
    printf("[time  ] descriptor preparation: %lu\n", end - start);
    ///////////////////////////////////////////////////////////////////////////



    ///////////////////////////////////////////////////////////////////////////
    // Descriptor Submission
    start = offload_start = rdtsc();
  	_mm_sfence();
  	movdir64b(wq_portal, &desc);
    end = rdtsc();
    printf("[time  ] submission: %lu\n", end - start);
    ///////////////////////////////////////////////////////////////////////////



    ///////////////////////////////////////////////////////////////////////////
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
    printf("[time  ] waiting: %lu\n", end - start);
    ///////////////////////////////////////////////////////////////////////////



    // Print Results
    printf("--------------------------------------\n");
    printf("[time  ] full offload: %lu\n", end - offload_start);

    if (comp.status == 1)
      printf("[verify] passed\n");
    else
      printf("[verify] failed: %x\n", comp.status);

    printf("[result] after: src=0x%04x, dst=0x%04x\n", (int)src[0], (int)dst[0]);
  }

  // Cleanup
  munmap(wq_portal, WQ_PORTAL_SIZE);
  free(src);
  free(dst);
  return;
}
