# File System integrity checker
#### This project deals with verifying the consistency of file systems created by the Unix based xv6 operating system

Running the code in fcheck.c over a file system image (.img file) checks for violations of these 12 conditions:

check 1: Each inode is either unallocated or one of the valid types
check 2:  For in-use inodes, each block address that is used by the inode is valid (points to a valid data block address within the image)
check 3: Root directory exists, its inode number is 1, and the parent of the root directory is itself
check 4: Each directory contains . and .. entries, and the . entry points to the directory itself
check 5: For in-use inodes, each block address in use is also marked in use in the bitmap.for the next check need to mark the blocks used in the block entry
check 6: For blocks marked in-use in bitmap, the block should actually be in-use in an inode or indirect block somewhere
check 7: For in-use inodes, each direct address in use is only used once.
check 8: For in-use inodes, each indirect address in use is only used once.
check 9: For all inodes marked in use, each must be referred to in at least one directory
check 10: For each inode number that is referred to in a valid directory, it is actually marked free
check 11: Reference counts (number of links) for regular files match the number of times file is referred to in directories (i.e., hard links work correctly).
check 12: No extra links allowed for directories (each directory only appears in one other directory).

Any violations will throw an error with corresponding error message.

