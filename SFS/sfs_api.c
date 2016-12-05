#include "sfs_api.h"
#include "disk_emu.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

//helpers
int sfs_fcreate(char *name);
int sfs_find_empty_inode();
int sfs_find_empty_dir_entry();
int sfs_find_empty_fd();
int sfs_find_in_directory(char *name);
int set_block_free(int blocknum);
int set_block_used(int blocknum);
int find_free_block();
int read_bitmap();
int write_bitmap(unsigned char* bitmap);
int add_block_to_inode(int blocknum, Inode *inode);
int file_get_nth_block(int blocknum, Inode inode);
int write_inode_table(Inode *i_table);
int read_inode_table(Inode *i_table);
int write_directory(Directory_entry *dir);
int read_directory(Directory_entry *dir);
int write_super_block(int *s_block);
int read_super_block(int *sblock);
int sfs_find_in_fd_table(int i_index);




const int BLOCKS_SIZE = 1024;
const int NUM_BLOCKS = 25000;
const int INODE_TABLE_LENGHT = 15;	//Each inode is 72 bytes, so 15 blocks for 200 inodes
const int ROOT_DIRECTORY = 0;		//Inode index of the root directory
const int NUM_INODES = 200;			//Limitting to the size of the inode table
const int DIRECTORY_LENGTH = 200;	//Number of max file in a directory mapping
const int DIRECTORY_BLOCKS = 5;
const int FD_TABLE_LENGTH = 20;		//Max entry in file descriptor table
const int INODE_TABLE_BLK = 1;
const int FREE_BITMAP_LENGTH = 5;			// 25000 block / (1024 byte * 8bits)
const int FREE_BITMAP_BLK = NUM_BLOCKS-FREE_BITMAP_LENGTH-1;  //-1 ?
int DIRECTORY_BLK_INDEX = FREE_BITMAP_BLK - DIRECTORY_BLOCKS;
int DIRECTORY_INDEX;
//In-memory structures;
Inode inode_table[NUM_INODES];
Directory_entry directory[DIRECTORY_LENGTH];
File_descriptor_entry fd_table[FD_TABLE_LENGTH];

int super_block[256]; //Lot of empty space so no segfault on memcpy for a block

//int super_block[5];
unsigned char bitmap[3125];
//char bitmap[3125];
int INT_SIZE;

