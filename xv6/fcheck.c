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


int numinodes;


typedef struct Datablock{

	bool inuse;
	int usecount;
	int inode;

}Datablock;

typedef struct Block{
	char data[BSIZE];
}Block;

void throwerr(char *string)
{
	char buf[256];
	sprintf(buf, "ERROR: %s\n", string);
	fprintf(stderr, buf);
	exit(1);
}


bool isBlockUsed(char *addr, int blocknum)
{
	int bmapblocknum = BBLOCK(blocknum, numinodes);
	int bmapbit = blocknum % BPB;

	char * bmap = addr + (bmapblocknum * BLOCK_SIZE);

	int m = 1 << (bmapbit % 8);
	if((bmap[bmapbit/8] & m) == 0) //bit not set. 
		return false;		
	else
		return true;
}


// Returns a char pointer to the beginning of the specified block number
char *getBlock(char *startaddr, int blocknum)
{
	return startaddr + (blocknum * BLOCK_SIZE);
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
	numinodes = sb->ninodes;

	Datablock dblocks[sb->size];
	memset(dblocks, 0, sizeof(Datablock) * sb->size);

	printf(" size of struct dinode - %d\n", sizeof(struct dinode));
	// loops through the inode BLOCKS
	for(int inum = 0; inum < sb->ninodes; inum++)
	{
		// check 1: Each inode is either unallocated or one of the valid types
		if(dip[inum].type != 0 && dip[inum].type != T_DIR && dip[inum].type != T_FILE && dip[inum].type != T_DEV)
			throwerr("bad inode");

		// check 2:  For in-use inodes, each block address that is used by the inode is valid (points to a valid data block address within the image)
		if(dip[inum].type != 0) // need not use BLOCK_SIZE since dip[].addrs via balloc returns the block number not the address
		{
			uint baddress;
			char *endaddress = addr + (sb->size * BLOCK_SIZE) - 1;
			// go through direct blocks
			for(int b = 0; b < NDIRECT; b++)
			{
				baddress = dip[inum].addrs[b] * BLOCK_SIZE; // need to check if used?
				if(addr + baddress > endaddress)
					throwerr("bad direct address in inode");
			}
			
			// check the indirect block
			baddress = dip[inum].addrs[NDIRECT] * BLOCK_SIZE;
			if(addr + baddress > endaddress)
				throwerr("bad indirect address in inode");
			//else
				//printf("indirect block num - %d, addr = %p, endaddr - %p\n", dip[inum].addrs[NDIRECT], addr + baddress, endaddress);

		}

		//check 3: Root directory exists, its inode number is 1, and the parent of the root directory is itself
		if(inum == ROOTINO)
		{
			if(dip[inum].type != T_DIR)
				throwerr("root directory does not exist");

			bool parentisitself = false;
			de = (struct dirent *) (addr + (dip[inum].addrs[0])*BLOCK_SIZE);

			// print the entries in the first block of root dir 

			n = dip[inum].size/sizeof(struct dirent);
			for (i = 0; i < n; i++,de++){
				if(strcmp(de->name, "..") == 0 && de->inum == inum)
				{
					parentisitself = true;
					break;
				}
			}

			if(!parentisitself)
				throwerr("root directory does not exist");

		}

		// check 4: Each directory contains . and .. entries, and the . entry points to the directory itself

		if(dip[inum].type == T_DIR)
		{
			bool dotfound, doubledotfound, ptstoitself;
			dotfound = doubledotfound = ptstoitself = false;

			de = (struct dirent *) (addr + (dip[inum].addrs[0])*BLOCK_SIZE); // is one block from addrs sufficient?

			n = dip[inum].size/sizeof(struct dirent);
			for (i = 0; i < n; i++,de++){
				if(strcmp(de->name, "..") == 0)
				{
					doubledotfound = true;
				}
				if(strcmp(de->name, ".") == 0)
				{
					dotfound = true;
					if(de->inum == inum)
						ptstoitself = true;
				}
			}

			if(!doubledotfound || !dotfound || !ptstoitself)
				throwerr("directory not properly formatted.");

		}

		// check 5: For in-use inodes, each block address in use is also marked in use in the bitmap.

		if(dip[inum].type != 0)
		{
			// go through direct blocks
			for(int b = 0; b < NDIRECT; b++)
			{
				 int blocknum = dip[inum].addrs[b]; // data blocknum
				 if(blocknum == 0) 
				 	continue;
				 int bmapblocknum = BBLOCK(blocknum, sb->ninodes);
				 int bmapbit = blocknum % BPB;

				 char * bmap = addr + (bmapblocknum * BLOCK_SIZE);

				 int m = 1 << (bmapbit % 8);
				 if((bmap[bmapbit/8] & m) == 0) //bit not set. 
				 {
					throwerr("1. address used by inode but marked free in bitmap.");		
				 }

			}
			
			// go through the indirect block direct blocks
			int indirectblocknum = dip[inum].addrs[NDIRECT];
			if(indirectblocknum != 0)
			{
				if(!isBlockUsed(addr, indirectblocknum))
					throwerr("2. address used by inode but marked free in bitmap.");

				char *indirectblock = getBlock(addr, indirectblocknum);
				for(int index = 0; index < NINDIRECT; index++)
				{
					if(!isBlockUsed(addr, (uint)indirectblock[index]))
					{
						printf("direct block num - %d, index = %d\n", (uint)indirectblock[index], index);	
						throwerr("3. address used by inode but marked free in bitmap.");
					}
				}
			}
			
		}


		// check 6: For blocks marked in-use in bitmap, the block should actually be in-use in an 
		// inode or indirect block somewhere
		
		int numdatablocks = sb->nblocks;
		int bitmapblocknum = 2 + (sb->ninodes/IPB); 
		char * bmap = addr + bitmapblocknum * BLOCK_SIZE;

		if(dip[inum].type != 0)
		{
			// go through direct blocks
			for(int b = 0; b < NDIRECT; b++)
			{
				 int blocknum = dip[inum].addrs[b]; // data blocknum
				 if(blocknum == 0) 
				 	continue;
				 int bmapblocknum = BBLOCK(blocknum, sb->ninodes);
				 int bmapbit = blocknum % BPB;

				 char * bmap = addr + (bmapblocknum * BLOCK_SIZE);

				 int m = 1 << (bmapbit % 8);
				 if((bmap[bmapbit/8] & m) == 0) //bit not set. 
				 {
					throwerr("1. address used by inode but marked free in bitmap.");		
				 }

			}
			
			// go through the indirect block direct blocks
			int indirectblocknum = dip[inum].addrs[NDIRECT];
			if(indirectblocknum != 0)
			{
				if(!isBlockUsed(addr, indirectblocknum))
					throwerr("2. address used by inode but marked free in bitmap.");

				char *indirectblock = getBlock(addr, indirectblocknum);
				for(int index = 0; index < NINDIRECT; index++)
				{
					if(!isBlockUsed(addr, (uint)indirectblock[index]))
					{
						printf("direct block num - %d, index = %d\n", (uint)indirectblock[index], index);	
						throwerr("3. address used by inode but marked free in bitmap.");
					}
				}
			}
			
		}

	}

	exit(0);

}

