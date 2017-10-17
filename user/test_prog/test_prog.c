#include <lwk/liblwk.h>
#include <unistd.h>
#include <stdio.h>
//#include <lwk/read_msr.h>




int main(){
    uint64_t d = 0;
    printf("hi   %i\n",nha_read_msr(&d));
    printf("%lu\n",d);
    return 0;

}