void mksfs(int fresh){
  //Format the given virtual disk and creates a SFS on top of it. fresh = create, else opened
	char *filename = "cVirtualDisk";		//Name of the virtual disk file
	INT_SIZE = sizeof(int);

	if(fresh){
    //Init the disk
		if(init_fresh_disk(filename, BLOCKS_SIZE, NUM_BLOCKS)<0){
			printf("Error while creating virtual disk\n");
			return;
		}

		//Initialize super block
    
		super_block[0] = 0;			//Magic, not used
		super_block[1] = BLOCKS_SIZE;		//Block size
		super_block[2] = NUM_BLOCKS;		//Number of blocks
		super_block[3] = INODE_TABLE_LENGHT;	//Lenght of inode table
		super_block[4] = ROOT_DIRECTORY;		//inode nb of the root directory
    //Write the super block
		if ( write_super_block(super_block) < 0){		//First block, 1 block
			printf("Error while wrinting super block to disk\n");
			return;
		}

    //Initialize directory
    DIRECTORY_INDEX=0;      //This is the index used for get_next_file();
    memset(directory,0,sizeof(Directory_entry)*DIRECTORY_LENGTH);
    //Write the directory to disk
    if ( write_directory(directory) < 0){    //First block, 1 block
      printf("Error while writing directory to disk\n");
      return;
    }

    //initialize inode table
    memset(inode_table, 0, NUM_INODES* sizeof(Inode));
		//initialize the first Inode for root directory
		Inode root_inode = {.mode=1};	//Every other value in the inode are 0. We use mode to say that the inode is used
		inode_table[ROOT_DIRECTORY]=root_inode;
    //write the inode table
    if ( write_inode_table(inode_table)){   //Second block block, 1 block (3 for the whole table)
      printf("Error while writing inode_table to disk\n");
      return;
    }


    //initialize the bitmap

    memset(bitmap,0xFF,3125);

    bitmap[0]=0;    //Contains Super block and inode table, not free
    bitmap[1]=0;    //Contains inode table
    bitmap[3124]=0; //Contains directory
    /*int i;
		for(i=0;i<3125;i++){
			if(i<2){
				bitmap[i]=0;
				continue;
			}
			bitmap[i]=0xFF;			//at the beggining every block is free
		}*/
    //Write bitmap to disk
		if ( write_bitmap(bitmap) < 0){
			printf("Error while writing bitmap to disk\n");
			return;
		}

	}else{
		if(init_disk(filename, BLOCKS_SIZE, NUM_BLOCKS)<0){
			printf("Error while opening virtual disk\n");
			return;
		}
		//read super block
    if ( read_super_block(super_block) < 0){    //First block, 1 block
      printf("Error while reading super block\n");
      return;
    }
    
		//read inode table
    if( read_inode_table(inode_table) < 0){
      printf("Error while reading inode table from disk\n");
      return;
    }

		//read bitmap
    if ( read_bitmap(bitmap) < 0){
      printf("Error while reading bitmap from disk\n");
      return;
    }

		//read directory
    if ( read_directory(directory) < 0){    //First block, 1 block
      printf("Error reading directory from disk\n");
      return;
    }
	}

}
int sfs_get_next_file_name(char *fname){
	//check if there is an entry here
	if(directory[DIRECTORY_INDEX].inode_index < 1){
    printf("No more file in directory\n");
    DIRECTORY_INDEX = 0;
    return 0;
  }

	char *name = directory[DIRECTORY_INDEX++].filename;
	strcpy(fname, name);
  return 1;
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
	printf("File %s have size %d\n",path, filesize);
  	return filesize;
}
int sfs_fopen(char *name){
  //Create entry in FD table and return this entry
  //find the first empty spot in fd_table
  int fd_index;
  if((fd_index = sfs_find_empty_fd()) == -1){
    printf("Error while looking for free file descriptor\n");
    return -1;
  }

	//Open or create file and return fd
	//1. Find its entry in directory or create the file
	int dir_index = sfs_find_in_directory(name);
	int file_index;
	if(dir_index == -1){	//File not found
		printf("Creating file %s\n", name);
		//TODO check namelength
		if((file_index = sfs_fcreate(name)) == -1 ){	//Get file inode num
			printf("Error creating file\n");
			return -1;
		}
		printf("Created file with inode_index: %i\n", file_index);
	}else{
		file_index = directory[dir_index].inode_index;
		printf("Retreived file %s as inode number %d\n", name, file_index);
	}

	//Now we have the inode index which is not -1
	
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
	if(fileID < 0){
		printf("Invalid file descriptor\n");
		return -1;
	}
	if(!(fd_table[fileID].inode_index > 0 && fd_table[fileID].inode_index < NUM_INODES)) {
		printf("Invalid file descriptor\n");
		return -1;
	}
	File_descriptor_entry empty_fd = {0};		//create an empty FD
	fd_table[fileID] = empty_fd;		//Assign empty FD to this one
	printf("Removed file descriptor\n");
  	return 0;
}
int sfs_frseek(int fileID, int loc){	//Change the rptr location
  if(loc < 0 || loc > inode_table[fd_table[fileID].inode_index].size){
    printf("Invalid rptr location\n");
    return -1;
  }
	if(fileID < 0){
		printf("Invalid file descriptor (rptr)\n");
		return -1;
	}
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
  if(loc < 0 || loc > inode_table[fd_table[fileID].inode_index].size){
    printf("Invalid wptr location\n");
    return -1;
  }
	if(fileID < 0){
		printf("Invalid file descriptor (wptr)\n");
		return -1;
	}
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
	if(fileID < 0 || length < 0){
		printf("Invalid file descriptor\n");
		return -1;
	}
	if(fd_table[fileID].inode_index < 1){
		printf("Error, could not find file descriptor in open file table \n");
		return -1;
	}
	Inode *file_inode = &inode_table[fd_table[fileID].inode_index];
  int initial_size = file_inode->size;
  int initial_wptr = fd_table[fileID].wptr;
	

  //New version, doing step-by-step in a loop

	int i;
	int bytes_written = 0;
	int index_to_write;
	int current_block_num;
	
	//Load last block
	//find which is the last block pointed by wprtHERE debugg
	int write_loc = fd_table[fileID].wptr / 1024;	//result is something like 5th block
	int write_loc_offset = fd_table[fileID].wptr % 1024;
	int last_write_block;	//find which block does this points to
	
	if((last_write_block = file_get_nth_block(write_loc,*file_inode)) < 0){
		printf("Error, could not get block number of read pointer\n");
	}

  //check if we have a block
  int new_block_num;
  if(last_write_block <= 0){
    //no block, assign a new one
    
        if((new_block_num = find_free_block()) < 1){
          printf("Error while looking for new block\n");
          return bytes_written*(-1);
        }
        //Assign in inode_table
        if(add_block_to_inode(new_block_num, file_inode) < 0){
          printf("Error while adding new block to inode\n");
          return bytes_written*(-1);
        }
        last_write_block = new_block_num;
  }else{
    //we retreived a block, make sure we are appending to the good location
    //printf(">>Block retreived: %d at %d<<\n",last_write_block, write_loc_offset );
  }

	char rbuf[1024];
	if(read_blocks(last_write_block, 1, rbuf) < 0){
		printf("Error, could not read data block pointed by write pointer\n");
		return -1;
	}else if(write_loc_offset != 0){
    //printf("Loading block before writing\n");
  }
	index_to_write = write_loc_offset;
	current_block_num = last_write_block;
	
	for(i=0;i<length;i++){//for each byte
		if(index_to_write >= 1024){	//Block is full, get new block
			//flush block
			if((write_blocks(current_block_num,1,rbuf)) < 0){
				printf("Error while writing indirect pointer to block\n");
				return bytes_written*(-1);
			}else{
        //printf("Succesfully written %d chars to block %d\n",strlen(rbuf),current_block_num);
      }
      //set index to 0 and clear buffer
      index_to_write = 0;
      memset(rbuf,0,1024);

			//get new block  - No, get NEXT block
      if((current_block_num = file_get_nth_block(++write_loc,*file_inode)) <= 0){
        //No next block in file inode, Create block
        if((new_block_num = find_free_block()) < 1){
          printf("Error while looking for new block\n");
          return bytes_written*(-1);
        }else{
          //printf("Found free block at: %d\n",new_block_num);
        }
        //Assign in inode_table
        if(add_block_to_inode(new_block_num, file_inode) < 0){
          printf("Error while adding new block to inode\n");
          return bytes_written*(-1);
        }
        current_block_num = new_block_num;  //Update block number
      }else{
        //found next block, load it
        if(read_blocks(current_block_num, 1, rbuf) < 0){
          printf("Error, could not read data block pointed by write pointer\n");
          return -1;
        }
        if(rbuf[1]=='\0'){
          printf("Error, loaded empty block\n");
        }
      }
				
			
		}	//now we know there is space to write a char
    if(i !=0){
      if(buf[i-1]=='\0'||buf[i]=='\0'){
        printf("\n\n>>Error, cannot write null or after null<<\n");
      }
    }
		rbuf[index_to_write++] = buf[i];	//Add byte to block
		bytes_written++;
		fd_table[fileID].wptr++;
    if(fd_table[fileID].wptr > file_inode->size){
      file_inode->size++;   
    }
		
	}
	//TODO investigate reald nb of bytes written to disk on error
	//flush last block
	if((write_blocks(current_block_num,1,rbuf)) < 0){
		printf("Error while writing indirect pointer to block\n");
		return -1;
	}else{
    //printf("Succesfully written %d char to block (last) %d\n",strlen(rbuf),current_block_num);
  }

	//flush inode
	//We never have to re-read inode table since its always modified in memory as well, so its in sync
	if(sizeof(inode_table)/1024 > INODE_TABLE_LENGHT){
		printf("Unexpected error, inode table is more than 20 blocks\n");
		return -1;
	}
	if((write_blocks(INODE_TABLE_BLK,INODE_TABLE_BLK,inode_table)) < 0){
		printf("Error while inode table to block\n");
		return -1;
	}
  if(bytes_written != strlen(buf)){
    printf(">Error, written %d bytes but buf has length %d and length should be %d\n",bytes_written, strlen(buf),length);
  }
  //check if size is correct
  if(fd_table[fileID].wptr > initial_size){
    int delta_size = file_inode->size - initial_size;
    int length_added = length - (initial_size - initial_wptr);
    if(delta_size != length_added || fd_table[fileID].wptr != (initial_wptr+length)){
      printf(">Error, unmatching lenghts\n");
    }
  }
	return bytes_written;
}
int sfs_fread(int fileID, char *buf, int length){
	if (length <= 0){
		printf("invalid length\n");
		return -1;
	}
	if(fileID < 0){
		printf("Invalid file descriptor\n");
		return -1;
	}
	if(!(fd_table[fileID].inode_index > 0 && fd_table[fileID].inode_index < NUM_INODES)) {
		printf("Invalid file descriptor\n");
		return -1;
	}

	int i;
	int bytes_read = 0;
	int index_to_read;
	//int current_block_num;
	//int int_in_block = 1024/INT_SIZE;
	char read_buffer[length];
	memset(read_buffer,0,length*sizeof(char));
	Inode file_inode = inode_table[fd_table[fileID].inode_index];

  if(file_inode.size == 0){
    printf("Can't read empty file\n");
  }
	
	//Load last block
	//find which is the block pointed by rptr
	int read_loc = fd_table[fileID].rptr / 1024;	//result is something like 5th block
	int read_loc_offset = fd_table[fileID].rptr % 1024;
	int last_read_block;	//find which block does this points to
	if((last_read_block = file_get_nth_block(read_loc,file_inode)) < 0){
		printf("Error, could not get block number of read pointer\n");
	}

	char block_buf[1024];
	if(read_blocks(last_read_block, 1, block_buf) < 0){
		printf("Error, could not read data block pointed by write pointer\n");
		return -1;
	}else{
    //printf("Succesfully read from block %d\n",last_read_block);
  }
  	//block is now in read_buf
	index_to_read = read_loc_offset;
	//current_block_num = last_read_block;
	int old_buflen = 0;
	for(i=0;i<length;i++){//for each byte
		if(fd_table[fileID].rptr > file_inode.size){
			printf("Error, read pointer is out of file\n");
			memcpy(buf,read_buffer, length);
  		return bytes_read-1;
		}
		if(index_to_read >= 1024){	//reached end of block
			//fetch next block
			memset(block_buf,0,1024);	//clear block cache
			read_loc++;
			if((last_read_block = file_get_nth_block(read_loc,file_inode)) < 0){
				printf("Error, could not get block number of read pointer\n");
			}
			if(read_blocks(last_read_block, 1, block_buf) < 0){
				printf("Error, could not read data block pointed by write pointer\n");
				return -1;
			}else{
        //printf("Succesfully read block %d into block_buf\n",last_read_block);
      }

			//TODO catch errors
				
			//set index to 0 and clear buffer
			index_to_read = 0;
			
		}	//now we know there is bytes to read in buf

		//read a byte from block in read_buffer
		read_buffer[i] = block_buf[index_to_read++];
    int buflen = strlen(read_buffer);
    if(buflen == old_buflen){
      //printf("Error, length of read buffer has not changed\n");
    }
    old_buflen = buflen;
		bytes_read++;
		fd_table[fileID].rptr++;
		
	}

  memcpy(buf,read_buffer, length);
  if(length != strlen(buf)){
    printf(">Error, could not read the correct amount of bytes\n");
    printf("Read: length given: %d, length of buffer: %d, bytes read: %d\n",length,strlen(buf),bytes_read);
  }

  //printf("Read: length given: %d, length of buffer: %d, bytes read: %d\n",length,strlen(buf),bytes_read);
  return bytes_read;
}
int sfs_remove(char *file){
	//remove from directory entry
	//get its number
	int file_index = sfs_find_in_directory(file);
	int inode_num = directory[file_index].inode_index;
  //Close the file first
  int fd_entry;
  if((fd_entry = sfs_find_in_fd_table(inode_num)) >= 0){
    printf("Closing file before removing it\n");
    if(sfs_fclose(fd_entry)){
      printf("Error, could not close file, cancelling rm\n");
    }
  }
	//directory[file_index] = {.filename = ""};	//clear the entry
	memset(&directory[file_index], 0, sizeof(Directory_entry));
	Inode *inode = &inode_table[inode_num];
	//free blocks
	int i;
	for(i=0;i<12;i++){	//clear block from direct pointers
		int ptr_value;
		if((ptr_value =inode->ptr[i]) > 0){
			if(set_block_free(ptr_value) < 0){
				printf("Error trying to free block to remove file\n");
				return -1;
			}
		}
	}
	//load indirect pointers block
	int ind_ptr_index = inode->indptr;
  if(ind_ptr_index > 0){    //check if there is an indirect table
      int ind_ptr_block[1024/INT_SIZE];
    if(read_blocks(ind_ptr_index,1,ind_ptr_block) <0 ){
      printf("Error reading indirect pointer block\n");
      return -1;
    }
    for(i = 0; i< 1024/INT_SIZE; i++){
      int ptr_value;
      if((ptr_value = ind_ptr_block[i]) > 0){
        if(set_block_free(ptr_value) < 0){
          printf("Error trying to free block to remove file (indirect ptr)\n");
          return -1;
        }
      }
    }
  }
	
	//remove inode
	//inode_table[inode_num] = {};
	memset(&inode_table[inode_num],0,sizeof(Inode));
	
  	return 0;
}

