//Functions you should implement. 
//Return -1 for error besides mksfs



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
