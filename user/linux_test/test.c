#include <unistd.h>
#include <stdio.h>
#include <pthread.h>
#include <stdint.h>
#include <inttypes.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include "./msr.h"
#include <fcntl.h>
#include <signal.h>
#include <sys/time.h>



#define INTERVAL 1 //miliseconds
/* global used for tsc delta */
uint64_t prev; 
uint64_t max_non_turbo;

void read_tsc(uint64_t *ret){
    /* setup ioctl to msr module to read tsc */
    struct msr_batch_array ba;
    int numops = 1;
    ba.numops = numops;
    ba.ops = (struct msr_batch_op *)malloc(sizeof(struct msr_batch_op) * numops);

    /* TSC batch op */
    struct msr_batch_op *op = &ba.ops[0];
    op->cpu = 0;
    op->isrdmsr = 1;
    op->msr = 0x10;
    op->msrdata = 0;


    int bfd = open("/dev/cpu/msr_batch",O_RDWR);
    int res;
    if(bfd >0)
        res = ioctl(bfd,X86_IOC_MSR_BATCH,&ba);
    else
        printf("error opening msr_batch\n");

    *ret = ba.ops[0].msrdata;

    close(bfd);
}



/* read msr on cpu into ret */
void read_msr(int cpu,uint64_t msr,uint64_t *ret){
    char msr_file[64];
    sprintf(msr_file,"/dev/cpu/%i/msr_safe",cpu);
    
    int fd = open(msr_file,O_RDONLY);
    pread(fd,ret,sizeof(*ret),msr);
    close(fd);
}


void sig_func(){
    uint64_t curr;
    read_tsc(&curr);

    uint64_t tsc_delta = curr - prev;
    double tsc_seconds_delta = (double)tsc_delta/(max_non_turbo * 100000000);
    printf("SECONDS DELTA: %f\n",tsc_seconds_delta);
    printf("\n");
    prev = curr;


}



int main(){
    read_tsc(&prev);// prev for tsc deltas

    /* get max non turbo ratio to get seconds from clock cycles */
    uint64_t platform_info;
    read_msr(0,0xCE,&platform_info);
    max_non_turbo = (platform_info >>8) & 0xFF; // m_n_t is the 8 bits(15:8) in 0xCE




    
    if(signal(SIGALRM,sig_func) == SIG_ERR){
        printf("sig err\n");
        return -1;
    }

    struct itimerval interval;
    interval.it_value.tv_sec = INTERVAL/1000; // seconds from millis
    interval.it_value.tv_usec = (INTERVAL *1000) % (1000 * 1000); //micro seconds
    interval.it_interval = interval.it_value;

    if(setitimer(ITIMER_REAL,&interval,NULL) ==-1){
        printf("timer err\n");
        return -1;
    }
    while(1){
        sleep(100);
    }
}