//-----------------Helpers----------------------

int sfs_fcreate(char *name){	//create file and return its inode_index
  //Check name length
  if(strlen(name) > 20){
    printf("Too many characters in string, maximum is %d\n",20);
    return -1;
  }

  //Add file to directory
  //find empty directory entry
  int new_dir_index=-1;
  if((new_dir_index = sfs_find_empty_dir_entry()) == -1){
    printf("Error while looking for free directory entry\n");
    return -1;
  }

	//create an inode
	//find empty inode
	int new_inode_index=-1;
	if((new_inode_index = sfs_find_empty_inode()) == -1){
		printf("Error while looking for free inode entry\n");
    memset(&directory[new_dir_index], 0, sizeof(Directory_entry));
    printf("Reverting just allocated dir entry\n");
		return -1;
	}
	//Populate inode
	Inode new_inode = {.mode=1};	//Size and pointer are 0
	inode_table[new_inode_index] = new_inode;
	
	//Populate directory entry
	Directory_entry new_dir_entry = { .inode_index = new_inode_index};
  strcpy(new_dir_entry.filename, name);
	directory[new_dir_index] = new_dir_entry;
	//Here we could modify the directory inode if there was anything to do with it

  //Save directory to disk
  if ( write_directory(directory) < 0){    //First block, 1 block
      printf("Error while writing directory to disk\n");
      return -1;
  }

	//Save inode table to disk
	if(sizeof(inode_table)/1024 > INODE_TABLE_LENGHT){
		printf("Unexpected error, inode table is more than 20 blocks\n");
		return -1;
	}
	if((write_blocks(INODE_TABLE_BLK,INODE_TABLE_BLK,inode_table)) < 0){
		printf("Error while inode table to block\n");
		return -1;
	}

	return new_inode_index;
}

