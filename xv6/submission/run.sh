#!/bin/bash

TC=./testcases/

rm -rf fcheck
gcc fcheck.c -o fcheck -Wall -Werror -O -std=gnu99
echo

echo
echo CONDITION 1
echo
echo badinode - file system with a bad type in an inode
./fcheck ${TC}/badinode
echo ------------------------------------------------------------------------------

echo
echo CONDITION 2
echo
echo badaddr - file system with a bad direct address in an inode
./fcheck ${TC}/badaddr
echo ------------------------------------------------------------------------------
echo badindir1 - file system with a bad indirect address in an inode
./fcheck $TC/badindir1
echo ------------------------------------------------------------------------------
echo badindir2 - file system with a bad indirect address in an inode
./fcheck $TC/badindir2
echo ------------------------------------------------------------------------------

echo
echo CONDITION 3
echo
echo badroot - file system with a root directory in bad location
./fcheck $TC/badroot
echo ------------------------------------------------------------------------------
echo badroot2 - file system with a bad root directory in good location
./fcheck $TC/badroot2

echo
echo CONDITION 4
echo
echo badfmt - file system without . or .. directories
./fcheck $TC/badfmt

echo
echo CONDITION 5
echo
echo mrkfree - file system with an inuse direct block marked free
./fcheck $TC/mrkfree
echo ------------------------------------------------------------------------------
echo indirfree - file system with an inuse indirect block marked free
./fcheck $TC/indirfree

echo
echo CONDITION 6
echo
echo mrkused - file system with a free block marked used
./fcheck $TC/mrkused

echo
echo CONDITION 7
echo
echo addronce - file system with a direct address used more than once
./fcheck $TC/addronce

echo
echo CONDITION 8
echo
echo addronce2 - file system with an indirect address used more than once
./fcheck $TC/addronce2

echo
echo CONDITION 9
echo
echo imrkused - file system with inode marked used, but not referenced in a directory
./fcheck $TC/imrkused
echo
echo CONDITION 10
echo
echo imrkfree - file system with inode marked free, but referenced in a directory
./fcheck $TC/imrkfree
echo
echo CONDITION 11
echo
echo badrefcnt - file system which has an inode that is referenced more than its reference count
./fcheck $TC/badrefcnt
echo ------------------------------------------------------------------------------
echo badrefcnt2 - file system which has an inode that is referenced more than its reference count
./fcheck $TC/badrefcnt2

echo
echo CONDITION 12
echo
echo badlarge - large file system with an indirect directory appearing more than once
./fcheck $TC/badlarge
echo ------------------------------------------------------------------------------
echo dironce - file system with a directory appearing more than once
./fcheck $TC/dironce
echo
echo MISCELLANEOUS - No ERRORs
echo
echo mismatch - file system with .. pointing to the wrong directory
./fcheck $TC/mismatch
echo ------------------------------------------------------------------------------
echo good - good file system
./fcheck $TC/good
echo ------------------------------------------------------------------------------
echo goodlarge - large good file system
./fcheck $TC/goodlarge
echo ------------------------------------------------------------------------------
echo goodlink - file system with only good directory link counts
./fcheck $TC/goodlink
echo ------------------------------------------------------------------------------
echo goodrefcnt - file system with only good file reference counts
./fcheck $TC/goodrefcnt
echo ------------------------------------------------------------------------------
echo goodrm - good file system having some files removed
./fcheck $TC/goodrm
echo ------------------------------------------------------------------------------

