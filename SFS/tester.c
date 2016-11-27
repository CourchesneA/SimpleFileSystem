#include <stdio.h>
#include "sfs_api.h"

int main(int argc, char *argv[]){
   	printf("Starting test sequence\n");
    mksfs(1);
    sfs_fopen("testfile");
   	printf("test done\n");
   	char z[3125];
   	printf("int size: %lu\n",sizeof(z));	//is 4
}