
#ifndef _LWK_READ_MSR_H
#define _LWK_READ_MSR_H
#include <lwk/types.h>
#include <arch/msr.h>

/* user space syscall prototype */
extern int
nha_read_msr(uint64_t *ret);


/* syscall handler prototye */
extern int
sys_nha_read_msr(uint64_t *ret);

#endif
