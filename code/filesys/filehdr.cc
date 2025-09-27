// filehdr.cc
//	Routines for managing the disk file header (in UNIX, this
//	would be called the i-node).
//
//	The file header is used to locate where on disk the
//	file's data is stored.  We implement this as a fixed size
//	table of pointers -- each entry in the table points to the
//	disk sector containing that portion of the file data
//	(in other words, there are no indirect or doubly indirect
//	blocks). The table size is chosen so that the file header
//	will be just big enough to fit in one disk sector,
//
//      Unlike in a real system, we do not keep track of file permissions,
//	ownership, last modification date, etc., in the file header.
//
//	A file header can be initialized in two ways:
//	   for a new file, by modifying the in-memory data structure
//	     to point to the newly allocated data blocks
//	   for a file already on disk, by reading the file header from disk
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#include "filehdr.h"
#include "openfile.h"
#include "string.h"
#include "system.h"
#include <cstdio>

//----------------------------------------------------------------------
// FileHeader::Allocate
// 	Initialize a fresh file header for a newly created file.
//	Allocate data blocks for the file out of the map of free disk blocks.
//	Return FALSE if there are not enough free blocks to accomodate
//	the new file.
//
//	"freeMap" is the bit map of free disk sectors
//	"fileSize" is the bit map of free disk sectors
//	"which" is the type of the file
//----------------------------------------------------------------------

bool FileHeader::Allocate(BitMap *freeMap, int fileSize, fileType which, const char *name) {
    numBytes = 0;
    type = which;
    numSectors = 0;

    return Extend(freeMap, fileSize);
} 

//----------------------------------------------------------------------
// FileHeader::Deallocate / DeallocateUndirectedBlock
// 	De-allocate all the space allocated for data blocks for this file.
//
//	"freeMap" is the bit map of free disk sectors
//----------------------------------------------------------------------

void FileHeader::Deallocate(BitMap *freeMap) {
    FileHeader *fileHdr;
    OpenFile *file;
    int sector, leftNumSectors;
    printf("Deallocate %d \n", numSectors);
    
    // Deallocate the sectors of the data blocks (that are not in the undirected block)
    for (int i = 0; i < (int)(NumDirect - 1) && i < numSectors; i++) {
        ASSERT(freeMap->Test((int)dataSectors[i])); // ought to be marked!
        freeMap->Clear((int)dataSectors[i]);
    }

    if (numSectors >= (int)(NumDirect - 1)) { // There is an undirected block
        fileHdr = new FileHeader();
        fileHdr->FetchFrom(dataSectors[NumDirect - 1]);
        leftNumSectors = numSectors - (NumDirect - 1);
        file = new OpenFile(dataSectors[NumDirect - 1]);

        // Deallocate the sectors that are in the undirected block
        for (int i = 0; i < leftNumSectors; i++) {
            file->Read((char *)&sector, 4);
            ASSERT(freeMap->Test((int)sector)); // ought to be marked!
            freeMap->Clear((int)sector);
        }

        // Deallocate the undirected block
        ASSERT(freeMap->Test((int)dataSectors[NumDirect - 1])); // ought to be marked!
        freeMap->Clear((int)dataSectors[NumDirect - 1]);

        fileHdr->DeallocateUndirectedBlock(freeMap);
    }
}

void FileHeader::DeallocateUndirectedBlock(BitMap *freeMap) {
    printf("Deallocate undirected block %d \n", numSectors);
    for (int i = 0; i < numSectors; i++) {
        ASSERT(freeMap->Test((int)dataSectors[i])); // ought to be marked!
        freeMap->Clear((int)dataSectors[i]);
    }
}

//----------------------------------------------------------------------
// FileHeader::FetchFrom
// 	Fetch contents of file header from disk.
//
//	"sector" is the disk sector containing the file header
//----------------------------------------------------------------------

void FileHeader::FetchFrom(int sector) { synchDisk->ReadSector(sector, (char *)this); }

