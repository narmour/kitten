
#ifndef _LWK_READ_MSR_H
#define _LWK_READ_MSR_H
#include <lwk/types.h>
#include <arch/msr.h>

#define MSR_APERF 0xe8
#define MSR_MPERF 0xe7
#define MSR_TSC 0x10
#define MSR_PKG_JOULES 0x611
#define NUM_MSR 4

/* user space syscall prototype */
extern int
nha_read_msr(uint64_t *ret);


/* syscall handler prototye */
extern int
sys_nha_read_msr(uint64_t *ret);

#endif
