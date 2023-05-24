#ifndef _COMMON_H__
#define __COMMON_H__

#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <linux/idxd.h>
#include <accel-config/libaccel_config.h>
#include <x86intrin.h>

#ifndef __GNUC__
#define __asm__ asm
#endif


void single(int offloads, int xfer_size, int buf_size);
void batch(int offloads, int xfer_size, int batch_size, int buf_size);
void async(int offloads, int xfer_size, int buf_size);



static __always_inline uint64_t
rdtsc(void) {
  uint64_t tsc;
  unsigned int dummy;
  tsc = __rdtscp(&dummy);
  __builtin_ia32_lfence();
  return tsc;
}

#define UMWAIT_STATE_C0_2 0
#define UMWAIT_STATE_C0_1 1
#define MAX_COMP_RETRY 2000000000
#define UMWAIT_DELAY 100000
#define WQ_PORTAL_SIZE 4096

static inline void
movdir64b(void *dst, const void *src) {
  asm volatile(".byte 0x66, 0x0f, 0x38, 0xf8, 0x02\t\n"
    : : "a" (dst), "d" (src));
}



static inline unsigned char
umwait(unsigned int state, unsigned long long timeout) {
  uint8_t r;
  uint32_t timeout_low = (uint32_t)timeout;
  uint32_t timeout_high = (uint32_t)(timeout >> 32);
  asm volatile(".byte 0xf2, 0x48, 0x0f, 0xae, 0xf1\t\n"
    "setc %0\t\n" :
    "=r"(r) :
    "c"(state), "a"(timeout_low), "d"(timeout_high));
  return r;
}



static inline void
umonitor(void *addr) {
  asm volatile(".byte 0xf3, 0x48, 0x0f, 0xae, 0xf0" : : "a"(addr));
}



static void * map_wq(void) {
  void *wq_portal;
  struct accfg_ctx *ctx;
  struct accfg_wq *wq;
  struct accfg_device *device;
  char path[PATH_MAX];
  int fd;
  int wq_found;
  accfg_new(&ctx);
  accfg_device_foreach(ctx, device) {
    /* Use accfg_device_(*) functions to select enabled device
    * based on name, numa node
    */
    accfg_wq_foreach(device, wq) {
      if (accfg_wq_get_user_dev_path(wq, path, sizeof(path)))
        continue;
      /* Use accfg_wq_(*) functions select WQ of type
      * ACCFG_WQT_USER and desired mode
      */
      wq_found = accfg_wq_get_type(wq) == ACCFG_WQT_USER &&
      accfg_wq_get_mode(wq) == ACCFG_WQ_DEDICATED;
      /* accfg_wq_get_mode(wq) == ACCFG_WQ_SHARED; */
      if (wq_found)
        break;
    }
    if (wq_found)
      break;
  }
  accfg_unref(ctx);
  if (!wq_found)
    return MAP_FAILED;
  fd = open(path, O_RDWR);
  if (fd < 0)
    return MAP_FAILED;
  wq_portal = mmap(NULL, WQ_PORTAL_SIZE, PROT_WRITE, MAP_SHARED | MAP_POPULATE, fd, 0);
  close(fd);
  return wq_portal;
}



static inline
void clflushopt(volatile void *__p) {
  asm volatile("clflushopt %0" : "+m" (*(volatile char  *)__p));
}

static inline void
cflush(char *buf, uint64_t len) {
  char *b = buf;
  char *e = buf + len;
  for (; b < e; b += 64)
    clflushopt(b);
}

#endif