//----------------------------------------------------------------------
// FileHeader::WriteBack
// 	Write the modified contents of the file header back to disk.
//
//	"sector" is the disk sector to contain the file header
//----------------------------------------------------------------------

void FileHeader::WriteBack(int sector) { synchDisk->WriteSector(sector, (char *)this); }

//----------------------------------------------------------------------
// FileHeader::ByteToSector
// 	Return which disk sector is storing a particular byte within the file.
//      This is essentially a translation from a virtual address (the
//	offset in the file) to a physical address (the sector where the
//	data at the offset is stored).
//
//	"offset" is the location within the file of the byte in question
//----------------------------------------------------------------------

int FileHeader::ByteToSector(int offset) {
    unsigned int entry, sector, newOffset, size;
    OpenFile *file;

    entry = offset / SectorSize;
    DEBUG('f', "Offset, we want to access to %d\n", offset);

    if (entry < NumDirect - 1) {
        return dataSectors[entry];
    }

    // Undirected block part
    file = new OpenFile(dataSectors[NumDirect - 1]);
    newOffset = offset - (SectorSize * (NumDirect - 1));
    DEBUG('f', "Offset, we want to access to %d in the undirected data block of size %d\n",
          newOffset, file->Length());

    // read the sectors in the undirected blocks
    size = file->ReadAt((char *)&sector, 4, 4 * (newOffset / SectorSize));
    DEBUG('f', "The size read by the ByteToSector is %d\n", size);
    ASSERT(size == 4);

    DEBUG('f', "The sector read by the ByteToSector is %d\n", sector);

    delete file;
    return sector;
}

//----------------------------------------------------------------------
// FileHeader::FileLength
// 	Return the number of bytes in the file.
//----------------------------------------------------------------------

int FileHeader::FileLength() { return numBytes; }

//----------------------------------------------------------------------
// FileHeader::IsDataFile
// 	Return if the file is a data file
//----------------------------------------------------------------------

bool FileHeader::IsDataFile() { return type == DATA_FILE; }

//----------------------------------------------------------------------
// FileHeader::IsDirectory
// 	Return if the file is a direcotry (root or not)
//----------------------------------------------------------------------

bool FileHeader::IsDirectory() { return type == DIRECTORY || type == ROOT; }

//----------------------------------------------------------------------
// FileHeader::IsRoot
// 	Return if the file is a data file
//----------------------------------------------------------------------

bool FileHeader::IsRoot() { return type == ROOT; }

//----------------------------------------------------------------------
// FileHeader::GetNumBytes
// 	Return the number of bytes in the file
//----------------------------------------------------------------------

int FileHeader::GetNumBytes() { return numBytes; }

//----------------------------------------------------------------------
// FileHeader::Extend / ExtendUndirectedBlock
// 	Increase the maximal size of the file (the number of sectors).
//
//	"freeMap" is the bit map of free disk sectors
//	"newSize" is the size (number of bytes) we want to add to the file
//----------------------------------------------------------------------

