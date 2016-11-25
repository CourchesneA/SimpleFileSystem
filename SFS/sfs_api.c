#include "sfs_api.h"
#include "disk_emu.h"
#include <stdio.h>
#include <string.h>

int sfs_fcreate(char *name);
int sfs_find_empty_inode();
int sfs_find_empty_dir_entry();

const int BLOCKS_SIZE = 1024;
const int NUM_BLOCKS = 50000;
const int INODE_TABLE_LENGHT = 3;
const int ROOT_DIRECTORY = 0;
const int NUM_INODES = 500;	//Limitting to the size of the inode table
const int DIRECTORY_LENGTH = 200;
const int FD_TABLE_LENGTH = 20;
int INODE_TABLE_BLK = 1;
int FREE_BITMAP_BLK = NUM_BLOCKS-1;
Inode inode_table[NUM_INODES];
Directory_entry directory[DIRECTORY_LENGTH];		
File_descriptor_entry fd_table[FD_TABLE_LENGTH];


void mksfs(int fresh){
//Format the given virtual disk and creates a SFS on top of it. fresh = create, else opened
	char *filename = "cVirtualDisk";		//Name of the virtual disk file
	

	if(fresh){
		if(init_fresh_disk(filename, BLOCKS_SIZE, NUM_BLOCKS)<0){
			printf("Error while creating virtual disk\n");
			return;
		}
		//Write the super block
		int super_block[5];
		super_block[0] = 0;			//Magic, not used
		super_block[1] = BLOCKS_SIZE;		//Block size
		super_block[2] = NUM_BLOCKS;		//Number of blocks
		super_block[3] = INODE_TABLE_LENGHT;	//Lenght of inode table
		super_block[4] = ROOT_DIRECTORY;		//inode nb of the root directory
		if ( write_blocks(0, 1, super_block) < 0){		//First block, 1 block
			printf("Error while initializing super block\n");
			return;
		}
		//write the first Inode
		//Lets say we can have 200 inodes
		Inode root_inode = {.mode=1};	//Every other value in the inode are 0. We use mode to say that the inode is initialized
		printf("Size of inode is: %lu\n", sizeof(root_inode));
		printf("Size of inode Table is: %lu\n", sizeof(inode_table));
		inode_table[ROOT_DIRECTORY]=root_inode;
		//Code disable, not writting to disk for now
		// if ( write_blocks(INODE_TABLE_BLK, 1, &root_inode) < 0){		//Second block block, 1 block (3 for the whole table)
		// 	printf("Error while initializing Directory Inode\n");
		// 	return;
		// }
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
		if(init_disk(filename, BLOCKS_SIZE, NUM_BLOCKS)<0){
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
int sfs_fopen(char *name){	//Second
	//Open or create file and return fd
	//1. Find its entry in directory
	int file_index = -1;
	int i;
	for(i=0;i<DIRECTORY_LENGTH;i++){
		if(strcmp(directory[i].filename, name)==0){
			file_index=directory[i].inode_index;
			break;
		}
	}
	if(file_index == -1){
		printf("Could not find file: %s in directory, creating file\n", name);
		file_index = sfs_fcreate(name);
		printf("Created file with inode_index: %i\n", file_index);
	}



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

//Helpers
int sfs_fcreate(char *name){	//create file and return its inode_index
	//create an inode
	//find empty inode
	int new_inode_index=-1;
	if((new_inode_index = sfs_find_empty_inode()) == -1){
		printf("Error while looking for free inode entry\n");
		return -1;
	}
	//Populate inode
	Inode new_inode = {.mode=1};	//Size and pointer are 0
	inode_table[new_inode_index] = new_inode;
	//Add file to directory
	//find empty directory entry
	int new_dir_index=-1;
	if((new_dir_index = sfs_find_empty_dir_entry()) == -1){
		printf("Error while looking for free directory entry\n");
		return -1;
	}
	//Populate directory entry
	Directory_entry new_dir_entry = {.filename = name, .inode_index = new_dir_index};
	directory[new_dir_index] = new_dir_entry;
	//Here we could modify the directory inode if there was anything to do with it
	return new_inode_index;
}

int sfs_find_empty_inode(){
	int i;
	for(i=0; i<INODE_TABLE_LENGHT; i++){
		if(inode_table[i].mode == 0){
			return i;
		}
	}
	printf("Error, all %i entries in inode table are taken\n",INODE_TABLE_LENGHT);
	return -1;
}
int sfs_find_empty_dir_entry(){
	int i;
	for(i=0; i<DIRECTORY_LENGTH; i++){
		if(directory[i].inode_index == 0){
			return i;
		}
	}
	printf("Error, all %i entries in directory are taken\n",DIRECTORY_LENGTH);
	return -1;
}

