// Raymond Liu 261079041
// Gordon Ng 261113030

#include "fsutil2.h"
#include "bitmap.h"
#include "cache.h"
#include "debug.h"
#include "directory.h"
#include "file.h"
#include "filesys.h"
#include "free-map.h"
#include "fsutil.h"
#include "inode.h"
#include "off_t.h"
#include "partition.h"
#include "../interpreter.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUFFER_SIZE 1024 //TO BE CHANGED ?

int copy_in(char *fname) {
  // Open the file
  FILE* sourceFilePointer = fopen(fname, "r");

  //Check the validity of the returned pointer
  if (sourceFilePointer == NULL) {
    return handle_error(FILE_DOES_NOT_EXIST);
  }

  //Trim the filename to retain only the name of the file
  char* targetFileName = strrchr(fname, '/');
  if (targetFileName == NULL) {
    targetFileName = fname;
  } else {
    targetFileName++;
  }

  printf("Target file name: %s\n", targetFileName);

  //Get full size of source file
  int sourceFileSize;

  int currentPos = ftell(sourceFilePointer);
  fseek(sourceFilePointer, 0, SEEK_END);
  sourceFileSize = ftell(sourceFilePointer);
  fseek(sourceFilePointer, currentPos, SEEK_SET);

  printf("Source file size: %d bytes\n", sourceFileSize);

  //Calculate the maximum available free space in the FS
  size_t freeSectors = num_free_sectors();
  size_t freeSpace = freeSectors * BLOCK_SECTOR_SIZE;
  //printf("Free space: %zu bytes\n", freeSpace);

  //Open the file in the FS
  filesys_create(targetFileName, sourceFileSize, false);  
  struct file* targetFilePointer = filesys_open(targetFileName); //Error handling?

  //Start writing, read a single char and write it to the target file
  char buffer[BUFFER_SIZE];
  int bytesWritten = 0;
  
  while ((fread(buffer, 1, sizeof(buffer), sourceFilePointer)) > 0) {
    size_t bufferSize = strlen(buffer) + 1; // +1 to include \0
    if (bufferSize > freeSpace) {
        bufferSize = freeSpace;
    }
    int actualBytesWritten = file_write(targetFilePointer, buffer, bufferSize);
    if (actualBytesWritten < bufferSize) {
        printf("Warning: could only write %d out of %d bytes (reached end of file)\n", bytesWritten + actualBytesWritten, sourceFileSize);
        return -1; //TODO: A REAL ERROR CODE
    }
    bytesWritten += actualBytesWritten;
    freeSpace -= actualBytesWritten;
    if (freeSpace == 0) {
        printf("Warning: could only write %d out of %d bytes (reached end of file)\n", bytesWritten, sourceFileSize);
        break;
    }
}
  
  printf("Bytes written: %d\n", bytesWritten);
  return 0;
}

int copy_out(char *fname) {
  //Open the file from the FS
  struct file* sourceFilePointer = filesys_open(fname);

  //Open the file to write to
  FILE* targetFilePointer = fopen(fname, "w");

  //Check the validity of the returned pointer
  if (sourceFilePointer == NULL) return -1;

  char buffer[BUFFER_SIZE];
  int offset = 0;
  int bytesRead;

  while((bytesRead = file_read_at(sourceFilePointer, buffer,BUFFER_SIZE, offset)) > 0) {
    printf("Read from file: %s", buffer);
    fwrite(buffer, sizeof(char), strlen(buffer), targetFilePointer);
    offset += bytesRead;
  }

  return 0;
}

void find_file(char *pattern) {
  //Iterate over all files in the directory
  //For each file, check if the pattern is contained within it

  struct dir *dir = dir_open_root();
  char name[NAME_MAX + 1]; //MAX FILE NAME LENGTH?

  while(dir_readdir(dir, name)) {
      char buffer[BUFFER_SIZE];
      int offset = 0;
      int bytesRead;

      struct file *file = get_file_by_fname(name);

      if (file == NULL) {
        file = filesys_open(name);
      }

        while((bytesRead = file_read_at(file, buffer,BUFFER_SIZE, offset)) > 0) {
            if (strstr(buffer, pattern) != NULL) {
                printf("%s\n", name);
                break;
            }
            offset += bytesRead;
        }

  }
  dir_close(dir);
}