bool FileHeader::Extend(BitMap *freeMap, int newSize) {
    DEBUG('f', "\n\nWE WANT TO EXTEND THE FILE\n");
    int i, j, sector, numAllocatedSectors, newNumTotalSectors, newNumSectors, undirectedIndex,
        newUndirectedSectors;
    FileHeader *fileHdr;
    OpenFile *file;

    undirectedIndex = NumDirect - 1;
    newNumTotalSectors = divRoundUp(numBytes + newSize, SectorSize);
    newNumSectors = newNumTotalSectors - numSectors;
    numAllocatedSectors = 0;

    // Allocate the sectors for the file (that are not in the undirected blocks)
    for (i = numSectors; i < newNumTotalSectors && i < undirectedIndex; i++) {
        if ((dataSectors[i] = freeMap->Find()) == -1) {
            return FALSE;
        }
        numAllocatedSectors++;
    }

    if (i < newNumTotalSectors) { // Check if we need an undirected block
        fileHdr = new FileHeader;
        if (numSectors > undirectedIndex) { // If the undirected block already exists
            DEBUG('f', "OLD Undirected block\n");
            fileHdr->FetchFrom(dataSectors[undirectedIndex]);
            newUndirectedSectors = newNumSectors;
        } else { // If we need to allocate the header for the undirected block as it doesn't already
                 // exists
            DEBUG('f', "NEW Undirected block\n");
            if ((dataSectors[undirectedIndex] = freeMap->Find()) == -1) {
                delete fileHdr;
                return FALSE;
            }
            fileHdr->Allocate(freeMap, 0, DATA_FILE, "");
            newUndirectedSectors = newNumSectors - numAllocatedSectors;
        }

        // Do the extension of the undirected block
        if (!fileHdr->ExtendUndirectedBlock(freeMap, newUndirectedSectors)) {
            delete fileHdr;
            return FALSE;
        }

        fileHdr->WriteBack(dataSectors[undirectedIndex]);

        file = new OpenFile(dataSectors[undirectedIndex]);

        // Seek to the position where we want to write the sectors in the undirected block
        file->Seek(file->Length() - (newUndirectedSectors * 4));

        // Write the sectors of the file in the undirected block
        for (j = 0; j < newUndirectedSectors; j++) {
            if ((sector = freeMap->Find()) == -1) {
                delete fileHdr;
                delete file;
                return FALSE;
            }
            file->Write((char *)&sector, 4);
        }

        delete fileHdr;
        delete file;
    }

    numBytes = numBytes + newSize;
    numSectors = newNumTotalSectors;
    DEBUG('f', "WE FINISHED THE EXTEND OF THE FILE\n");
    return TRUE;
}

bool FileHeader::ExtendUndirectedBlock(BitMap *freeMap, int newSectors) {
    DEBUG('f', "WE WANT TO EXTEND THE UNDIRECTED BLOCK\n");
    int i, newNumTotalSectors;

    newNumTotalSectors = divRoundUp(numBytes + (newSectors * 4), SectorSize);
    DEBUG('f', "The size of the file is %d; %d;\n", numBytes, newSectors);

    if (newNumTotalSectors > (int)NumDirect) {
        DEBUG('f', "Don't have necessary place in order to extend this file\n");
        return FALSE;
    }

    for (i = numSectors; i < newNumTotalSectors; i++) {
        if ((dataSectors[i] = freeMap->Find()) == -1) {
            return FALSE;
        }
    }

    numSectors = newNumTotalSectors;
    numBytes = numBytes + (newSectors * 4);

    DEBUG('f', "WE FINISHED THE EXTEND OF THE UNDIRECTED BLOCK\n");
    return TRUE;
}

//----------------------------------------------------------------------
// FileHeader::Print
// 	Print the contents of the file header, and the contents of all
//	the data blocks pointed to by the file header.
//----------------------------------------------------------------------

void FileHeader::Print() {
    int i, j, k, sector, sectorPosition;
    char *data = new char[SectorSize];
    OpenFile *file;

    printf("FileHeader contents.  File size: %d.  Number of sectors: %d. File blocks:\n", numBytes,
           numSectors);
    for (i = 0; i < numSectors; i++) {
        if (i < (int)(NumDirect - 1)) {
            sector = dataSectors[i];
        } else {
            sectorPosition = i - (NumDirect - 1);
            file = new OpenFile(dataSectors[NumDirect - 1]);
            file->ReadAt((char *)&sector, 4, 4 * sectorPosition);
            delete file;
        }

        printf("%d ", sector);
    }

    printf("\nFile contents:\n");
    for (i = k = 0; i < numSectors; i++) {
        if (i < (int)(NumDirect - 1)) {
            sector = dataSectors[i];
        } else {
            sectorPosition = i - (NumDirect - 1);
            file = new OpenFile(dataSectors[NumDirect - 1]);
            file->ReadAt((char *)&sector, 4, 4 * sectorPosition);
            delete file;
        }
        synchDisk->ReadSector(sector, data);
        for (j = 0; (j < SectorSize) && (k < numBytes); j++, k++) {
            printf("%c", data[j]);
        }
        printf("\n");
    }
    delete[] data;
}
