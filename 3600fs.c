/*
 * CS3600, Spring 2014
 * Project 2 Starter Code
 * (c) 2013 Alan Mislove
 *
 * This file contains all of the basic functions that you will need 
 * to implement for this project.  Please see the project handout
 * for more details on any particular function, and ask on Piazza if
 * you get stuck.
 */

#define FUSE_USE_VERSION 26

#ifdef linux
/* For pread()/pwrite() */
#define _XOPEN_SOURCE 500
#endif

#define _POSIX_C_SOURCE 199309

#include <time.h>
#include <fuse.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <assert.h>
#include <sys/statfs.h>

#ifdef HAVE_SETXATTR
#include <sys/xattr.h>
#endif

#include "3600fs.h"
#include "disk.h"

vcb* disk_vcb;
dirent** dirents;
fatent** fatents;

/*
 * Initialize filesystem. Read in file system metadata and initialize
 * memory structures. If there are inconsistencies, now would also be
 * a good time to deal with that. 
 *
 * HINT: You don't need to deal with the 'conn' parameter AND you may
 * just return NULL.
 *
 */
static void* vfs_mount(struct fuse_conn_info *conn) {
  fprintf(stderr, "vfs_mount called\n");

  // Do not touch or move this code; connects the disk
  dconnect();

  /* 3600: YOU SHOULD ADD CODE HERE TO CHECK THE CONSISTENCY OF YOUR DISK
           AND LOAD ANY DATA STRUCTURES INTO MEMORY */

  // Get VCB Data
  char vcb_data[BLOCKSIZE];
  if (dread(0, vcb_data) < 0) { fprintf(stderr, "dread failed for block 0\n"); }
  // Cast our data from disk into a vcb structure
  disk_vcb = calloc(1, sizeof(vcb));
  memcpy(disk_vcb, &vcb_data, sizeof(vcb));

  printf("MAGIC: %i\n", disk_vcb->magic); 
  printf("BLOCKSIZE: %i\n", disk_vcb->blocksize); 
  printf("DE_START: %i\n", disk_vcb->de_start); 
  printf("DE_LENGTH: %i\n", disk_vcb->de_length); 
  printf("FAT_START: %i\n", disk_vcb->fat_start); 
  printf("FAT_LENGTH: %i\n", disk_vcb->fat_length); 
  printf("DB_START: %i\n", disk_vcb->db_start); 

  if (disk_vcb->magic != 666) { fprintf(stderr, "magic number mismatch\n"); exit(1); }
 
  // Get Directory Entries
  // Start reading blocks at the dirent start, until the start + the length
  for (int i = disk_vcb->de_start; i < disk_vcb->de_start + disk_vcb->de_length; i++) {
    // Read the block off the disk into a character buffer
    char dirent_buf[BLOCKSIZE];
    if (dread(i, dirent_buf) < 0) { fprintf(stderr, "dread failed\n"); }

    // Move the character buffer into a dirent struct
    dirent* tmp = calloc(1, sizeof(dirent));
    memcpy(tmp, &dirent_buf, sizeof(dirent));

    // Place that struct into the global array
    // i - disk_vcb->de_start + 1 is the amount of entries in the array
    dirents = realloc(dirents, (i - disk_vcb->de_start + 1) * sizeof(dirent*));
    dirents[i - disk_vcb->de_start] = tmp;
  }  

  // Get FAT Entries
  for (int j = disk_vcb->fat_start; j < disk_vcb->fat_start + disk_vcb->fat_length; j++) {
    char fat_buf[BLOCKSIZE];
    if (dread(j, fat_buf) < 0) { fprintf(stderr, "dread failed\n"); }
    fatent* fat_tmp = calloc(1, sizeof(fatent));
    memcpy(fat_tmp, &fat_buf, sizeof(fatent));

    fatents = realloc(fatents, (j - disk_vcb->fat_start + 1) * sizeof(fatent*));
    fatents[j - disk_vcb->fat_start] = fat_tmp;
  }
  return NULL;
}

/*
 * Called when your file system is unmounted.
 *
 */
static void vfs_unmount (void *private_data) {
  fprintf(stderr, "vfs_unmount called\n");

  /* 3600: YOU SHOULD ADD CODE HERE TO MAKE SURE YOUR ON-DISK STRUCTURES
           ARE IN-SYNC BEFORE THE DISK IS UNMOUNTED (ONLY NECESSARY IF YOU
           KEEP DATA CACHED THAT'S NOT ON DISK */

  // Do not touch or move this code; unconnects the disk
  dunconnect();
}

/* 
 *
 * Given an absolute path to a file/directory (i.e., /foo ---all
 * paths will start with the root directory of the CS3600 file
 * system, "/"), you need to return the file attributes that is
 * similar stat system call.
 *
 * HINT: You must implement stbuf->stmode, stbuf->st_size, and
 * stbuf->st_blocks correctly.
 *
 */
