#include <stdio.h>
#include <stdint.h>
#include "common.h"



void single(int offloads, int xfer_size, int buf_size) {
  void *wq_portal;
	uint64_t *(data_buf[3][buf_size]);
  struct dsa_hw_desc *desc_buf;
  struct dsa_completion_record *comp_buf;
  int retry;
  uint64_t start, end, prep, submit, wait, offload_start;
  int verify_check;

  // Create Descriptor and Data Buffers
  desc_buf = (struct dsa_hw_desc*)aligned_alloc(64, buf_size * sizeof(struct dsa_hw_desc));
  comp_buf = (struct dsa_completion_record*)aligned_alloc(32, buf_size * sizeof(struct dsa_completion_record));
  for (int j = 0; j < buf_size; j++)
    for (int i = 0; i < 3; i++)
      data_buf[i][j] = (uint64_t*)malloc(xfer_size);



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
  for (int offload; offload < offloads; offload++) {
    prep = submit = wait = 0;
    verify_check = 1;
    if (offload != 0)
      printf("\n\n\n");
    printf("             Offload-%02d\n", offload);
    printf("======================================\n");

    // Set random values to buffers
    for (int j = 0; j < buf_size; j++) {
      for (int i = 0; i < 3; i++)
        data_buf[i][j][0] = rand() % 65536;
      printf("[data  ] before-%02d: src=%04x, dst=%04x\n", j, (int)data_buf[0][j][0], (int)data_buf[1][j][0]);
      cflush((char*)data_buf[0][j], xfer_size);
      cflush((char*)data_buf[1][j], xfer_size);
    }
    printf("--------------------------------------\n");


    // Submit 4 seprate single offloads in a row
    offload_start = rdtsc();
    for (int i = 0; i < buf_size; i++) {

      ///////////////////////////////////////////////////////////////////////////
      // Descriptor Preparation
      start = rdtsc();
      comp_buf[i].status          = 0;
      desc_buf[i].opcode          = DSA_OPCODE_MEMMOVE;
      desc_buf[i].flags           = IDXD_OP_FLAG_RCR | IDXD_OP_FLAG_CRAV;
      desc_buf[i].xfer_size       = xfer_size;
      desc_buf[i].src_addr        = (uintptr_t)data_buf[0][i];
      desc_buf[i].dst_addr        = (uintptr_t)data_buf[1][i];
      desc_buf[i].completion_addr = (uintptr_t)&(comp_buf[i]);
      end = rdtsc();
      prep += end - start;
      ///////////////////////////////////////////////////////////////////////////



      ///////////////////////////////////////////////////////////////////////////
      // Descriptor Submission
      start = rdtsc();
	    _mm_sfence();
	    movdir64b(wq_portal, &desc_buf[i]);
      end = rdtsc();
      submit += end - start;
      ///////////////////////////////////////////////////////////////////////////



      ///////////////////////////////////////////////////////////////////////////
      // Check for Completion
      retry = 0;
      start = rdtsc();
      while (comp_buf[i].status == 0 && retry++ < MAX_COMP_RETRY) {
        umonitor(&(comp_buf[i]));
        if (comp_buf[i].status == 0) {
          uint64_t delay = __rdtsc() + UMWAIT_DELAY;
          umwait(UMWAIT_STATE_C0_1, delay);
        }
      }
      end = rdtsc();
      wait += end - start;
      ///////////////////////////////////////////////////////////////////////////
    }



    // Print Results
    printf("[time  ] descriptor preparation: %lu\n", prep);
    printf("[time  ] submission: %lu\n", submit);
    printf("[time  ] waiting: %lu\n", wait);
    printf("--------------------------------------\n");
    printf("[time  ] full offload: %lu\n", end - offload_start);

    for (int i = 0; i < buf_size; i++) {
      if (comp_buf[i].status != 1) {
        verify_check = comp_buf[i].status;
        break;
      }
    }
    if (verify_check != 1)
      printf("[verify] failed: %x\n", verify_check);
    else
      printf("[verify] passed\n");

    for (int j = 0; j < buf_size; j++)
      printf("[data  ] after-%02d: src=%04x, dst=%04x\n", j, (int)data_buf[0][j][0], (int)data_buf[1][j][0]);
  }


  // Cleanup
  munmap(wq_portal, WQ_PORTAL_SIZE);
  for (int j = 0; j < buf_size; j++)
    for (int i = 0; i < 3; i++)
      free(data_buf[i][j]);
  free(desc_buf);
  free(comp_buf);
  return;
}
