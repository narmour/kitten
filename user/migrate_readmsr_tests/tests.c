#define LINUX 0
#define KITTEN 1
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sched.h>
#include <pthread.h>
#include <lwk/liblwk.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "msr.h"
#include <sys/ioctl.h>

#define SAMPLES 100// number of samples to get per test
#define ITERS 10 // number of runs per test taking SAMPLES
void *migrate_func(void *args);

void stat_data(uint64_t data[][SAMPLES]){
    int i,j;
    for(i =0;i<ITERS;i++){
        for(j =0;j<SAMPLES;j++){
            print("%llu ",data[i][j]);
        }
        print("\n");
    }
}

void read_tsc_test(){
    if(LINUX){
        int i,j;

        // read tsc from sysfs made by msr_safe
        uint64_t sysfs_timestamps[ITERS][SAMPLES];
        int fd = open("/dev/cpu/0/msr_safe",O_RDWR);
        if(fd <1){
            printf("ERROR OPENING MSR_SAFE\n");
            return;
        }
        for(i =0;i<ITERS;i++)
            for(j =0;j<SAMPLES;j++){
                pread(fd,&sysfs_timestamps[i][j],sizeof(sysfs_timestamps[i][j]),0x10);
            }

        // read tsc from clock_gettime(CLOCK_REALTIME) aka gettimeofday()
        uint64_t gettime_timestamps[ITERS][SAMPLES];
        for(i =0;i<ITERS;i++)
            for(j =0;j<SAMPLES;j++){
                struct timespec ts;
                clock_gettime(CLOCK_REALTIME,&ts);
                gettime_timestamps[i][j] = (ts.tv_sec * 1000000000) + ts.tv_nsec;
            }
    }
    else if (KITTEN){
        uint64_t timestamps[ITERS][SAMPLES];
        int i,j;
        for(i =0;i<ITERS;i++)
            for(j =0;j<SAMPLES;j++){
                nha_read_msr(&timestamps[i][j],0);
            }
        stat_data(timestamps);
    }
}

double current_time(){
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME,&ts);
    return (double)(ts.tv_sec * 1000000000) + ts.tv_nsec;
}

/*
 * Spawns a thread and moves it between cpu's.
 * Keep track of the time it takes to do so
 */
void thread_migrate_test(){
    int i,j;
    uint64_t thread_delta_timestamps[ITERS][SAMPLES];

    pthread_t thread;
    pthread_create(&thread,NULL,migrate_func,thread_delta_timestamps);
    pthread_join(thread,NULL);
}

/*
 * Pthread function. Takes pre allocated 2d array to store timestamps
 * Stores deltas
 *
 */
void *migrate_func(void *args){
    uint64_t (*timestamps)[ITERS][SAMPLES] = args;
    cpu_set_t cpuset;
    int i,j,cpu;
    double pre,post;

    for(i =0;i<ITERS;i++)
        for(j =0;j<SAMPLES;j++){
            if (LINUX){
                cpu = sched_getcpu();
                CPU_ZERO(&cpuset);
                if(cpu == 0) //switch to 1
                    CPU_SET(1,&cpuset);
                else if (cpu == 1) // switch to 0
                    CPU_SET(0,&cpuset);
                else
                    printf("SCHED_GETCPU broke\n");
                pre = current_time();
                pthread_setaffinity_np(pthread_self(),sizeof(cpuset),&cpuset);
                post = current_time();
                (*timestamps)[i][j] = post-pre;
            }
            else if (KITTEN){
                cpu = sched_getcpu();
                if(cpu ==0){
                    pre = current_time();
                    task_switch_cpus(1);
                }
                else if(cpu ==1){
                    pre = current_time();
                    task_switch_cpus(0);
                }
                else
                    printf("SCHED_GETCPU broke\n");
                post = current_time();
                (*timestamps)[i][j] = post-pre;
            }
        }
}


/*
 * Multi Core MSR sampling function.
 * Will read APERF, MPERF, TSC on core 0 and 1
 *
 * Linux uses msr_safe batch interface
 * Kitten uses nha_read_msr and task_switch_cpus
 */
void multi_core_msr_test(){
    if(LINUX){
        uint64_t APERF[ITERS][SAMPLES];
        uint64_t MPERF[ITERS][SAMPLES];
        uint64_t TSC[ITERS][SAMPLES];
        int i,j,k,res;
        /* setup ioctl to msr module to read APERF, MPERF, TSC */
        struct msr_batch_array ba;
        int numcpu = 2;
        int numops =  3 * numcpu;
        ba.numops = numops;
        ba.ops = (struct msr_batch_op *)malloc(sizeof(struct msr_batch_op) * numops);
        for(k=0;k<numcpu;k++){
            // APERF batch op
            struct msr_batch_op *op = &ba.ops[(k * 3)];
            op->cpu = k;
            op->isrdmsr = 1;
            op->msr = 0xe8;
            op->msrdata = 0;
            // MPERF batch op
            op = &ba.ops[(k*3) + 1];
            op->cpu = k;
            op->isrdmsr = 1;
            op->msr = 0xe7;
            op->msrdata = 0;
            /* TSC batch op */
            op = &ba.ops[(k*3) + 2];
            op->cpu = k;
            op->isrdmsr = 1;
            op->msr = 0x10;
            op->msrdata = 0;
        }
        int bfd = open("/dev/cpu/msr_batch",O_RDWR);
        for(i=0;i<ITERS;i++){
            for(j=0;j<SAMPLES;j++){
                res = ioctl(bfd,X86_IOC_MSR_BATCH,&ba);
                for(k=0;k<numcpu;k++){
                    APERF[i][j] = ba.ops[(k*3)].msrdata;
                    MPERF[i][j] = ba.ops[(k*3) + 1].msrdata;
                    TSC[i][j] = ba.ops[(k*3) + 2].msrdata;
                }
            }
        }
        close(bfd);
    }
    
    else if(KITTEN){
        uint64_t APERF[ITERS][SAMPLES];
        uint64_t MPERF[ITERS][SAMPLES];
        uint64_t TSC[ITERS][SAMPLES];
        uint64_t ALL[ITERS][SAMPLES * 3];
        int i,j,k;
        int numcpu = 2;
        for(i=0;i<ITERS;i++){
            for(j=0;j<SAMPLES;j++){
                for(k=0;k<numcpu;k++){
                    task_switch_cpus(k);
                    int idx = (j * 3);
                    printf("idx: %i\n",idx);
                    nha_read_msr(&ALL[i][idx],1);
                }
            }
        }

        for(i =0;i<ITERS;i++){
            for(j =0; j < SAMPLES;j++){
                APERF[i][j] = 0;
                MPERF[i][j] = 0;
                TSC[i][j] = 0;
            }
        }

    }




}




int main(){

    //read_tsc_test();
    //thread_migrate_test();
    multi_core_msr_test();
    return 0;
}