static int vfs_getattr(const char *path, struct stat *stbuf) {
  // I think this is supposed to be removed? 
  fprintf(stderr, "vfs_getattr called\n");
  
  if (strrchr(path, '/') > path) {
    fprintf(stderr, "Unable to get_attr on a multilevel dir");
  }

  // Do not mess with this code 
  stbuf->st_nlink = 1; // hard links
  stbuf->st_rdev  = 0;
  stbuf->st_blksize = BLOCKSIZE;

  /* 3600: YOU MUST UNCOMMENT BELOW AND IMPLEMENT THIS CORRECTLY */
 
 int found = 0; // Flag to determine if the file was found. 0 if no, 1 if yes
 int i;
 for (i = disk_vcb->de_start; i < disk_vcb->de_start + disk_vcb->de_length; i++) { // May need diff. way to get iterations length
   if (dirents[i]->valid = 1 && strcmp((path+1), dirents[i]->name) == 0) {
     found = 1;
     break;
   }
 }
 
 if (found == 0) {
   return -ENOENT;
 } 
 else {
    // if (The path represents the root directory)
    if (*path == '/') { //if the first char is a '/', it is referencing the root dir
      stbuf->st_mode  = 0777 | S_IFDIR;
    } else {
      stbuf->st_mode  = dirents[i]->mode | S_IFREG; 
    }

    stbuf->st_uid     = dirents[i]->user; // file uid
    stbuf->st_gid     = dirents[i]->group; // file gid
    stbuf->st_atime   = dirents[i]->access_time.tv_sec; // access time 
    stbuf->st_mtime   = dirents[i]->modify_time.tv_sec; // modify time
    stbuf->st_ctime   = dirents[i]->create_time.tv_sec; // create time
    stbuf->st_size    = dirents[i]->size; // file size
    stbuf->st_blocks  = ceil(dirents[i]->size / 512); // file size in blocks: TODO not sure how to get this
                                                      // maybe like this? Can depend on what unit the size is given in

    return 0;
  }
}

/** Read directory
 *
 * Given an absolute path to a directory, vfs_readdir will return 
 * all the files and directories in that directory.
 *
 * HINT:
 * Use the filler parameter to fill in, look at fusexmp.c to see an example
 * Prototype below
 *
 * Function to add an entry in a readdir() operation
 *
 * @param buf the buffer passed to the readdir() operation
 * @param name the file name of the directory entry
 * @param stat file attributes, can be NULL
 * @param off offset of the next entry or zero
 * @return 1 if buffer is full, zero otherwise
 * typedef int (*fuse_fill_dir_t) (void *buf, const char *name,
 *                                 const struct stat *stbuf, off_t off);
 *			   
 * Your solution should not need to touch fi
 *
 */
static int vfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                       off_t offset, struct fuse_file_info *fi)
{
  if (strrchr(path, '/') > path) {
    fprintf(stderr, "Unable to readdir on a multilevel dir");
    return -1;
  }

    // If the given path is not the root of the file system, throw error
    if (strcmp(path, "/") != 0) {
      return -1;
    }

    return 0;
}

/*
 * Given an absolute path to a file (for example /a/b/myFile), vfs_create 
 * will create a new file named myFile in the /a/b directory.
 *
 */
static int vfs_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
   
   if (strrchr(path, '/') > path) {
     fprintf(stderr, "Unable to create on a multilevel dir");
     return -1;
   }

 
    // If the file already exists, throw an error
    struct stat *stbuf = NULL; 
    if (vfs_getattr(path, stbuf) != -ENOENT) {
       return -EEXIST;
    }

    // Create a new dirent for this file 
    dirent* new_de = calloc(1, sizeof(dirent)); // maybe don't need to calloc it
    new_de->valid = 1;
    new_de->mode = mode;
    new_de->user = geteuid();
    new_de->group = getegid();
    // new_de->size = ???
    // new_de->first_block = ???
    struct timespec mytime;
    clock_gettime(CLOCK_REALTIME, &mytime);
    new_de->create_time = mytime;
    new_de->modify_time = mytime;
    new_de->access_time = mytime;
    strcpy(new_de->name, (path + 1));
    //new_de.name = "I'manewfile";//(path + 1); // probably need to parse the path, get the actual name
                            // Make sure this pointer math is right. It should skip right past
                            // the "/" and start at first char of the path
                            // Note: only works due to single-level directories
    // TODO need to do stuff with the fatent 
    // Maybe need a call to open()???
    return 0;
}

/*
 * The function vfs_read provides the ability to read data from 
 * an absolute path 'path,' which should specify an existing file.
 * It will attempt to read 'size' bytes starting at the specified
 * offset (offset) from the specified file (path)
 * on your filesystem into the memory address 'buf'. The return 
 * value is the amount of bytes actually read; if the file is 
 * smaller than size, vfs_read will simply return the most amount
 * of bytes it could read. 
 *
 * HINT: You should be able to ignore 'fi'
 *
 */