void fragmentation_degree() {
  //Iterate over all files in the directory
  //For each file, access its inodes and check which sector it is located in
  //Store the previous sector, and check if it is more than 3 away from the next one

  struct dir *dir = dir_open_root();
  char name[NAME_MAX + 1]; //MAX FILE NAME LENGTH?

  int fragmentedFiles = 0;
  int totalFiles = 0;

  while(dir_readdir(dir, name)) {
    struct file* file = get_file_by_fname(name); //Attempt to load from memory

    if (file == NULL) {
      file = filesys_open(name); //Attempt to load from disk
    }

    add_to_file_table(file, name);

    struct inode* inode = file_get_inode(file);

    //Iterate over direct blocks
    struct inode_disk inodeData = inode -> data;
    int previousSectorNumber = inodeData.direct_blocks[0];


    //DIRECT BLOCKS
    for(int i = 1; i < DIRECT_BLOCKS_COUNT; i++) {
      if (inodeData.direct_blocks[i] == 0) { //We've reached an unallocated sector
        goto endWhileLoop; //Immediately force next iteration
      } else {
        if (inodeData.direct_blocks[i] - previousSectorNumber > 3) {
          fragmentedFiles++;
          goto endWhileLoop; //Immediately force next iteration
        }
        previousSectorNumber = inodeData.direct_blocks[i];
      }
    }


    //INDIRECT BLOCKS DEGREE 1 (A BLOCK FULL OF POINTERS)
    if (inodeData.indirect_block != 0) { //If indirect block is allocated
      block_sector_t indirectBlockPointers[INDIRECT_BLOCKS_PER_SECTOR]; //Array of pointers to blocks
      buffer_cache_read(inodeData.indirect_block, &indirectBlockPointers); //Read the indirect block sector into the pointer

      for(int i = 0; i < INDIRECT_BLOCKS_PER_SECTOR; i++) {
        if (indirectBlockPointers[i] == 0) { //We've reached an unallocated sector
          goto endWhileLoop; //Immediately force next iteration
        } else {
          if (indirectBlockPointers[i] - previousSectorNumber > 3) {
            fragmentedFiles++;
            goto endWhileLoop; //Immediately force next iteration
          }
          previousSectorNumber = indirectBlockPointers[i];
        }
      }
    }

    //INDIRECT BLOCKS DEGREE 2 (A BLOCK FULL OF POINTERS TO BLOCKS FULL OF POINTERS)
    if (inodeData.doubly_indirect_block != 0) { //If doubly indirect block is allocated
      block_sector_t doublyIndirectBlockPointers[INDIRECT_BLOCKS_PER_SECTOR]; //Array of pointers to blocks
      buffer_cache_read(inodeData.doubly_indirect_block, &doublyIndirectBlockPointers); //Read the doubly indirect block sector into the pointer

      for(int i = 0 ; i < INDIRECT_BLOCKS_PER_SECTOR; i++) {
        if (doublyIndirectBlockPointers[i] == 0) { //We've reached an unallocated sector
          goto endWhileLoop; //Immediately force next iteration
        } else {
          block_sector_t indirectBlockPointers[INDIRECT_BLOCKS_PER_SECTOR]; //Array of pointers to blocks
          buffer_cache_read(doublyIndirectBlockPointers[i], &indirectBlockPointers); //Read the indirect block sector into the pointer

          for(int j = 0; j < INDIRECT_BLOCKS_PER_SECTOR; j++) {
            if (indirectBlockPointers[j] == 0) { //We've reached an unallocated sector
              goto endWhileLoop; //Immediately force next iteration
            } else {
              if (indirectBlockPointers[j] - previousSectorNumber > 3) {
                fragmentedFiles++;
                goto endWhileLoop; //Immediately force next iteration
              }
              previousSectorNumber = indirectBlockPointers[j];
            }
          }
        }
      }
    }
    endWhileLoop : ;
    totalFiles++;
  }
  printf("Num fragmentable files: %d\n", totalFiles);
  printf("Num fragmented files: %d\n", fragmentedFiles);
  printf("Fragmentation pct: %f\n", (float)fragmentedFiles/totalFiles);
}

// Defragment approach is to copy all files to memory, delete them from disk, and then recreate them
int defragment() {
  struct tempFile {
    char fileName[NAME_MAX + 1];
    char* content;
    off_t length;
  };

  int fileCount = 0;
  char fileName[NAME_MAX + 1];

  //Count out how many files we need to malloc for
  struct dir *dir = dir_open_root();
  while(dir_readdir(dir, fileName)) {
    fileCount++;
  }
  dir_close(dir);

  struct tempFile* files = malloc(fileCount * sizeof(struct tempFile));

  //Copy each file into memory and then delete it from the FS
  int i = 0;
  dir = dir_open_root();
  while(dir_readdir(dir, fileName)) {
    struct file* file = filesys_open(fileName); //Read straight from disk

    if (file != NULL) {
      files[i].length = file_length(file);
      files[i].content = malloc(file_length(file) * sizeof(char));
      file_read(file, files[i].content, file_length(file));
      strcpy(files[i].fileName, fileName);
      i++;
    }
    filesys_remove(fileName);
  }
  dir_close(dir);

  //We should have an empty disk now (NOT FORMATTED HOWEVER)
  //This allows for recovery if we want
  fsutil_freespace();

  for(int j = 0; j < fileCount; j++) {
    printf("%s\n", files[j].fileName);
  }

  //Recreate the files
  for (int j = 0; j < fileCount; j++) {
    filesys_create(files[j].fileName, files[j].length, false);
    struct file* file = filesys_open(files[j].fileName);
    file_write(file, files[j].content, files[j].length);
  }

  return 0;
}  

