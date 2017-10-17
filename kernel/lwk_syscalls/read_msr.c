#include <lwk/read_msr.h>
#include <arch/msr.h>

int 
sys_nha_read_msr(uint64_t *ret)
{
    return nha_read_msr(ret);
}

int
nha_read_msr(uint64_t *ret){
    return rdtsc();
}