int sfs_find_empty_inode(){
	int i;
	for(i=0; i<NUM_INODES; i++){
		if(inode_table[i].mode == 0){
			return i;
		}
	}
	printf("Error, all %i entries in inode table are taken\n",NUM_INODES);
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
    if(directory[i].inode_index < 1){
      continue;
    }
		if(strcmp(directory[i].filename, name)==0){
			return i;
		}
	}
	printf("File %s not found in directory\n",name);
	return -1;
}

int sfs_find_in_fd_table(int i_index){
  //Find a file in the file descriptor table and return its index
  int i;
  for(i=0;i<FD_TABLE_LENGTH;i++){
    if(fd_table[i].inode_index < 1){
      continue;
    }
    if(fd_table[i].inode_index == i_index){
      return i;
    }
  }
  return -1;
}

int set_block_free(int blocknum){	//16 to 24995
	if(blocknum < 16 || blocknum >= 24995 ){
		printf("Error, block number %d is not a data block\n", blocknum);
		return -1;
	}
	//read the bitmap
	//char bitmap[3125]; //bitmap already in cache
	//read_bitmap(bitmap);

	//set the bit
	int bitmap_index = blocknum / 8;	//find which of the 3125 bytes to change
	unsigned char bit_number = blocknum % 8;		//find which bit
	unsigned char bit_value = 1 << bit_number;	//value of this bit
	//check current value
	unsigned char char_value	= bitmap[bitmap_index];
	if(((char_value >> bit_number) & 1)){	//if the bit is free
		printf("Error ,trying to free a block already free\n");
		return -1;
	}
	unsigned char new_char_value = char_value | bit_value;	// set the bit TODO
	bitmap[bitmap_index] = new_char_value;

	//write the bitmap
	int status;
	if((status = write_bitmap(bitmap))<0){
		return -1;
	}
	return 0;
}

