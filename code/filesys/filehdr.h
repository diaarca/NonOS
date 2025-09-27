// filehdr.h
//	Data structures for managing a disk file header.
//
//	A file header describes where on disk to find the data in a file,
//	along with other information about the file (for instance, its
//	length, owner, etc.)
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#ifndef FILEHDR_H
#define FILEHDR_H

#include "bitmap.h"
#include "disk.h"

#define NumDirect ((SectorSize - (3 * sizeof(int))) / sizeof(int))
#define MaxFileSize (NumDirect * SectorSize)

enum fileType { DATA_FILE, DIRECTORY, ROOT };

// The following class defines the Nachos "file header" (in UNIX terms,
// the "i-node"), describing where on disk to find all of the data in the file.
// The file header is organized as a simple table of pointers to
// data blocks.
//
// The file header data structure can be stored in memory or on disk.
// When it is on disk, it is stored in a single sector -- this means
// that we assume the size of this data structure to be the same
// as one disk sector.  Without indirect addressing, this
// limits the maximum file length to just under 4K bytes.
//
// There is no constructor; rather the file header can be initialized
// by allocating blocks for the file (if it is a new file), or by
// reading it from disk.

class FileHeader {
  public:
    bool Allocate(BitMap *bitMap,
                  int fileSize,
                  fileType which,
                  const char *name);                // Initialize a file header,
                                                    //  including allocating space
                                                    //  on disk for the file data
    void Deallocate(BitMap *bitMap);                // De-allocate this file's
                                                    //  data blocks
    void DeallocateUndirectedBlock(BitMap *bitMap); // De-allocate the undirected block

    void FetchFrom(int sectorNumber); // Initialize file header from disk
    void WriteBack(int sectorNumber); // Write modifications to file header
                                      //  back to disk

    int ByteToSector(int offset); // Convert a byte offset into the file
                                  // to the disk sector containing
                                  // the byte

    int FileLength();                      // Return the length of the file
                                           // in bytes
    void SetNumBytes(int newNumBytes);     // Set the numBytes field to a given value
    void SetNumSectors(int newNumSectors); // Set the numSectors field to a given value
    void Print();                          // Print the contents of the file.

    bool IsDataFile();  // The file is a data file
    bool IsDirectory(); // The file is a directory
    bool IsRoot();      // The file is a Root
    int GetNumBytes();  // Return the number of bytes of the file

    bool Extend(BitMap *freeMap, int newSize); // Extend the file by adding 'newSize' (append)
    bool ExtendUndirectedBlock(
        BitMap *freeMap,
        int newSectors); // Extend the size of the undirected block file with 'newSectors' sectors

  private:
    fileType type;              // 0 = File, 1 = Directory, 2 = root Directory
    int numBytes;               // Number of bytes in the file
    int numSectors;             // Number of data sectors in the file
    int dataSectors[NumDirect]; // Disk sector numbers for each data
                                // block in the file
};

#endif // FILEHDR_H
