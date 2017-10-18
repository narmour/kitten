#include <lwk/liblwk.h>
#include <unistd.h>
#include <stdio.h>




int main(){
    uint64_t data[NUM_MSR];
    nha_read_msr(data);

    int i;
    for(i=0;i<NUM_MSR;i++)
        printf("%lu\n",data[i]);
    return 0;

}
