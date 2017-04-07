/*

Sfuse
  (c) 2016 2016 INESC TEC.  by J. Paulo (ADD Other authors here)

*/


#include "nopalign.h"



int nop_align_read (const char *path, char *buf, size_t size, off_t offset,void *fi,struct fuse_operations nextlayer){

    return nextlayer.read(path, buf, size, offset,fi);

}



int nop_align_write (const char *path, const char *buf, size_t size, off_t offset,void *fi,struct fuse_operations nextlayer){
	
	return nextlayer.write(path, buf, size, offset, fi);

}

int nop_align_create (const char *path, mode_t mode, void *fi,struct fuse_operations nextlayer)
{
    
    return nextlayer.create(path, mode,fi);
    
}

int nop_align_open (const char *path, void *fi,struct fuse_operations nextlayer)
{
    return nextlayer.open(path,fi);
}

off_t nop_align_get_file_size(const char *path, struct fuse_file_info *fi, struct fuse_operations nextlayer){
    struct stat st;

    if(fi!=NULL){
        nextlayer.fgetattr(path, &st,fi);
        DEBUG_MSG("nop align fi is not null\n");
    }else{
        nextlayer.getattr(path, &st);
        DEBUG_MSG("nop align fi is null\n");
    }

    return st.st_size;
}

int nop_align_truncate (const char* path, off_t size,struct fuse_file_info *fi_in,struct fuse_operations nextlayer){

    int res;

	if(fi_in==NULL){
        res = nextlayer.truncate(path,size);
    }else{
        res = nextlayer.ftruncate(path,size,fi_in);
    }
    return res;
}