int set_block_used(int blocknum){		//we will use 0 for used
	if(blocknum < 16 || blocknum >= 24995 ){
		printf("Error, block number %d is not a data block\n", blocknum);
		return -1;
	}
	//read the bitmap
	//char bitmap[3125];   //Already in memory
	//read_bitmap(bitmap);

	//set the bit
	int bitmap_index = blocknum / 8;	//find which of the 3125 bytes to change
	unsigned char bit_number = blocknum % 8;		//find which bit
	unsigned char bit_value = 1 << bit_number;	//value of this bit
	//check current value
	unsigned char char_value	= bitmap[bitmap_index];
	if(!((char_value >> bit_number) & 1)){	//if the bits where the same then true
		printf("Error ,trying to use an occupied block\n");
		return -1;
	}
	unsigned char new_char_value = char_value - bit_value;	// clear the bit
	bitmap[bitmap_index] = new_char_value;

	//write the bitmap
	int status;
	if((status = write_bitmap(bitmap))<0){
		return -1;
	}
	return 0;
}

int find_free_block(){	//Also set the block as used
	//read_bitmap(bitmap);   //Already in cache
	int i;
	for(i=2;i<3125;i++){		//Bytes before 2 are used for SB and inodes
		if(bitmap[i]){		//There is one free block in this char
			int j;
			for(j=0;j<8;j++){
				if((bitmap[i] >> j) & 1 ){
					int found_block = 8*i+j;
					if(set_block_used(found_block)<0){
						printf("Error setting found bit as used\n");
					}
					return found_block;		//return the free block number
				}
			}
			printf("Error, unexpected\n");
		}
	}
	printf("Error, no free block found\n");
	return -1;
}

