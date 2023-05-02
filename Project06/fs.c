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
				inodeNum = (i - 1) * INODES_PER_BLOCK + j + 1;

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

	// Check if file system is not mounted
	if (!isMounted) {

		printf("Error: Filesystem not mounted\n");
		return 0;

	}

	// Initialize block
	union fs_block block;

	// Read data from block
	disk_read(thedisk, 0, block.data);

	// Initialize inode blocks number and variables for iteration
	int inodeblocksNum = block.super.ninodeblocks;
	int i = 0;
	int j = 0;
	int k = 0;
	int inumber;

	// Iterate through all the inodes
	for (i = 1; i <= inodeblocksNum; i++) {

		// Read the block data
		disk_read(thedisk, i, block.data);

		// Iterate through the inodes in a block
		for (j = 0; j < INODES_PER_BLOCK; j++) {

			// Check if the inode is valid
			if (block.inode[j].isvalid == 0) {

				// Update inode to invalid
				block.inode[j].isvalid = 1;

				// Use time function to get the time
				time(&timeTemp);

				// Update ctime and size for the inode
				block.inode[j].ctime = timeTemp;
				block.inode[j].size = 0;

				// Initialize indirect block
				union fs_block indirectBlock;

				// Check if indirect blocks exist
				if (block.inode[j].indirect > 0) {

					// Read indirect block data
					disk_read(thedisk, block.inode[j].indirect, indirectBlock.data);

					// Iterate through pointers in a block
					for (k = 0; k < POINTERS_PER_BLOCK; k++) {

						// Check if there are indirect pointers and set to zero
						if (indirectBlock.pointers[k] > 0) {
							indirectBlock.pointers[k] = 0;
						}
						
					}

					// Write data to the disk
					disk_write(thedisk, block.inode[j].indirect, indirectBlock.data);

				}

				// Iterate through the pointers in an inode
				for (k = 0; k < POINTERS_PER_INODE; k++) {

					// Set direct blocks to 0
					block.inode[j].direct[k] = 0;

				}

				// Set indirect block to 0
				block.inode[j].indirect = 0;
				
				// Write data onto the disk
				disk_write(thedisk, i, block.data);

				// Calculate inumber
				inumber = ((i - 1) * INODES_PER_BLOCK) + j + 1;

				// Return inumber
				return inumber;

			}
		}
	}

	return 0;

}

// Function to delete the inode indicated by the inumber
int fs_delete( int inumber )
{

	// Delete the inode indicated by the inumber. Release all data and indirect blocks
	// assigned to this inode and return them to the free block map. On success, return one. On failure,
	// return 0.

	// Check if filesystem is not mounted
	if (!isMounted) {
		printf("Error: Filesystem not mounted\n");
		return 0;
	}

	// Initialize the block
	union fs_block block;

	// Read the block data
	disk_read(thedisk, 0, block.data);

	// Initialize inodes number, block number and offset, and variables for iteration
	int inodesNum = block.super.ninodes;
	int blockNum = (inumber - 1) / INODES_PER_BLOCK + 1;
	int offset = (inumber - 1) % INODES_PER_BLOCK;
	int i = 0;
	int j = 0;

	// Read the block data
	disk_read(thedisk, blockNum, block.data);

	// Initialize the indirect block
	union fs_block indirectBlock;

	// Check if number is valid
	if (inumber > inodesNum || inumber < 1) {
		printf("Error: Invalid number\n");
		return 0;
	}

	// Check if the inode number exists
	if (block.inode[offset].isvalid == 0) {
		printf("Error: Inode number doesn't exist\n");
		return 0;
	}

	// Set inodes to valid
	block.inode[offset].isvalid = 0;

	// Write data to the disk
	disk_write(thedisk, blockNum, block.data);

	// Iterate through pointers in the inode
	for (i = 0; i < POINTERS_PER_INODE; i++) {
		
		// Check if direct blocks exist to delete
		if (block.inode[offset].direct[i] > 0) {
			freeBitmapBlock[block.inode[offset].direct[i]] = 0;
		}

	}

	// Check if indirect blocks exist
	if (block.inode[offset].indirect > 0) {

		// Read indirect block data
		disk_read(thedisk, block.inode[offset].indirect, indirectBlock.data);

		// Iterate through pointers in the block
		for (j = 0; j < POINTERS_PER_BLOCK; j++) {
			
			// Check if pointers exist to delete
			if (indirectBlock.pointers[i] > 0) {
				freeBitmapBlock[indirectBlock.pointers[i]] = 0;
			}
			
		}

		// Change indirect blocks to 0
		freeBitmapBlock[block.inode[offset].indirect] = 0;

	}

	return 1;

}

