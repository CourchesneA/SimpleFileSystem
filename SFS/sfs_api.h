//Functions you should implement. 
//Return -1 for error besides mksfs

typedef struct Inode {
	int mode;
	int linkcnt;
	int uidl;
	int gid;
	int size;
	int ptr[12];
	int indptr;
} Inode;

typedef struct Directory_entry {
	char *filename;
	int inode_index;
} Directory_entry;

typedef struct File_descriptor_entry {
	char *fileID;
	int inode_index;
	int rptr;
	int wptr;
} File_descriptor_entry;

void mksfs(int fresh);
int sfs_get_next_file_name(char *fname);
int sfs_get_file_size(char* path);
int sfs_fopen(char *name);
int sfs_fclose(int fileID);
int sfs_frseek(int fileID, int loc);
int sfs_fwseek(int fileID, int loc);
int sfs_fwrite(int fileID, char *buf, int length);
int sfs_fread(int fileID, char *buf, int length);
int sfs_remove(char *file);