int add_block_to_inode(int blocknum, Inode *inode){
	int i;
	for(i=0; i<12; i++){
		if(!(inode->ptr[i] > 0)){	//empty ptr, use this one
			inode->ptr[i] = blocknum;
			return 0;
		}
	}
	int int_in_block = 1024/INT_SIZE;
	int ind_ptr[int_in_block];	
	memset( ind_ptr, 0, int_in_block*INT_SIZE);	//Init the arrays with zeros
	int ind_ptr_block;
	if(!((ind_ptr_block = inode->indptr) > 0)){	//indirect pointer does not exists
		//Create the block
		if((ind_ptr_block = find_free_block()) < 0){
			printf("Error while looking for free block (new ind ptr)\n");
			return -1;
		}
		printf("Assigned data block for indirect pointer\n");
    //initialize the indirect pointer array
    memset(ind_ptr,0,sizeof(int)*int_in_block);
		//add the pointer at the beggining
		ind_ptr[0] = blocknum;
    inode->indptr=ind_ptr_block;
	}else{
		//read the block
		if(read_blocks(ind_ptr_block,1,ind_ptr) < 0){
			printf("Error while reading inode indirect pointer block\n");
			return -1;
		}
		//if not full add a pointer
		for(i=0;i<int_in_block;i++){
			if(ind_ptr[i] == 0){
				ind_ptr[i] = blocknum;
				break;
			}
		}
		if(i == int_in_block){	//catch no more space
			printf("No more space in inode, all %d pointers are taken\n", 12+int_in_block);
			return -1;
		}
	}
	//now pointer exists in memory
	//write the block

	if(write_blocks(ind_ptr_block,1,ind_ptr) < 0){
			printf("Error while writing indirect pointer to block\n");
			return -1;
	}
  if(write_inode_table(inode_table) < 0){
    printf("Error writing inode table\n");
    return -1;
  }
	return 0;
}

