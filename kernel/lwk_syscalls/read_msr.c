#include <lwk/read_msr.h>
#include <arch/msr.h>
#include <arch/uaccess.h>
#include <linux/slab.h>



/*
 * This function reads the MSR's listed in read_msr.h, and places them back in user space
 * ret = where to place msr data in user space. assume ret is pre allocated to NUM_MSR
 */
int 
sys_nha_read_msr(uint64_t *ret)
{
    // read the msr's into data.
    uint64_t *data = kmalloc(sizeof(uint64_t) * NUM_MSR,GFP_USER);
    nha_read_msr(data);

    // put data into user space
    if(copy_to_user(ret,data,sizeof(uint64_t) * NUM_MSR))
        return -EFAULT;

    kfree(data);



    return 0;
}

/*
 * ret = where to place read MSR values in user space
 */
int
nha_read_msr(uint64_t *ret){
    uint32_t msr_list[NUM_MSR] = {MSR_APERF,MSR_MPERF,MSR_TSC,MSR_PKG_JOULES}; 

    uint64_t *test = kmalloc(sizeof(uint64_t),GFP_USER);
    rdmsrl_safe(msr_list[0],test);
    printk("TEST: %llu\n",*test);


    int i;
    for(i =0;i<NUM_MSR;i++){
        printk("MSR: %x\n",msr_list[i]);
        rdmsrl_safe(msr_list[i],&ret[i]);
        printk("%llu\n",ret[i]);
    }

    return 0;
}
