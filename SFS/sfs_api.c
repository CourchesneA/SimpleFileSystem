#include "sfs_api.h"
#include "disk_emu.h"
#include <stdio.h>


void mksfs(int fresh){
//Format the given virtual disk and creates a SFS on top of it. fresh = create, else opened
	char *filename = "cVirtualDisk";		//Name of the virtual disk file
	const int BLOCK_SIZE = 1024;
	const int NUM_BLOCKS = 500;
	const int INODE_TABLE_LENGHT = 3;
	const int ROOT_DIRECTORY = 1;
	int INODE_TABLE_BLK = 1;
	int FREE_BITMAP_BLK = 499;

	if(fresh){
		if(init_fresh_disk(filename, BLOCK_SIZE, NUM_BLOCKS)<0){
			printf("Error while creating virtual disk\n");
			return;
		}
		//Write the super block
		int super_block[5];
		super_block[0] = 0;			//Magic, not used
		super_block[1] = BLOCK_SIZE;		//Block size
		super_block[2] = NUM_BLOCKS;		//Number of blocks
		super_block[3] = INODE_TABLE_LENGHT;	//Lenght of inode table
		super_block[4] = ROOT_DIRECTORY;		//inode nb of the root directory
		if ( write_blocks(0, 1, super_block) < 0){		//First block, 1 block
			printf("Error while initializing super block\n");
			return;
		}
		//write the first Inode
		Inode root_inode = {.mode=1};	//Every other value in the inode are 0. We use mode to say that the inode is initialized
		printf("Size of inode is: %lu\n", sizeof(root_inode));
		if ( write_blocks(INODE_TABLE_BLK, 1, &root_inode) < 0){		//Second block block, 1 block (3 for the whole table)
			printf("Error while initializing Directory Inode\n");
			return;
		}
		//write the free bitmap
		int free_bitmap[31];	//each int has 16 bits so 16*31=496 which is enough for 494 blocks (6 used for meta data not int free bitmap)
		int i;
		for(i=0;i<31;i++){
			free_bitmap[i]=0;			//at the beggining every block is free
		}
		if ( write_blocks(FREE_BITMAP_BLK, 1, free_bitmap) < 0){		//Last block, 1 block
			printf("Error while initializing free bitmap\n");
			return;
		}


	}else{
		if(init_disk(filename, BLOCK_SIZE, NUM_BLOCKS)<0){
			printf("Error while opening virtual disk\n");
			return;
		}
	}

}
int sfs_get_next_file_name(char *fname){
  return 0;
}
int sfs_get_file_size(char* path){
  return 0;
}
int sfs_fopen(char *name){
  return 0;
}
int sfs_fclose(int fileID){
  return 0;
}
int sfs_frseek(int fileID, int loc){
  return 0;
}
int sfs_fwseek(int fileID, int loc){
  return 0;
}
int sfs_fwrite(int fileID, char *buf, int length){
  return 0;
}
int sfs_fread(int fileID, char *buf, int length){
  return 0;
}
int sfs_remove(char *file){
  return 0;
}
