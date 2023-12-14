#include <stdio.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>
#include <stdbool.h>

#include "types.h"
#include "fs.h"

#define BLOCK_SIZE (BSIZE)

#define COLOR_BOLD	"\e[1m"
#define COLOR_OFF	"\e[m"
#define ENDL		"\n"


#define T_DIR  1   // Directory
#define T_FILE 2   // File
#define T_DEV  3   // Special device


void throwerr(char *string)
{
	char buf[256];
	sprintf(buf, "ERROR: %s\n", string);
	fprintf(stderr, buf);
	exit(1);
}


int
main(int argc, char *argv[])
{
  int r,i,n,fsfd;
  char *addr;
  struct dinode *dip;
  struct superblock *sb;
  struct dirent *de;
  struct stat statbuf;

  if(argc < 2){
    fprintf(stderr, "Usage: fcheck <file_system_image>\n");
    exit(1);
  }

  fsfd = open(argv[1], O_RDONLY);
  if(fsfd < 0){
    fprintf(stderr, "image not found\n");
    exit(1);
  }

  /*DONE: Dont hard code the size of file. Use fstat to get the size */
  assert(fstat(fsfd, &statbuf) == 0);

  int filesize = statbuf.st_size;

  addr = mmap(NULL, filesize, PROT_READ, MAP_PRIVATE, fsfd, 0);
  if (addr == MAP_FAILED){
	perror("mmap failed");
	exit(1);
  }
  /* read the super block */
  sb = (struct superblock *) (addr + 1 * BLOCK_SIZE);
  printf("fs size %d, no. of blocks %d, no. of inodes %d \n", sb->size, sb->nblocks, sb->ninodes);

  /* read the inodes */
  dip = (struct dinode *) (addr + IBLOCK((uint)0)*BLOCK_SIZE); 
  printf("begin addr %p, begin inode %p , offset %d \n", addr, dip, (char *)dip -addr);

  // read root inode
  printf("Root inode  size %d links %d type %d \n", dip[ROOTINO].size, dip[ROOTINO].nlink, dip[ROOTINO].type);

  // get the address of root dir 
  de = (struct dirent *) (addr + (dip[ROOTINO].addrs[0])*BLOCK_SIZE);

  // print the entries in the first block of root dir 

  n = dip[ROOTINO].size/sizeof(struct dirent);
  for (i = 0; i < n; i++,de++){
 	printf(" inum %d, name %s ", de->inum, de->name);
  	printf("inode  size %d links %d type %d \n", dip[de->inum].size, dip[de->inum].nlink, dip[de->inum].type);
  }

  printf("============ check started ================\n");

  printf(" size of struct dinode - %d\n", sizeof(struct dinode));
  // loops through the inode BLOCKS
  for(i = 0; i < sb->ninodes; i++)
  {
	//printf("type - %d, size - %d, links - %d, addrs - %p, inode num - %d, block offset - %d\n", dip[i].type, dip[i].size, dip[i].nlink, dip[i].addrs, i, (char*)dip - addr);
	//
	
	if((dip[i].type != 0 && (dip[i].type != T_DIR || dip[i].type != T_FILE || dip[i].type != T_DEV)))
	{
		throwerr("bad inode");
	}
  }



  exit(0);

}