static int vfs_read(const char *path, char *buf, size_t size, off_t offset,
                    struct fuse_file_info *fi)
{

    return 0;
}

/*
 * The function vfs_write will attempt to write 'size' bytes from 
 * memory address 'buf' into a file specified by an absolute 'path'.
 * It should do so starting at the specified offset 'offset'.  If
 * offset is beyond the current size of the file, you should pad the
 * file with 0s until you reach the appropriate length.
 *
 * You should return the number of bytes written.
 *
 * HINT: Ignore 'fi'
 */
static int vfs_write(const char *path, const char *buf, size_t size,
                     off_t offset, struct fuse_file_info *fi)
{

  /* 3600: NOTE THAT IF THE OFFSET+SIZE GOES OFF THE END OF THE FILE, YOU
           MAY HAVE TO EXTEND THE FILE (ALLOCATE MORE BLOCKS TO IT). */

  return 0;
}

/**
 * This function deletes the last component of the path (e.g., /a/b/c you 
 * need to remove the file 'c' from the directory /a/b).
 */
static int vfs_delete(const char *path)
{
   
    // If the path given is a multilevel path 
    if (strrchr(path, '/') > path) {
      fprintf(stderr, "Unable to create on a multilevel dir");
      return -1;
    }

    /* 3600: NOTE THAT THE BLOCKS CORRESPONDING TO THE FILE SHOULD BE MARKED
             AS FREE, AND YOU SHOULD MAKE THEM AVAILABLE TO BE USED WITH OTHER FILES */
    struct stat *stbuf = NULL; 
    if (vfs_getattr(path, stbuf) == -ENOENT) {
      fprintf(stderr, "Can't delete a file that doesn't exist"); 
      return -EEXIST;
    }

    int found = 0; // Flag to determine if the file was found. 0 if no, 1 if yes
    int i;
    for (i = disk_vcb->de_start; i < disk_vcb->de_start + disk_vcb->de_length; i++) { // May need diff. way to get iterations length
       if (dirents[i]->valid = 1 && strcmp((path+1), dirents[i]->name) == 0) {
         found = 1;
         break;
       }
    }

    dirents[i]->valid = 0;
    // mark the fat entry as unused
    //TODO maybe works like this?
    unsigned int tmp = dirents[i]->first_block;
    fatents[tmp]->used = 0;

    return 0;
}

/*
 * The function rename will rename a file or directory named by the
 * string 'oldpath' and rename it to the file name specified by 'newpath'.
 *
 * HINT: Renaming could also be moving in disguise
 *
 */
static int vfs_rename(const char *from, const char *to)
{

    return 0;
}


/*
 * This function will change the permissions on the file
 * to be mode.  This should only update the file's mode.  
 * Only the permission bits of mode should be examined 
 * (basically, the last 16 bits).  You should do something like
 * 
 * fcb->mode = (mode & 0x0000ffff);
 *
 */
static int vfs_chmod(const char *file, mode_t mode)
{

    return 0;
}

/*
 * This function will change the user and group of the file
 * to be uid and gid.  This should only update the file's owner
 * and group.
 */
static int vfs_chown(const char *file, uid_t uid, gid_t gid)
{

    return 0;
}

/*
 * This function will update the file's last accessed time to
 * be ts[0] and will update the file's last modified time to be ts[1].
 */
static int vfs_utimens(const char *file, const struct timespec ts[2])
{

    return 0;
}

/*
 * This function will truncate the file at the given offset
 * (essentially, it should shorten the file to only be offset
 * bytes long).
 */
static int vfs_truncate(const char *file, off_t offset)
{

  /* 3600: NOTE THAT ANY BLOCKS FREED BY THIS OPERATION SHOULD
           BE AVAILABLE FOR OTHER FILES TO USE. */

    return 0;
}


/*
 * You shouldn't mess with this; it sets up FUSE
 *
 * NOTE: If you're supporting multiple directories for extra credit,
 * you should add 
 *
 *     .mkdir	 = vfs_mkdir,
 */
static struct fuse_operations vfs_oper = {
    .init    = vfs_mount,
    .destroy = vfs_unmount,
    .getattr = vfs_getattr,
    .readdir = vfs_readdir,
    .create	 = vfs_create,
    .read	 = vfs_read,
    .write	 = vfs_write,
    .unlink	 = vfs_delete,
    .rename	 = vfs_rename,
    .chmod	 = vfs_chmod,
    .chown	 = vfs_chown,
    .utimens	 = vfs_utimens,
    .truncate	 = vfs_truncate,
};

int main(int argc, char *argv[]) {
    /* Do not modify this function */
    umask(0);
    if ((argc < 4) || (strcmp("-s", argv[1])) || (strcmp("-d", argv[2]))) {
      printf("Usage: ./3600fs -s -d <dir>\n");
      exit(-1);
    }
    return fuse_main(argc, argv, &vfs_oper, NULL);
}

