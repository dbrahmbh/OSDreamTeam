/* main.c : Main file for redextract */

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

/* Include to account for strdup due to C99 flag */
char *strdup(const char *s);

#include <string.h>

#include "packet.h"
#include "pcap-process.h"
#include "pcap-read.h"

// Max number of files that can be read, and max length each file can be
#define MAX_SIZE 100
#define MAX_LENGTH 100

// Initializing locs and condition variables
pthread_mutex_t LockStack;
pthread_mutex_t LockTable;
pthread_cond_t PushCond;
pthread_cond_t PopCond;

// Initialize the number of values in the stack
int StackNum = 0;

// Initialize flags to be used to keep track of conditions
char FinishedFlag = 0;
char Continue = 1;
char Finished = 0;

// Initialize packet
struct Packet *StackObjects[MAX_SIZE];

// Function for the producer thread
void *thread_producer(void *PacketData) {
  
  // Set up memory for FileInfo pointer
  struct FilePcapInfo *FileInfo = (struct FilePcapInfo *)PacketData;
  
  // Read the file and push the packets
  readPcapFile(FileInfo);
  return NULL;
  
}

// Function for the consumer thread
void *thread_consumer(void *PacketData) {
  
  struct Packet *currPacket;

  // While loop as long as continue flag is true
  while (Continue) {

    // Lock the mutex for the stack
    pthread_mutex_lock(&LockStack);

    // If stack is empty, then wait
    while (StackNum <= 0) {

      // Wait
      pthread_cond_wait(&PopCond, &LockStack);
      
      // Check if finished
      if (StackNum == 0 && FinishedFlag) {
        
        Finished = 0;
        pthread_mutex_unlock(&LockStack);
        return NULL;
        
      }
    }
    
    // Pop the packet
    currPacket = StackObjects[StackNum - 1];
    StackNum--;

    // Communicate to producers that there is room to push
    pthread_cond_signal(&PushCond);
    pthread_mutex_unlock(&LockStack);
    
    // Lock the table's lock and process the packet
    pthread_mutex_lock(&LockTable);
    processPacket(currPacket);

    // Unlock the table's lock
    pthread_mutex_unlock(&LockTable);
    
  }
  
  return NULL;
  
}

// Function to process the pcap file
void PcapFileProcess(char *fileName, int numThreads) {
  
  struct FilePcapInfo fileInfo;
  fileInfo.FileName = fileName;
  fileInfo.EndianFlip = 0;
  fileInfo.BytesRead = 0;
  fileInfo.Packets = 0;
  fileInfo.MaxPackets = 0;

  // Initialize consumer threads
  int numConsumerThreads = numThreads - 1;
  pthread_t *pThreadConsumers;
  pThreadConsumers =
      (pthread_t *)malloc(sizeof(pthread_t *) * numConsumerThreads);

  // Initialize producer threads
  pthread_t pThreadProducer;

  // Start the producer thread
  pthread_create(&pThreadProducer, 0, thread_producer, &fileInfo);

  // Create consumer threads
  for (int i = 0; i < numConsumerThreads; i++) {
    pthread_create(&pThreadConsumers[i], 0, thread_consumer, 0);
  }

  // Use join function to allow producer thread to finish
  pthread_join(pThreadProducer, 0);
  
  // Update flag values to let consumers know that producers are done now
  FinishedFlag = 1;
  Finished = 1;

  // Broadcast signal to pop while the finished flag is true
  while (Finished) {
    pthread_cond_broadcast(&PopCond);
  }

  // Iterate through consumer threads to join them
  for (int i = 0; i < numConsumerThreads; i++) {
    pthread_join(pThreadConsumers[i], 0);
  }
  
}

int main(int argc, char *argv[]) {
  
  if (argc < 2) {
    printf("Usage: redextract FileX\n");
    printf("       redextract FileX\n");
    printf("  FileList        List of pcap files to process\n");
    printf("    or\n");
    printf("  FileName        Single file to process (if ending with .pcap)\n");
    printf("\n");
    printf("Optional Arguments:\n");
    /* You should handle this argument but make this a lower priority when
       writing this code to handle this
     */
    printf("  -threads N       Number of threads to use (2 to 8)\n");
    /* Note that you do not need to handle this argument in your code */
    printf("  -window  W       Window of bytes for partial matching (64 to "
           "512)\n");
    printf("       If not specified, the optimal setting will be used\n");
    return -1;
  }

  /* Note that the code as provided below does only the following
   *
   * - Reads in a single file twice
   * - Reads in the file one packet at a time
   * - Process the packets for redundancy extraction one packet at a time
   * - Displays the end results
   */

  char *inputFile = argv[1];

  // Initialize default number of threads
  int numThreads = 2;

  // parse arguments
  for (int i = 2; i < argc; i++) {

    // Check -threads flag
    if (strcmp(argv[i], "-threads") == 0) {

      if (i + 1 >= argc) {
        printf("Error: value not specified after -threads\n");
        return 0;
      }
      else {

        // Store the number of threads value
        numThreads = atoi(argv[i + 1]);

        // Check if number of threads is within bounds
        if (numThreads < 2 || numThreads > 8) {
          printf("Error: number of threads must be a number from 2-8\n");
          return 0;
        }
        
      }
      
    }
    
  }

  // Initialize locks
  pthread_mutex_init(&LockStack, 0);
  pthread_mutex_init(&LockTable, 0);

  printf("MAIN: Initializing the table for redundancy extraction\n");
  initializeProcessing(DEFAULT_TABLE_SIZE);
  printf("MAIN: Initializing the table for redundancy extraction ... done\n");

  // If the input file is a .pcap file, process it
  if (strstr(inputFile, ".pcap")) {
    PcapFileProcess(inputFile, numThreads);

  }
  // Else loop to process .txt files
  else {
    
    FILE *fp;
    char str[MAX_LENGTH];
    fp = fopen(inputFile, "r");
    
    if (fp == NULL) {
      printf("error: unable to open file\n");
      return 0;
    }
    
    // Read pcap files in the .txt files
    while (fgets(str, MAX_LENGTH, fp)) {
      // Remove \n character
      str[strcspn(str, "\n")] = 0;
      // Do producer/consumer code by calling pcap file process function
      char *file = strdup(str);
      PcapFileProcess(file, numThreads);
    }
    
    fclose(fp);
    
  }

  printf("Summarizing the processed entries\n");
  tallyProcessing();
  
  /* Output the statistics */

  printf("Parsing of file %s complete\n", argv[1]);

  printf("  Total Packets Parsed:    %d\n", gPacketSeenCount);
  printf("  Total Bytes   Parsed:    %lu\n", (unsigned long)gPacketSeenBytes);
  printf("  Total Packets Duplicate: %d\n", gPacketHitCount);
  printf("  Total Bytes   Duplicate: %lu\n", (unsigned long)gPacketHitBytes);

  float fPct;

  fPct = (float)gPacketHitBytes / (float)gPacketSeenBytes * 100.0;

  printf("  Total Duplicate Percent: %6.2f%%\n", fPct);

  return 0;
  
}