// Function to get the logical size of the given inode in bytes
int fs_getsize( int inumber )
{

	// Return the logical size of the given inode, in bytes. Note that zero is a valid logical
	// size for an inode! On failure, return -1.

	// Check if filesystem is mounted
	if (!isMounted) {
		printf("Error: The fs must be already mounted to perform 'fs_getsize'\n");
		return 0;
	}

	// Initialize block
	union fs_block block;

	// Read the block data
	disk_read(thedisk, 0, block.data);

	// Initialize inodes temp variable
	int ninodesTemp = block.super.ninodes;

	// Check if inode number is in bounds
	if (inumber < 1 || inumber > ninodesTemp){
		printf("Error: Inode number is out of bounds\n");
		return -1;
	}

	// Initialize offset and blocknum
	int blockNum = (inumber - 1) / (INODES_PER_BLOCK) + 1;
	disk_read(thedisk, blockNum, block.data);
	int maskOffset = (inumber - 1) % INODES_PER_BLOCK;

	// Check if offset is valis
	if (block.inode[maskOffset].isvalid == 0) {
		printf("Error: Given inode does not seem to exist\n");
		return -1;
	}

	// Return logical size of given inode in bytes
	return block.inode[maskOffset].size;

}

// Function to read data from a valid inode
int fs_read( int inumber, char *data, int length, int offset )
{

	// Read data from a valid inode. Copy "length" bytes from the inode into the "data"
	// pointer, starting at "offset" in the inode. Return the total number of bytes read. The number of
	// bytes actually read could be smaller than the number of bytes requested, perhaps if the end of
	// the inode is reached. If the given inumber is invalid, or any other error is encountered, return 0.

	// Check if file system is mounted
	if (!isMounted) {
		printf("Error: Filesystem not mounted\n");
		return 0;
	}

	// Initialize block, inodeBlock, iOffset, and read block data
	union fs_block block;
	size_t inodeBlock = (inumber / INODES_PER_BLOCK) + 1;
	size_t iOffset = inumber % INODES_PER_BLOCK;
	disk_read(thedisk, inodeBlock, block.data);

    // Check if inode is valid
    if (!block.inode[iOffset].isvalid) {
        printf("Error: Inode is not valid\n");
        return 0;
    }

    // Check if offset is valid
    if (offset > block.inode[iOffset].size) {
        printf("Error: Offset is greater than file size\n");
        return 0;
    }

    // Calculate the starting data block index and offset
    size_t blockIndex = offset / BLOCK_SIZE;
    size_t dataOffset = offset % BLOCK_SIZE;

	// Initialize bytes read
    int bytesRead = 0;

    // Read the data blocks while the conditions are valid
    while (blockIndex < POINTERS_PER_INODE && bytesRead < length && offset < block.inode[iOffset].size) {

		// Initialize block address
        size_t blockAddress = block.inode[iOffset].direct[blockIndex];

        // Initialize a data block and read it
        union fs_block dataBlock;
        disk_read(thedisk, blockAddress, dataBlock.data);

        // Iterate through the block to copy data to the buffer
        for (int i = dataOffset; i < BLOCK_SIZE && bytesRead < length && offset < block.inode[iOffset].size; i++) {

			// Copy the data
            data[bytesRead] = dataBlock.data[i];

			// Increment bytes read and offset
            bytesRead++;
            offset++;

        }

        // Update the data offset to 0
        dataOffset = 0;

        // Move to the next block
        blockIndex++;

    }

    // Iterate through to read data from the indirect blocks, if any
    if (blockIndex < POINTERS_PER_INODE && block.inode[iOffset].indirect) {

		// Initialize block address
        size_t blockAddress = block.inode[iOffset].indirect;

        // Initialize a new indirect block and read it
        union fs_block indirectBlock;
        disk_read(thedisk, blockAddress, indirectBlock.data);

        // While conditions hold, read data from the indirect blocks
        while (blockIndex < POINTERS_PER_INODE + POINTERS_PER_BLOCK && bytesRead < length && offset < block.inode[iOffset].size) {

			// Initialize block address
            size_t blockAddress = indirectBlock.pointers[blockIndex - POINTERS_PER_INODE];

            // Initialize and read a data block
            union fs_block dataBlock;
            disk_read(thedisk, blockAddress, dataBlock.data);

            // Iterate through the block to copy data to the buffer
            for (int i = dataOffset; i < BLOCK_SIZE && bytesRead < length && offset < block.inode[iOffset].size; i++) {

				// Copy the data
                data[bytesRead] = dataBlock.data[i];

				// Increment bytes read and the offset
                bytesRead++;
                offset++;

            }

            // Update the data offset
            dataOffset = 0;

            // Move to the next block
        	blockIndex++;

        }

    }

	// Return the bytes read
    return bytesRead;

}

// Function to write data to a valid inode
int fs_write( int inumber, const char *data, int length, int offset )
{
	
	// Write data to a valid inode. Copy "length" bytes from the pointer "data" into the inode
	// starting at "offset" bytes. Allocate any necessary direct and indirect blocks in the process. Return
	// the number of bytes actually written. The number of bytes actually written could be smaller than
	// the number of bytes request, perhaps if the disk becomes full. If the given inumber is invalid, or
	// any other error is encountered, return 0.

	// Check if file system is mounted
	if (!isMounted) {
        printf("Error: Filesystem not mounted\n");
        return 0;
    }

	return 0;
}