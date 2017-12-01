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
#include <math.h>

#define SAMPLES 100// number of samples to get per test
#define ITERS 5 // number of runs per test taking SAMPLES
/* GLOBALS */
int numcpu = 2;
uint64_t max_non_turbo;



/* PROTOTYPE */
void *migrate_func(void *args);



/*
 * calculates mean, std dev of data
 * data needs to be pre allocated as 2d double array   ITERS by samples
 */
void stat_data(void *data,void* mean, void *sdev, int samples){

    double (*DATA)[ITERS][samples] = data;
    double (*MEAN)[ITERS] = mean;
    double (*SDEV)[ITERS] = sdev;
    int i,j;
   // Get the mean
   for(i =0;i<ITERS;i++)
       for(j=0;j < samples;j++){
           (*MEAN)[i] += (*DATA)[i][j];
        }
   for(i =0;i<ITERS;i++)
       (*MEAN)[i]/= samples;


   // Get the STD DEV
   for(i=0;i<ITERS;i++){
       for(j=0;j<samples;j++)
           (*SDEV)[i] += pow((*DATA)[i][j] - (*MEAN)[i],2); 
       (*SDEV)[i]/= samples-1;
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
        //stat_data(timestamps);
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
    double thread_delta_timestamps[ITERS][SAMPLES];
    double mean[ITERS];
    double sdev[ITERS];

    pthread_t thread;
    pthread_create(&thread,NULL,migrate_func,thread_delta_timestamps);
    pthread_join(thread,NULL);
    stat_data(thread_delta_timestamps,mean,sdev,SAMPLES);

    for(i=0;i<ITERS;i++){
        printf("RUN %i\n",i);

        printf("MEAN: %.15lf\n",mean[i]);
        printf("SDEV: %.15lf\n\n",sdev[i]);
    }



}

/*
 * Pthread function. Takes pre allocated 2d array to store timestamps
 * Stores deltas
 *
 */
void *migrate_func(void *args){
    double (*timestamps)[ITERS][SAMPLES] = args;
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
                (*timestamps)[i][j] = (post-pre)/(2.5 *1000000000);
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
                (*timestamps)[i][j] = (post-pre)/(2.3*1000000000);
            }
        }
}


/*
 * Multi Core MSR sampling function.
 * Will read APERF, MPERF, TSC on core 0 and 1
 * I discard APERF/MPERF since im just looking at TSC....
 *
 * Linux uses msr_safe batch interface
 * Kitten uses nha_read_msr and task_switch_cpus
 */