int recover(int flag) {
  if (flag == 0) { // recover deleted inodes
    for(size_t i = 0; i < bitmap_size(free_map); i++) {
      if (bitmap_test(free_map, i) == 0) {
        struct inode_disk recoveredInode;
        buffer_cache_read(i, &recoveredInode);

        if (recoveredInode.magic == INODE_MAGIC) {
          // filename
          char fileName[NAME_MAX + 1];
          sprintf(fileName, "recovered0-%ld", i);

          struct dir *dir = dir_open_root();
          if (dir == NULL) {
            return -1;
          }
          // add back the file to the directory
          else if (!dir_add(dir, fileName, i, false)){
            return FILE_CREATION_ERROR;
          }
         
          dir_close(dir);
        }
      }
    }
  } else if (flag == 1) { // recover all non-zeroed out data blocks
    for(size_t i = 4; i < bitmap_size(free_map); i++) {

      char buffer[BLOCK_SECTOR_SIZE];
      buffer_cache_read(i, &buffer);

      // there must be a less stupid way to check this
      // Check if the block is all zeros
      bool isZero = true;
      for(int j = 0; j < BLOCK_SECTOR_SIZE; j++) {
        if (buffer[j] != 0) {
          isZero = false;
          break;
        }
      }

      if (!isZero) {
        char fileName[NAME_MAX + 1];
        sprintf(fileName, "recovered1-%ld.txt", i);

        // Write blocks to a file
        FILE *file = fopen(fileName, "w");
        if (file != NULL) {
          for(int j = 0; j < BLOCK_SECTOR_SIZE; j++) {

            // If buffer[j] is not zero, meaning there is data, write to file
            if (buffer[j] != 0) {
              fwrite(&buffer[j], sizeof(char), 1, file);
            }
          }
          fclose(file);
          
        }
      }
    }
  } else if (flag == 2) { // data past end of file.
    struct dir *dir = dir_open_root();
    char fileName[NAME_MAX + 1];

    // Go through all files in the directory
    while (dir_readdir(dir, fileName)) {

      struct file* file = filesys_open(fileName);

      if (file != NULL) {
        off_t fileLength = file_length(file);
        size_t numSectors = bytes_to_sectors(fileLength);
        block_sector_t* dataSectors = get_inode_data_sectors(file->inode);

        // If the file is not a multiple of BLOCK_SECTOR_SIZE and there are sectors allocated
        if (fileLength % BLOCK_SECTOR_SIZE != 0 && numSectors > 0) {
          char buffer[BLOCK_SECTOR_SIZE];
          buffer_cache_read(dataSectors[numSectors - 1], buffer);

          // Check if the last block is zeroed out
          int isZero = 1;
          for (int i = fileLength % BLOCK_SECTOR_SIZE; i < BLOCK_SECTOR_SIZE; i++) {
            if (buffer[i] != 0) {
              isZero = 0;
              break;
            }
          }

          // If the last block is not zeroed out, write the data to a file
          if (!isZero) {
            char recoveredFileName[NAME_MAX + 1];
            sprintf(recoveredFileName, "recovered2-%s.txt", fileName);

            size_t writeSize = BLOCK_SECTOR_SIZE - fileLength % BLOCK_SECTOR_SIZE;
            FILE *recoveredFile = fopen(recoveredFileName, "w");
            if (recoveredFile != NULL) {
              for (size_t i = 0; i < writeSize; i++) {
                // If buffer[i] is not zero, meaning there is data, write to file
                if (buffer[fileLength % BLOCK_SECTOR_SIZE + i] != 0) {
                  fwrite(buffer + fileLength % BLOCK_SECTOR_SIZE + i, sizeof(char), 1, recoveredFile);
                }
              }
              fclose(recoveredFile);
            }
          }
        }
        file_close(file);
      }
    }
    dir_close(dir);
  }
  
  return 0;
}

