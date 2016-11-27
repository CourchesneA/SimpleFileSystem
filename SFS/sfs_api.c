#include "sfs_api.h"
#include "disk_emu.h"
#include <stdio.h>
#include <string.h>

int sfs_fcreate(char *name);
int sfs_find_empty_inode();
int sfs_find_empty_dir_entry();
int sfs_find_empty_fd();
int sfs_find_in_directory(char *name);

const int BLOCKS_SIZE = 1024;
const int NUM_BLOCKS = 25000;
const int INODE_TABLE_LENGHT = 15;	//Each inode is 72 bytes, so 15 blocks for 200 inodes
const int ROOT_DIRECTORY = 0;		//Inode index of the root directory
const int NUM_INODES = 200;			//Limitting to the size of the inode table
const int DIRECTORY_LENGTH = 200;	//Number of max file in a directory mapping
const int FD_TABLE_LENGTH = 20;		//Max entry in file descriptor table
const int INODE_TABLE_BLK = 1;
int FREE_BITMAP_LENGTH = 4;			// 25000 block / (1024 byte * 8bits)
int FREE_BITMAP_BLK = NUM_BLOCKS-FREE_BITMAP_LENGTH-1;
//In-memory structures;
Inode inode_table[NUM_INODES];
Directory_entry directory[DIRECTORY_LENGTH];
File_descriptor_entry fd_table[FD_TABLE_LENGTH];
int bitmap[]

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
		Inode root_inode = {.mode=1};	//Every other value in the inode are 0. We use mode to say that the inode is used
		printf("Size of inode is: %lu\n", sizeof(root_inode));
		printf("Size of inode Table is: %lu\n", sizeof(inode_table));
		inode_table[ROOT_DIRECTORY]=root_inode;
		//Code disable, not writting to disk for now
		// if ( write_blocks(INODE_TABLE_BLK, 1, &root_inode) < 0){		//Second block block, 1 block (3 for the whole table)
		// 	printf("Error while initializing Directory Inode\n");
		// 	return;
		// }

		//Initialize directory with empty strings
		int i;
		for(i=0; i<DIRECTORY_LENGTH; i++){
			directory[i].filename = "";
		}

		//write the free bitmap
		int free_bitmap[31];	//each int has 16 bits so 16*31=496 which is enough for 494 blocks (6 used for meta data not int free bitmap)
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
	//get index of the file in the directory
	int dir_index;
	if((dir_index = sfs_find_in_directory(path)) < 0){
		printf("Error, file %s not found in directory\n",path);
		return -1;
	}
	//get its inode
	int inode_nb = directory[dir_index].inode_index;
	if(inode_nb < 1 || inode_nb > NUM_INODES){
		printf("Error, incorrect inode index: %d\n",inode_nb);
		return -1;
	}
	int filesize = inode_table[inode_nb].size;
	printf("File %s have size %d",path, filesize);
  	return filesize;
}
int sfs_fopen(char *name){	//Second
	//Open or create file and return fd
	//1. Find its entry in directory or create the file
	int dir_index = sfs_find_in_directory(name);
	int file_index;
	if(dir_index == -1){	//File not found
		printf("Could not find file: %s in directory, creating file\n", name);
		if((file_index = sfs_fcreate(name)) == -1 ){	//Get file inode num
			printf("Error creating file\n");
			return -1;
		}
		printf("Created file with inode_index: %i\n", file_index);
	}else{
		file_index = directory[dir_index].inode_index;
		printf("Retreived file %s as inode number %d", name, file_index);
	}

	//Now we have the inode index which is not -1
	//2. Create entry in FD table and return this entry
	//find the first empty spot in fd_table
	int fd_index;
	if((fd_index = sfs_find_empty_fd()) == -1){
		printf("Error while looking for free file descriptor\n");
		return -1;
	}
	//fd_table[fd_index]
	File_descriptor_entry fd = {.inode_index = file_index,	//TODO might want to check for fd_outofbound
							.rptr=0,
							.wptr=inode_table[file_index].size};	//Use the size of the file to set the rptr
							//wprt should be 0 for new file since size is 0

	fd_table[fd_index] = fd;
	printf("Created entry %d in fd table, wptr at %d\n",fd_index,inode_table[file_index].size);

	//return fd index
  	return fd_index;
}
int sfs_fclose(int fileID){	//Find fd and remove it
	if(!(fd_table[fileID].inode_index > 0 && fd_table[fileID].inode_index < NUM_INODES)){
		printf("Invalid file descriptor\n");
		return -1;
	}
	File_descriptor_entry empty_fd = {0};		//create an empty FD
	fd_table[fileID] = empty_fd;		//Assign empty FD to this one
	printf("Removed file descriptor\n");
  	return 0;
}
int sfs_frseek(int fileID, int loc){	//Change the rptr location
	//TODO add conditions
	if(!(fd_table[fileID].inode_index > 0 && fd_table[fileID].inode_index < NUM_INODES)){
		printf("Invalid file descriptor (rptr)\n");
		return -1;
	}
	fd_table[fileID].rptr = loc;
	printf("Successfully moved read ptr to %d\n", loc);
  	return 0;
}
int sfs_fwseek(int fileID, int loc){	//Change the wptr location
	//TODO add conditions
	if(!(fd_table[fileID].inode_index > 0 && fd_table[fileID].inode_index < NUM_INODES)){
		printf("Invalid file descriptor (wptr)\n");
		return -1;
	}
	fd_table[fileID].wptr = loc;
	printf("Successfully moved write ptr to %d\n", loc);
  	return 0;
}
int sfs_fwrite(int fileID, char *buf, int length){
	//Allocate blocks in the bitmap

	//Modify i-Node table in memory

	//read block and append data

	//flush modification to disk

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
int sfs_find_empty_dir_entry(){	//return empty entry index or -1 if not found
	int i;
	for(i=0; i<DIRECTORY_LENGTH; i++){
		if(directory[i].inode_index == 0){
			return i;
		}
	}
	printf("Error, all %i entries in directory are taken\n",DIRECTORY_LENGTH);
	return -1;
}

int sfs_find_empty_fd(){	//return empty entry index or -1 if not found
	int i;
	for(i=0; i<FD_TABLE_LENGTH; i++){
		if(fd_table[i].inode_index == 0){	//If inode_index is 0 then fd is empty
			return i;
		}
	}
	printf("Error, all %i entries in file descriptor table are taken\n",FD_TABLE_LENGTH);
	return -1;
}

int sfs_find_in_directory(char *name){
	//Find a file in the directory and return its index
	int i;
	for(i=0;i<DIRECTORY_LENGTH;i++){
		if(strcmp(directory[i].filename, name)==0){
			return i;
		}
	}
	printf("File %s not found in directory\n",name);
	return -1;
}
