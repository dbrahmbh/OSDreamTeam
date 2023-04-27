/*
Implementation of SimpleFS.
Make your changes here.
*/

#include "fs.h"
#include "disk.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <unistd.h>
#include <math.h>
#include <time.h>

// Global variables for disk, to check if the system is mounted, keep track of bitmap, and time
extern struct disk *thedisk;
bool isMounted = false;
int * freeBitmapBlock;
time_t timeTemp;

// Global variables
union fs_block currentFreeBlock; 

// Function to create a new filesystem on the disk and destroy any data already present
int fs_format()
{
	// Creates a new filesystem on the disk, destroying any data already present. Sets aside
	// ten percent of the blocks for inodes, clears the inode table, and writes the superblock. Returns
	// one on success, zero otherwise. Note that formatting a filesystem does not cause it to be
	// mounted. Also, an attempt to format an already-mounted disk should do nothing and return failure.

	// Check if the filesystem is mounted to print error and exit
	if (isMounted) {

		printf("Error: The disk can't be formatted because it is already mounted\n");
		return -1;

	}

	// Initialize new updated values
	int nblocksUpdated = disk_nblocks(thedisk);
	// Update ninodeblocks using ceil (should always be 10 percent of nblocks after rounding up)
	int ninodeblocksUpdated = ceil(0.10 * (double) nblocksUpdated);
	int ninodesUpdated = ninodeblocksUpdated * INODES_PER_BLOCK;

	// Initialize the block
	union fs_block block;

	// Initialize the superblock values
	block.super.magic = FS_MAGIC;
	block.super.nblocks = nblocksUpdated;
	block.super.ninodeblocks = ninodeblocksUpdated;
	block.super.ninodes = ninodesUpdated;

	// Write to the disk
	disk_write(thedisk, 0, block.data);

	// Iterate through inodes to set them as invalid
	int i;
	for (i = 0; i < INODES_PER_BLOCK; ++i) {
		block.inode[i].isvalid = 0;
	}

	// Iterate through to write to disk
	for (i = 1; i < ninodeblocksUpdated; ++i) {
		disk_write(thedisk, i, block.data);
	}

	return 1;

}

//  Function to scan a mounted filesystem and report on how the inodes and blocks are organized
void fs_debug()
{
	// Initialize the block
	union fs_block block;

	// Initial disk read
	disk_read(thedisk, 0, block.data);

	// Verify the magic number
	if (block.super.magic != FS_MAGIC) {
		printf("Error: Invalid magic number - filesystem is corrupt!\n");
		return;
	}

	// Initial print
	printf("superblock:\n");
	printf("    0x%08X -> magic number\n", block.super.magic);
	printf("    %d blocks\n",block.super.nblocks);
	printf("    %d inode blocks\n",block.super.ninodeblocks);
	printf("    %d inodes\n",block.super.ninodes);

	// Initialize vaiables
	int inodeblocksNum = block.super.ninodeblocks;
	int i = 0;
	int j = 0;
	int k = 0;
	int inodeNum;
	int dataBlockNum;
	
	// Initialize a new inode block and indirect block
	union fs_block inodeBlock;
	union fs_block indirectBlock;

	// For loop to iterate through all the inodeblocks
	for (i = 1; i < inodeblocksNum + 1; i++) {

		// Use disk_read function to read the data from the blocks
		disk_read(thedisk, i, inodeBlock.data);

		// For loop to go through all the inodes in an individual block
		for(j = 0; j < INODES_PER_BLOCK; j++) {

			// Check if the inode is valid
			if (inodeBlock.inode[j].isvalid == 1) {

				// Set up the inode number
				inodeNum = (i - 1) * INODES_PER_BLOCK + j;

				// Print the individual inode's data
				printf("inode %d:\n", inodeNum);
				printf("    size: %d bytes\n", inodeBlock.inode[j].size);
				printf("    direct blocks:");

				// For loop to parse through all existing pointers in inode 'j'
				for (k = 0; k < POINTERS_PER_INODE; k++) {
					
					// Check if the blocks are direct blocks
					if (inodeBlock.inode[j].direct[k]) {
						
						// Check if the direct block is out of range before printing
						if (inodeBlock.inode[j].direct[k] < inodeblocksNum + 1 || inodeBlock.inode[j].direct[k] >= block.super.nblocks) {
							
							printf("Error: Direct block out of range\n");
							return;

						}
						else {
							printf(" %d ", inodeBlock.inode[j].direct[k]);
						}

					}

				}
				
				// Print out a new line for readability
				printf("\n");

				// Check if indirect blocks exist
				if (inodeBlock.inode[j].indirect > 0) {

					// Check if the indirect block is out of range
					if (inodeBlock.inode[j].indirect < inodeblocksNum + 1 || inodeBlock.inode[j].indirect >= block.super.nblocks) {
						
						printf("Error: Indirect block out of range\n");
						return;
					
					}
					else {

						// Read in indirect block data
						disk_read(thedisk, inodeBlock.inode[j].indirect, indirectBlock.data);

						// Print out info for indirect block
						printf("    indirect block: %d \n", inodeBlock.inode[j].indirect);

						// Print out info for inderect data blocks
						printf("    indirect data blocks:");

						// For loop to iterate through all the pointers in the individual block
						for (k = 0; k < POINTERS_PER_BLOCK; k++) {
							
							// Check if pointers exist to print info
							if (indirectBlock.pointers[k]) {

								// Calculate the data block number
								dataBlockNum = indirectBlock.pointers[k];

								// Check if the data block is out of range before printing
								if (dataBlockNum < inodeblocksNum + 1 || dataBlockNum >= block.super.nblocks) {
									printf("Error: Data block is out of range\n");
									return;
								}
								else {
									printf(" %d ", indirectBlock.pointers[k]);
								}				
							}
						}

						// Print out a new line for readability
						printf("\n");

					}
				}
			}
		}
	}
}