void multi_core_msr_test(){
    uint64_t CORE0_TSC[ITERS][SAMPLES]={{0}};
    uint64_t CORE1_TSC[ITERS][SAMPLES]={{0}};
    double CORE0_DELTA[ITERS][SAMPLES-1]={{0}};
    double CORE1_DELTA[ITERS][SAMPLES-1]={{0}};
    double CORE0_MEAN[ITERS] = {0};
    double CORE1_MEAN[ITERS] = {0};
    double CORE0_SDEV[ITERS] = {0.0};
    double CORE1_SDEV[ITERS] = {0.0};
    if(LINUX){
        //uint64_t APERF[ITERS][SAMPLES];
        //uint64_t MPERF[ITERS][SAMPLES];
        //uint64_t TSC[ITERS][SAMPLES];
        int i,j,k,res;
        /* setup ioctl to msr module to read APERF, MPERF, TSC */
        struct msr_batch_array ba;
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
                CORE0_TSC[i][j] = ba.ops[2].msrdata;
                CORE1_TSC[i][j] = ba.ops[5].msrdata;
                //APERF[i][j] = ba.ops[(k*3)].msrdata;
                //MPERF[i][j] = ba.ops[(k*3) + 1].msrdata;
                //TSC[i][j] = ba.ops[(k*3) + 2].msrdata;

             }
        }
        close(bfd);


        // DATA ANALYSIS


        // Get the Deltas
        for(i = 0;i<ITERS;i++)
            for(j=0;j<SAMPLES-1;j++){
                CORE0_DELTA[i][j] = (CORE0_TSC[i][j+1] - CORE0_TSC[i][j])/(2.5*1000000000);
                CORE1_DELTA[i][j] = (CORE1_TSC[i][j+1] - CORE0_TSC[i][j])/(2.5*1000000000);
                //printf("CORE0_DELTA: %.15f\n",CORE0_DELTA[i][j]);
            }
        stat_data(CORE0_DELTA,CORE0_MEAN,CORE0_SDEV,SAMPLES-1);
        stat_data(CORE1_DELTA,CORE1_MEAN,CORE1_SDEV,SAMPLES-1);

        for(i = 0;i<ITERS;i++){
           printf("RUN %i\n",i);
           printf("CORE0 STDEV: %.15f\n",(double)CORE0_SDEV[i]);
           printf("CORE1 STDEV: %.15f\n",(double)CORE1_SDEV[i]);

           printf("CORE0 MEAN: %.15f\n",(double)CORE0_MEAN[i]);
           printf("CORE1 MEAN: %.15f\n",(double)CORE1_MEAN[i]);
           printf("\n");



        }
       printf("LINUX\n");
    }
    
    else if(KITTEN){
        //uint64_t APERF[ITERS][SAMPLES * numcpu];
        //uint64_t MPERF[ITERS][SAMPLES * numcpu];
        //uint64_t TSC[ITERS][SAMPLES * numcpu];
        uint64_t ALL[ITERS][SAMPLES * 6] = {{69}};
        int i,j,k,idx,lastidx;

        //int seen[ITERS][SAMPLES] = {{69}};
        /* 
         * ALL[i][0] = CORE0_APERF
         * ALL[i][1] = CORE0_MPERF
         * ALL[i][2] = CORE0_TSC
         * ALL[i][3] = CORE1_APERF
         * ALL[i][4] = CORE1_MPERF
         * ALL[i][5] = CORE1_TSC
         * .
         * .
         * .
         *
         */
        //printf("idx: ");
        for(i=0;i<ITERS;i++){
            for(j=0;j<SAMPLES;j++){
                idx = j*6;
                //seen[i][j] = idx;
                for(k=0;k<numcpu;k++){
                    task_switch_cpus(k);
                    idx +=(k*3); // probably a better way to handle this lol
                    //printf("%i ",idx);
                    nha_read_msr(&ALL[i][idx],1);
                }
            }
            printf("\n");
        }

        /* 
         * SANITY CHECK
        for(i=0;i<ITERS;i++)
            for(j=0;j<SAMPLES*6;j++)
                printf("%llu ",ALL[i][j]);
        printf("\n");
        printf("SEEN:    ");
        for(i=0;i<ITERS;i++)
            for(j=0;j<SAMPLES;j++)
                printf("%i ",seen[i][j]);
        printf("\n");
        */

        /*
         * Split ALL to make my life easier? 
         * I only really care about TSC...
         */
        int jj,jjj,core;
        for(i =0;i<ITERS;i++){
            idx = 2; // core 0 sample 0 TSC
            jj = 0;
            jjj =0;
            core = 0;
            for(j =0; j < SAMPLES * numcpu;j++){
                    //TSC[i][j] = ALL[i][idx];
                    if(core ==0){
                        CORE0_TSC[i][jj] = ALL[i][idx];
                        jj++;
                        core =1;
                    }
                    else{
                        CORE1_TSC[i][jjj] = ALL[i][idx];
                        jjj++;
                        core = 0;
                    }
                    idx+=3;
                }
        }

        // DATA ANALYSIS 
        for(i =0;i<ITERS;i++)
            for(j=0;j < SAMPLES-1;j++){
                CORE0_DELTA[i][j] = (double)(CORE0_TSC[i][j+1] - CORE0_TSC[i][j]) / (2.3 * 1000000000);
                CORE1_DELTA[i][j] = (double)(CORE1_TSC[i][j+1] - CORE1_TSC[i][j]) / (2.3 * 1000000000);
                //CORE0_DELTA[i][j] = (double)(CORE0_TSC[i][j+1] - CORE0_TSC[i][j]);
                //CORE1_DELTA[i][j] = (double)(CORE1_TSC[i][j+1] - CORE1_TSC[i][j]);
                //printf("CORE0DELTA: %f\n",CORE0_DELTA[i][j]);
            }

        stat_data(CORE0_DELTA,CORE0_MEAN,CORE0_SDEV,SAMPLES-1);
        stat_data(CORE1_DELTA,CORE1_MEAN,CORE1_SDEV,SAMPLES-1);
       for(i=0;i<ITERS;i++){
           printf("RUN %i\n",i);
           printf("CORE0 STDEV: %.15f\n",(double)CORE0_SDEV[i]);
           printf("CORE1 STDEV: %.15f\n",(double)CORE1_SDEV[i]);

           printf("CORE0 MEAN: %.15f\n",(double)CORE0_MEAN[i]);
           printf("CORE1 MEAN: %.15f\n",(double)CORE1_MEAN[i]);
           printf("\n");
       } 
        /*
        // Get the mean
        for(i =0;i<ITERS;i++)
            for(j=0;j < SAMPLES-1;j++){
                CORE0_MEAN[i] += CORE0_DELTA[i][j];
                CORE1_MEAN[i] += CORE1_DELTA[i][j];
            }
        for(i =0;i<ITERS;i++){
            CORE0_MEAN[i]/= (SAMPLES-1);
            CORE1_MEAN[i]/= (SAMPLES-1);
            //printf("CORE0_MEAN: %f\n",CORE0_MEAN[i]);
            //printf("CORE1_MEAN: %f\n",CORE1_MEAN[i]);
        }

        // Get the STD DEV
        for(i=0;i<ITERS;i++){
            for(j=0;j<SAMPLES-1;j++){
                CORE0_SDEV[i] += pow((CORE0_DELTA[i][j] - CORE0_MEAN[i]),2); 
                CORE1_SDEV[i] += pow((CORE1_DELTA[i][j] - CORE1_MEAN[i]),2); 
            }
        }

       */

        
    }
}


void test_func(){
    uint64_t core0_tsc;
    uint64_t core1_tsc;
    nha_read_msr(&core0_tsc,0);
    printf("MY CPU: %i      %llu\n",sched_getcpu(),core0_tsc);
    task_switch_cpus(1);
    nha_read_msr(&core1_tsc,0);
    printf("MY CPU: %i      %llu\n",sched_getcpu(),core1_tsc);


}


int main(){
    uint64_t platform_info;
    //nha_read_msr(&platform_info,2);
    //max_non_turbo = (platform_info >> 8) & 0xFF;

    //read_tsc_test();
    thread_migrate_test();
    //multi_core_msr_test();
    //test_func();
    return 0;
}

