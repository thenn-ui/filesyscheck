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

#define T_DIR  1   // Directory
#define T_FILE 2   // File
#define T_DEV  3   // Special device


#define DIRECTADDR 1
#define INDIRECTADDR 2

int numinodes;

typedef struct Datablock{

	bool bitset;
	int usecount;
	int inode;
	int type;

}Datablock;


typedef struct Inode{
	int inuse;
	int refcount;
	int parentinode;
}Inode;


void throwerr(char *string)
{
	char buf[256];
	sprintf(buf, "ERROR: %s\n", string);
	fprintf(stderr, "%s", buf);
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
	int i,n,fsfd;
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
		fprintf(stderr, "image not found.\n");
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

	/* read the inodes */
	dip = (struct dinode *) (addr + IBLOCK((uint)0)*BLOCK_SIZE);

	numinodes = sb->ninodes;

	Datablock dblocks[sb->size];
	memset(dblocks, 0, sizeof(Datablock) * sb->size);

	Inode inodes[sb->ninodes];
	memset(inodes, 0, sizeof(Inode) * sb->ninodes);

	// loops through the inode BLOCKS
	for(int inum = 0; inum < sb->ninodes; inum++)
	{
		// check 1: Each inode is either unallocated or one of the valid types
		if(dip[inum].type != 0 && dip[inum].type != T_DIR && dip[inum].type != T_FILE && dip[inum].type != T_DEV)
			throwerr("bad inode.");

		// check 2:  For in-use inodes, each block address that is used by 
		// the inode is valid (points to a valid data block address within the image)
		if(dip[inum].type != 0) 
		{
			inodes[inum].inuse = true;
			int blocknum = 0;
			// go through direct blocks
			for(int b = 0; b < NDIRECT; b++)
			{
				blocknum = dip[inum].addrs[b];
				if(blocknum >= sb->size || blocknum < 0)
					throwerr("bad direct address in inode.");
			}
			
			// check the indirect blocks
			blocknum = dip[inum].addrs[NDIRECT];
			if(blocknum >= sb->size || blocknum < 0)
				throwerr("bad indirect address in inode.");

			int indirectblocknum = blocknum;
			if(indirectblocknum != 0)
			{
				uint *indirectblock = (uint*) getBlock(addr, indirectblocknum); //check for zero entries
				for(int index = 0; index < NINDIRECT; index++)
				{
					if(indirectblock[index] == 0) // empty entry
						continue;
					if(indirectblock[index] >= sb->size || indirectblock[index] < 0)
						throwerr("bad indirect address in inode.");
				}
			}

		}

		//check 3: Root directory exists, its inode number is 1, and the parent 
		//of the root directory is itself
		if(inum == ROOTINO)
		{
			if(dip[inum].type != T_DIR)
				throwerr("root directory does not exist.");

			bool parentisitself = false;
			de = (struct dirent *) (addr + (dip[inum].addrs[0])*BLOCK_SIZE);

			n = dip[inum].size/sizeof(struct dirent);
			for (i = 0; i < n; i++,de++){
				if(strcmp(de->name, "..") == 0 && de->inum == inum)
				{
					parentisitself = true;
					break;
				}
			}

			if(!parentisitself)
				throwerr("root directory does not exist.");

		}

		// check 4: Each directory contains . and .. entries, and the . entry points 
		// to the directory itself
		if(dip[inum].type == T_DIR)
		{
			bool dotfound, doubledotfound, ptstoitself;
			dotfound = doubledotfound = ptstoitself = false;

			de = (struct dirent *) (addr + (dip[inum].addrs[0])*BLOCK_SIZE); 

			int DPB = BLOCK_SIZE/sizeof(struct dirent); // directory entries per block
			n = dip[inum].size/sizeof(struct dirent);

			int blockstraversed;
			bool indblocktraversed = false;

			for (i = 0, blockstraversed = 0; i < n; i++,de++)
			{
				if(i % DPB == 0)
				{
					blockstraversed++;	
				}
				if(!indblocktraversed && blockstraversed > NDIRECT) //encountered the indirect block, switch to the direct block pointers
				{
					uint firstblocknum = *(uint*) getBlock(addr, dip[inum].addrs[NDIRECT]);
					de = (struct dirent *) getBlock(addr, firstblocknum);
					indblocktraversed = true;
				}
				
				if(strcmp(de->name, "..") == 0)
				{
					doubledotfound = true;
					inodes[inum].parentinode = de->inum;
				}
				if(strcmp(de->name, ".") == 0)
				{
					dotfound = true;
					if(de->inum == inum)
						ptstoitself = true;
				}

				//check 10: For each inode number that is referred to in a valid directory, 
				//it is actually marked free
				if(de->inum != 0 && dip[de->inum].type == 0)
				{
					throwerr("inode referred to in directory but marked free.");
				}

				// Book keeping FOR check 9
				if(de->inum != 0 && dip[de->inum].type != 0)
				{
					if(strcmp(de->name, "..") != 0 && strcmp(de->name, ".") != 0)
					{
						inodes[de->inum].refcount++;
					}
					if(inum == ROOTINO)
						inodes[inum].refcount = 1;
				}

			}

			if(!doubledotfound || !dotfound || !ptstoitself)
				throwerr("directory not properly formatted.");

		}

		// check 5: For in-use inodes, each block address in use is also marked in use in the bitmap.
		// for the next check need to mark the blocks used in the block entry
		
		// check 7: For in-use inodes, each direct address in use is only used once.
		// check 8: For in-use inodes, each indirect address in use is only used once.

		if(dip[inum].type != 0)
		{
			// go through direct blocks
			for(int b = 0; b < NDIRECT; b++)
			{
				 int blocknum = dip[inum].addrs[b]; // data blocknum
				 if(blocknum == 0) 
				 	continue;
				 dblocks[blocknum].usecount++;
				 dblocks[blocknum].inode = inum;
				 dblocks[blocknum].type = DIRECTADDR;

				 int bmapblocknum = BBLOCK(blocknum, sb->ninodes);
				 int bmapbit = blocknum % BPB;

				 char * bmap = addr + (bmapblocknum * BLOCK_SIZE);

				 int m = 1 << (bmapbit % 8);
				 if((bmap[bmapbit/8] & m) == 0) //bit not set. 
				 {
					throwerr("address used by inode but marked free in bitmap.");		
				 }
				 else
				 {
				 	dblocks[blocknum].bitset = true;
				 }

				if(dblocks[blocknum].usecount > 1)
				 	throwerr("direct address used more than once.");

			}
			
			// go through the indirect block direct blocks
			int indirectblocknum = dip[inum].addrs[NDIRECT];
			
			if(indirectblocknum != 0)
			{
				if(!isBlockUsed(addr, indirectblocknum))
					throwerr("address used by inode but marked free in bitmap.");
				
				dblocks[indirectblocknum].inode = inum;
				dblocks[indirectblocknum].usecount++;
				dblocks[indirectblocknum].bitset = true;
				dblocks[indirectblocknum].type = INDIRECTADDR;
				if(dblocks[indirectblocknum].usecount > 1)
				 	throwerr("indirect address used more than once.");

				uint *indirectblock = (uint*) getBlock(addr, indirectblocknum); //check for zero entries
				for(int index = 0; index < NINDIRECT; index++)
				{
					if(indirectblock[index] == 0) // empty entry
						continue;
					if(!isBlockUsed(addr, indirectblock[index]))
					{
						throwerr("address used by inode but marked free in bitmap.");
					}

					dblocks[indirectblock[index]].inode = inum;
					dblocks[indirectblock[index]].usecount++;
					dblocks[indirectblock[index]].bitset = true;
					dblocks[indirectblock[index]].type = DIRECTADDR;

					if(dblocks[indirectblock[index]].usecount > 1)
					{
				 		throwerr("indirect address used more than once.");
					}

				}
			}
			
		}

	}


	// check 6: For blocks marked in-use in bitmap, the block should actually be in-use in an 
	// inode or indirect block somewhere

	int bitmapblocknum = 3 + (sb->ninodes/IPB); 
	int bmcount = sb->nblocks/BPB+1;

	int datablockstart = bitmapblocknum + bmcount;

	for(int bnum = datablockstart; bnum < sb->size; bnum++)
	{
		if(isBlockUsed(addr, bnum) && dblocks[bnum].usecount == 0)
		{
			throwerr("bitmap marks block in use but it is not in use.");
		}

	}

	// check 9: For all inodes marked in use, each must be referred to in at least one directory
	// check 11: Reference counts (number of links) for regular files match the number of times 
	// file is referred to in directories (i.e., hard links work correctly).
	//
	// check 12: No extra links allowed for directories (each directory only appears in one other 
	// directory).

	for(int inum = 1; inum < sb->ninodes; inum++)
	{
		if(inodes[inum].inuse && inodes[inum].refcount < 1)
		{
			throwerr("inode marked use but not found in a directory.");
		}

		if(dip[inum].type == T_FILE && dip[inum].nlink != inodes[inum].refcount)
		{
			throwerr("bad reference count for file.");
		}

		if(dip[inum].type == T_DIR && inodes[inum].refcount > 1)
		{
			throwerr(" directory appears more than once in file system.");
		}

	}

	exit(0);

}