// Function to examine the disk for a filesystem and prepare filesystem for use
int fs_mount()
{

	// Examine the disk for a filesystem. If one is present, read the superblock, build a free
	// block bitmap, and prepare the filesystem for use. Return one on success, zero otherwise. Note
	// that a successful mount is a pre-requisite for the remaining calls.

	// Check if the filesystem has already been mounted, if not continue
	if (isMounted) {

		printf("Error: The filesystem can't already be mounted when trying to mount\n");
		return 0;

	}

	// Initialize a block
	union fs_block block;

	// Read the disk
	disk_read(thedisk, 0, block.data);

	// Check if magic number is valid
	if (block.super.magic != FS_MAGIC) {

		printf("Error: The magic number did not match when trying to mount\n");
		return 0;

	}

	// Initialize temporary superblock nblocks and ninodeblocks
	int nblocksTemp = block.super.nblocks;
	int ninodeblocksTemp = block.super.ninodeblocks;

	// Initialize the current free bitmap block using malloc
	freeBitmapBlock = malloc(nblocksTemp * sizeof(int));
	freeBitmapBlock[0] = 1;

	// Initialize the indirect block and variables for iteration
	union fs_block indirectBlock;
	int i, j, k;

	// Iterate through all the inode blocks
	for (i = 1; i <= ninodeblocksTemp; ++i) {

		// Initialize bitmap value and read the data
		freeBitmapBlock[i] = 1;
		disk_read(thedisk, i, block.data);

		// Iterate through all the inodes in a block
		for (j = 0; j < INODES_PER_BLOCK; ++j) {

			// Check if the block is valid
			if (block.inode[j].isvalid == 1) {
				
				// Update the bitmap's direct blocks used
				for (k = 0; k < POINTERS_PER_INODE; ++k) {

					// If direct blocks exist, update bitmap
					if (block.inode[j].direct[k] > 0) {
						freeBitmapBlock[block.inode[j].direct[k]] = 1;
					}
				}

				// Check for indirect blocks to update bitmap
				if (block.inode[j].indirect > 0) {

					// Read the data for indirect blocks
					disk_read(thedisk, block.inode[j].indirect, indirectBlock.data);

					freeBitmapBlock[block.inode[j].indirect] = 1;

					// Iterate through all the pointers per block
					for (k = 0; k < POINTERS_PER_BLOCK; k++) {

						// If indirect blocks exist, update bitmap
						if (indirectBlock.pointers[k] > 0) {
							freeBitmapBlock[indirectBlock.pointers[k]] = 1;
						}

					}

				}
			}
		}
	}

	// Change mounted boolean variable to true if mount was successful
	isMounted = true;

	return 1;

}

// Function to create a new inode of zero length
int fs_create()
{

	// Create a new inode of zero length. On success, return the (positive) inumber. On
	// failure, return zero. (Note that this implies zero cannot be a valid inumber.)

	return 0;

}

// Function to delete the inode indicated by the inumber
int fs_delete( int inumber )
{

	// Delete the inode indicated by the inumber. Release all data and indirect blocks
	// assigned to this inode and return them to the free block map. On success, return one. On failure,
	// return 0.

	return 0;

}

// Function to get the logical size of the given inode in bytes
int fs_getsize( int inumber )
{

	// Return the logical size of the given inode, in bytes. Note that zero is a valid logical
	// size for an inode! On failure, return -1.

	return 0;

}

// Function to read data from a valid inode
int fs_read( int inumber, char *data, int length, int offset )
{

	// Read data from a valid inode. Copy "length" bytes from the inode into the "data"
	// pointer, starting at "offset" in the inode. Return the total number of bytes read. The number of
	// bytes actually read could be smaller than the number of bytes requested, perhaps if the end of
	// the inode is reached. If the given inumber is invalid, or any other error is encountered, return 0.

	return 0;

}

// Function to write data to a valid inode
int fs_write( int inumber, const char *data, int length, int offset )
{

	// Write data to a valid inode. Copy "length" bytes from the pointer "data" into the inode
	// starting at "offset" bytes. Allocate any necessary direct and indirect blocks in the process. Return
	// the number of bytes actually written. The number of bytes actually written could be smaller than
	// the number of bytes request, perhaps if the disk becomes full. If the given inumber is invalid, or
	// any other error is encountered, return 0.

	return 0;

}