int file_get_nth_block(int blocknum, Inode inode){
	int int_in_block = 1024/INT_SIZE;
	int block_index;
	if(blocknum >= 0 && blocknum < 12){
		block_index = inode.ptr[blocknum];
	}else if( blocknum < int_in_block+12){	//block pointed by indirect pointer
		int ind_ptr_block = inode.indptr;		//get num of pointers block

		int ind_ptrs[int_in_block];
		if(read_blocks(ind_ptr_block,1,ind_ptrs) < 0){	//Read that block in array
			printf("Error while reading indirect pointer block\n");
			return -1;
		}
		block_index = ind_ptrs[blocknum-12];

	}else{
		printf("Error, write pointer points to inode block %d, which is outside file\n", blocknum);
		return -1;
	}
	return block_index;
}

int write_bitmap(unsigned char *bitmap){
  if(write_blocks(FREE_BITMAP_BLK,FREE_BITMAP_LENGTH, bitmap)<0){
    printf("Error while writing bitmap to disk\n");
    return -1;
  }
  return 0;
}

int read_bitmap(unsigned char *bitmap){
  if(read_blocks(FREE_BITMAP_BLK,FREE_BITMAP_LENGTH, bitmap)<0){
    printf("Error while reading bitmap from disk\n");
    return -1;
  }
  return 0;
}

int write_inode_table(Inode *i_table){
  if(write_blocks(INODE_TABLE_BLK,INODE_TABLE_LENGHT, i_table)<0){
    printf("Error while writing inode table to disk\n");
    return -1;
  }
  return 0;
}

int read_inode_table(Inode *i_table){
  if(read_blocks(INODE_TABLE_BLK,INODE_TABLE_LENGHT, i_table)<0){
    printf("Error while reading inode table from disk\n");
    return -1;
  }
  return 0;
}

int write_directory(Directory_entry *dir){  //Handle type conversion
  if(write_blocks(DIRECTORY_BLK_INDEX,DIRECTORY_BLOCKS, dir)<0){
    printf("Error while writing directory to disk\n");
    return -1;
  }
  return 0;
}

int read_directory(Directory_entry *dir){   //handle type conversion
  if(read_blocks(DIRECTORY_BLK_INDEX,DIRECTORY_BLOCKS, dir)<0){
    printf("Error while reading directory from disk\n");
    return -1;
  }
  return 0;
}

int write_super_block(int *s_block){  //Handle type conversion
  if(write_blocks(0,1, s_block)<0){
    printf("Error while writing directory to disk\n");
    return -1;
  }
  return 0;
}

int read_super_block(int *s_block){   //handle type conversion
  if(read_blocks(0,1, s_block)<0){
    printf("Error while reading directory from disk\n");
    return -1;
  }
  return 0;
}