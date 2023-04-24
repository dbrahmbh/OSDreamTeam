
#include "fs.h"
#include "disk.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <stdint.h>
#include <math.h>

#define FS_MAGIC           0xf0f03410
#define INODES_PER_BLOCK   128
#define POINTERS_PER_INODE 5
#define POINTERS_PER_BLOCK 1024

struct fs_superblock {
	int magic;
	int nblocks;
	int ninodeblocks;
	int ninodes;
};

struct fs_inode {
	int isvalid;
	int size;
	int direct[POINTERS_PER_INODE];
	int indirect;
};

union fs_block {
	struct fs_superblock super;
	struct fs_inode inode[INODES_PER_BLOCK];
	int pointers[POINTERS_PER_BLOCK];
	char data[DISK_BLOCK_SIZE];
};

int fs_format()
{
	return 0;
}

//  Function to scan a mounted filesystem and report on how the inodes and blocks are organized
void fs_debug()
{

	// Initialize the block
	union fs_block block;

	// Initial disk read
	disk_read(0, block.data);

	// Verify the magic number
	if (block.super.magic != FS_MAGIC) {
		printf("Error: Invalid magic number - filesystem is corrupt!\n");
		return;
	}

	// Initial print
	printf("superblock:\n");
	printf("    %d blocks\n", block.super.nblocks);
	printf("    %d inode blocks\n", block.super.ninodeblocks);
	printf("    %d inodes\n", block.super.ninodes);

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
		disk_read(i, inodeBlock.data);

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
						disk_read(inodeBlock.inode[j].indirect, indirectBlock.data);

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

int fs_mount()
{
	return 0;
}

int fs_create()
{
	return 0;
}

int fs_delete( int inumber )
{
	return 0;
}

int fs_getsize( int inumber )
{
	return -1;
}

int fs_read( int inumber, char *data, int length, int offset )
{
	return 0;
}

int fs_write( int inumber, const char *data, int length, int offset )
{
	return 0;
}