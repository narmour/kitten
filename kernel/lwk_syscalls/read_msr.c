#include <lwk/read_msr.h>
#include <arch/msr.h>
#include <arch/uaccess.h>
#include <linux/slab.h>



/*
 * This function reads the MSR's listed in read_msr.h, and places them back in user space
 * ret = where to place msr data in user space. assume ret is pre allocated to NUM_MSR
 */
int 
sys_nha_read_msr(uint64_t *ret,int type)
{
    // read the msr's into data.
    //uint64_t *data = kmalloc(sizeof(uint64_t) * NUM_MSR,GFP_USER);
    uint64_t data[NUM_MSR];
    nha_read_msr(data,type);

    // put data into user space
    if(copy_to_user(ret,data,sizeof(uint64_t) * NUM_MSR))
        return -EFAULT;

    //kfree(data);


    return 0;
}

/*
 * ret = where to place read MSR values in user space
 * type = I got lazy and ddidnt want to add multiple syscalls
 * to read a differing amount of MSRS.
 *
 * type0 = read tsc
 * type1 = read msr_list
 */
int
nha_read_msr(uint64_t *ret,int type){
    if (type == 0)
        *ret = rdtsc();
    else if (type ==1){
        uint32_t msr_list[NUM_MSR] = {MSR_APERF,MSR_MPERF,MSR_TSC}; 
        int i;
        for(i =0;i<NUM_MSR;i++){
            rdmsrl_safe(msr_list[i],&ret[i]);
        }
    }
    return 0;
}
