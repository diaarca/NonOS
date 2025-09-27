// filesys.cc
//	Routines to manage the overall operation of the file system.
//	Implements routines to map from textual file names to files.
//
//	Each file in the file system has:
//	   A file header, stored in a sector on disk
//		(the size of the file header data structure is arranged
//		to be precisely the size of 1 disk sector)
//	   A number of data blocks
//	   An entry in the file system directory
//
// 	The file system consists of several data structures:
//	   A bitmap of free disk sectors (cf. bitmap.h)
//	   A directory of file names and file headers
//
//      Both the bitmap and the directory are represented as normal
//	files.  Their file headers are located in specific sectors
//	(sector 0 and sector 1), so that the file system can find them
//	on bootup.
//
//	The file system assumes that the bitmap and directory files are
//	kept "open" continuously while Nachos is running.
//
//	For those operations (such as Create, Remove) that modify the
//	directory and/or bitmap, if the operation succeeds, the changes
//	are written immediately back to disk (the two files are kept
//	open during all this time).  If the operation fails, and we have
//	modified part of the directory and/or bitmap, we simply discard
//	the changed version, without writing it back to disk.
//
// 	Our implementation at this point has the following restrictions:
//
//	   there is no synchronization for concurrent accesses
//	   files have a fixed size, set when the file is created
//	   files cannot be bigger than about 3KB in size
//	   there is no hierarchical directory structure, and only a limited
//	     number of files can be added to the system
//	   there is no attempt to make the system robust to failures
//	    (if Nachos exits in the middle of an operation that modifies
//	    the file system, it may corrupt the disk)
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#include "bitmap.h"
#include "directory.h"
#include "disk.h"
#include "filehdr.h"
#include "filesys.h"
#include "openfile.h"
#include <cstring>

// Sectors containing the file headers for the bitmap of free sectors,
// and the directory of files.  These file headers are placed in well-known
// sectors, so that they can be located on boot-up.
#define FreeMapSector 0
#define RootSector 1

// Initial file sizes for the bitmap and directory; until the file system
// supports extensible files, the directory size sets the maximum number
// of files that can be loaded onto the disk.
#define FreeMapFileSize (NumSectors / BitsInByte)
#define NumDirEntries 10
#define DirectoryFileSize (sizeof(DirectoryEntry) * NumDirEntries)

//----------------------------------------------------------------------
// FileSystem::FileSystem
// 	Initialize the file system.  If format = TRUE, the disk has
//	nothing on it, and we need to initialize the disk to contain
//	an empty directory, and a bitmap of free sectors (with almost but
//	not all of the sectors marked as free).
//
//	If format = FALSE, we just have to open the files
//	representing the bitmap and the directory.
//
//	"format" -- should we initialize the disk?
//----------------------------------------------------------------------

FileSystem::FileSystem(bool format) {
    DEBUG('f', "Initializing the file system.\n");

    // Initializing the opened files bit map and static table
    openedFileMap = new BitMap(10);
    openedFileMutex = new Lock("Opened file");
    // Lock for the freeMap and the current directory
    freeMapMutex = new Lock("Free Map");
    directoryMutex = new Lock("Directory");
    // The current directory sector is the root when we start the file system
    DirectorySector = RootSector;

    if (format) {
        BitMap *freeMap = new BitMap(NumSectors);
        Directory *directory = new Directory(NumDirEntries);
        FileHeader *mapHdr = new FileHeader;
        FileHeader *dirHdr = new FileHeader;

        DEBUG('f', "Formatting the file system.\n");

        // First, allocate space for FileHeaders for the directory and bitmap
        // (make sure no one else grabs these!)
        freeMap->Mark(FreeMapSector);
        freeMap->Mark(RootSector);

        // Second, allocate space for the data blocks containing the contents
        // of the directory and bitmap files.  There better be enough space!

        ASSERT(mapHdr->Allocate(freeMap, FreeMapFileSize, DATA_FILE, "glbl_bitmap"));
        ASSERT(dirHdr->Allocate(freeMap, DirectoryFileSize, ROOT, "root_dir"));

        // Flush the bitmap and directory FileHeaders back to disk
        // We need to do this before we can "Open" the file, since open
        // reads the file header off of disk (and currently the disk has garbage
        // on it!).

        DEBUG('f', "Writing headers back to disk.\n");
        mapHdr->WriteBack(FreeMapSector);
        dirHdr->WriteBack(RootSector);

        // OK to open the bitmap and directory files now
        // The file system operations assume these two files are left open
        // while Nachos is running.

        freeMapFile = new OpenFile(FreeMapSector);
        directoryFile = new OpenFile(RootSector);

        // Once we have the files "open", we can write the initial version
        // of each file back to disk.  The directory at this point is completely
        // empty; but the bitmap has been changed to reflect the fact that
        // sectors on the disk have been allocated for the file headers and
        // to hold the file data for the directory and bitmap.

        DEBUG('f', "Writing bitmap and directory back to disk.\n");
        freeMap->WriteBack(freeMapFile); // flush changes to disk
        directory->WriteBack(directoryFile);

        if (DebugIsEnabled('f')) {
            freeMap->Print();
            directory->Print();

            delete freeMap;
            delete directory;
            delete mapHdr;
            delete dirHdr;
        }
    } else {
        // if we are not formatting the disk, just open the files representing
        // the bitmap and directory; these are left open while Nachos is running
        freeMapFile = new OpenFile(FreeMapSector);
        directoryFile = new OpenFile(RootSector);
    }
}

//----------------------------------------------------------------------
// FileSystem::Create
// 	Create a file in the Nachos file system (similar to UNIX create).
//	Since we can't increase the size of files dynamically, we have
//	to give Create the initial size of the file.
//
//	The steps to create a file are:
//	  Make sure the file is not a protected file (the . and .. are protected)
//	  Make sure the file doesn't already exist
//    Allocate a sector for the file header
// 	  Allocate space on disk for the data blocks for the file
//	  Add the name to the directory
//	  Store the new file header on disk
//	  Flush the changes to the bitmap and the directory back to disk
//
//	Return TRUE if everything goes ok, otherwise, return FALSE.
//
// 	Create fails if:
// 	    the filename is a protected name (. and ..)
//   	filename already exist in current directory
//	 	no free space for file header
//	 	no free entry for file in current directory
//	 	no free space for data blocks for the file
//
//	"name" -- name of file to be created
//	"initialSize" -- size of file to be created
//----------------------------------------------------------------------

bool FileSystem::Create(const char *name, int initialSize) {
    Directory *directory;
    BitMap *freeMap;
    FileHeader *hdr;
    int sector;

    DEBUG('f', "Creating file %s, size %d\n", name, initialSize);

    // Check if it is not a protected file (. and .. are protected names)
    if (!strcmp(name, ".") || !strcmp(name, "..")) {
        DEBUG('f', "The filename %s is protected by the file system\n", name);
        return FALSE;
    }

    // Fetch the current directory
    directory = new Directory(NumDirEntries);
    directoryMutex->Acquire();
    directory->FetchFrom(directoryFile);

    // Check if the filename is not already used by another file
    if (directory->Find(name) != -1) {
        DEBUG('f', "The filename %s already exist\n", name);
        delete directory;
        directoryMutex->Release();
        return FALSE;
    }

    // Fetch the freeMap of sectors
    freeMapMutex->Acquire();
    freeMap = new BitMap(NumSectors);
    freeMap->FetchFrom(freeMapFile);
    // Allocate a sector for the file header
    if ((sector = freeMap->Find()) == -1) { // If there is no space for the file header
        DEBUG('f', "No space for file header\n");
        delete directory;
        delete freeMap;
        freeMapMutex->Release();
        directoryMutex->Release();
        return FALSE;
    }

    // Add the file to the directory
    if (!directory->Add(name, sector)) {
        DEBUG('f', "No space in directory\n");
        delete directory;
        delete freeMap;
        freeMapMutex->Release();
        directoryMutex->Release();
        return FALSE;
    }

    hdr = new FileHeader;
    // Allocate the values to the file header
    if (!hdr->Allocate(freeMap, initialSize, DATA_FILE, name)) { // If the allocate fails
        DEBUG('f', "No space on disk for data\n");
        delete directory;
        delete freeMap;
        delete hdr;
        freeMapMutex->Release();
        directoryMutex->Release();
        return FALSE;
    }

    // everything worked, flush all changes back to disk
    hdr->WriteBack(sector);
    directory->WriteBack(directoryFile);
    freeMap->WriteBack(freeMapFile);

    delete directory;
    delete freeMap;
    delete hdr;
    freeMapMutex->Release();
    directoryMutex->Release();

    return TRUE;
}

//----------------------------------------------------------------------
// FileSystem::CreateDir
// 	Create a directory in the Nachos file system (similar to UNIX mkdir).
//
//	The steps to create a directory are:
//	  Make sure the file is not a protected file (the . and .. are protected)
//	  Make sure the file doesn't already exist
//    Allocate a sector for the file header
//	  Allocate space on disk for the data blocks for the file
//	  Add the name to the directory
//	  Store the new file header on disk
//	  Flush the changes to the bitmap and the directory back to disk
//
//	Return TRUE if everything goes ok, otherwise, return FALSE.
//
// 	Create fails if:
// 	    the dirname is a protected name (. and ..)
//   	filename already exist in current directory
//	 	no free space for directory header
//	 	no free entry for directory in current directory
//	 	no free space for data blocks for the directory
//
//	"name" -- name of directory to be created
//----------------------------------------------------------------------

bool FileSystem::CreateDir(const char *name) {
    BitMap *freeMap;
    Directory *newDirectory, *directory;
    FileHeader *dirHdr;
    OpenFile *newDirectoryFile;
    int sector;

    DEBUG('f', "Try to create a directory %s\n", name);

    // Check if it is not a protected file (. and .. are protected names)
    if (!strcmp(name, ".") || !strcmp(name, "..")) {
        DEBUG('f', "The filename %s is protected by the file system\n", name);
        return FALSE;
    }

    directoryMutex->Acquire();
    directory = new Directory(NumDirEntries);
    directory->FetchFrom(directoryFile);
    // Check a file/directory doesn't have a name
    if (directory->Find(name) != -1) {
        DEBUG('f', "The filename %s already exist\n", name);
        delete directory;
        directoryMutex->Release();
        return FALSE;
    }

    freeMapMutex->Acquire();
    freeMap = new BitMap(NumSectors);
    freeMap->FetchFrom(freeMapFile);
    sector = freeMap->Find(); // find a sector to hold the file header
    if (sector == -1) {
        DEBUG('f', "No space for directory header\n");
        delete directory;
        delete freeMap;
        freeMapMutex->Release();
        directoryMutex->Release();
        return FALSE;
    }

    dirHdr = new FileHeader;
    if (!dirHdr->Allocate(freeMap, DirectoryFileSize, DIRECTORY, name)) {
        DEBUG('f', "No space on disk for directory\n");
        delete directory;
        delete freeMap;
        delete dirHdr;
        freeMapMutex->Release();
        directoryMutex->Release();
        return FALSE;
    }

    if (!directory->Add(name, sector)) {
        DEBUG('f', "No space in directory\n");
        delete directory;
        delete freeMap;
        delete dirHdr;
        freeMapMutex->Release();
        directoryMutex->Release();
        return FALSE;
    }

    newDirectory = new Directory(NumDirEntries);

    DEBUG('f', "Writing headers back to disk.\n");
    dirHdr->WriteBack(sector);

    // Update changes of the bitmap and the current directory (parent)
    DEBUG('f', "Writing bitmap and directory back to disk.\n");
    freeMap->WriteBack(freeMapFile); // flush changes to disk
    directory->WriteBack(directoryFile);

    // Create new directory with the . and .. directory
    // Open the new directory file
    newDirectoryFile = new OpenFile(sector);

    // Add the protected directory . and .. in the new directory
    newDirectory->Add(".", sector);
    newDirectory->Add("..", DirectorySector);

    newDirectory->WriteBack(newDirectoryFile);

    delete newDirectoryFile;
    delete newDirectory;
    delete directory;
    delete freeMap;
    delete dirHdr;
    freeMapMutex->Release();
    directoryMutex->Release();

    return TRUE;
}

//----------------------------------------------------------------------
// FileSystem::Open
// 	Open a file for reading and writing.
//	To open a file:
//	  Find the location of the file's header, using the directory
//	  Bring the header into memory
//    Add it to the opened file table
//
//	"name" -- the text name of the file to be opened
//
//	Return:
//	    The OpenFile of the file 'name' of the current directory
//----------------------------------------------------------------------

OpenFile *FileSystem::Open(const char *name) {
    Directory *directory = new Directory(NumDirEntries);
    OpenFile *openFile = NULL;
    int sector;

    DEBUG('f', "Opening kernel file %s\n", name);

    directoryMutex->Acquire();
    directory->FetchFrom(directoryFile);
    sector = directory->Find(name);
    if (sector >= 0)
        openFile = new OpenFile(sector); // name was found in directory

    delete directory;
    directoryMutex->Release();
    return openFile; // return NULL if not found
}

//----------------------------------------------------------------------
// FileSystem::OpenUser
// 	Open a file at user level for reading and writing.
//	To open a file:
//	  Find the location of the file's header, using the directory
//	  Bring the header into memory
//    Add it to the opened file table
//
//	"name" -- the text name of the file to be opened
//
//	Return:
//	    The file descriptor of the file 'name' of the current directory
//----------------------------------------------------------------------

int FileSystem::OpenUser(const char *name) {
    Directory *directory = new Directory(NumDirEntries);
    OpenFile *openFile = NULL;
    FileHeader *fileHdr;
    int sector, index, i;

    DEBUG('f', "Opening user file %s\n", name);

    directoryMutex->Acquire();
    directory->FetchFrom(directoryFile);
    sector = directory->Find(name);

    if (sector < 0) {
        DEBUG('f', "File didn't found\n");
        delete directory;
        directoryMutex->Release();
        return -1;
    }

    if ((openFile = new OpenFile(sector)) == NULL) {
        DEBUG('f', "Open file at kernel level failed\n");
        delete directory;
        directoryMutex->Release();
        return -1;
    }

    delete directory;
    directoryMutex->Release();

    fileHdr = new FileHeader;
    fileHdr->FetchFrom(sector);
    if (!fileHdr->IsDataFile()) {
        DEBUG('f', "User can only open data files\n");
        delete fileHdr;
        return -1;
    }

    // Checking if this same file is already opened in the open fd table
    openedFileMutex->Acquire();
    for (i = 0; i < MaxOpenedFiles; i++) {
        if (openedFileMap->Test(i) && openedFiles[i].id == sector) {
            DEBUG('f', "This file is already opened under an other fd id: %d\n", i);
            delete fileHdr;
            openedFileMutex->Release();
            return -1;
        }
    }

    if ((index = openedFileMap->Find()) == -1) {
        DEBUG('f', "No more slots for opened files\n");
        openedFileMutex->Release();
        return -1;
    }

    openedFiles[index].id = sector;
    openedFiles[index].object = openFile;
    openedFiles[index].mutex = new Lock("OpenFile");

    openedFileMutex->Release();
    delete fileHdr;
    return index;
}

//----------------------------------------------------------------------
// FileSystem::CloseUser
// 	Close a file from reading and writing.
//
// 	"index" -- the index of the opened file to close
//----------------------------------------------------------------------

int FileSystem::CloseUser(int index) {
    openedFileMutex->Acquire();
    if (index < MaxOpenedFiles && openedFileMap->Test(index)) {
        DEBUG('f', "The file of fd id = %d is close\n", index);
        openedFiles[index].mutex->Acquire();
        openedFileMap->Clear(index);
        delete openedFiles[index].object;
        openedFiles[index].mutex->Release();
        delete openedFiles[index].mutex;
        openedFileMutex->Release();
        return 0;
    }

    DEBUG('f', "The file of fd id = %d is not open or it is out of range\n", index);
    openedFileMutex->Release();
    return -1;
}

//----------------------------------------------------------------------
// FileSystem::WriteUser / ReadUser
// 	Write / Read in the given file.
//
// 	"buffer" -- the buffer for the write / read action
// 	"size"   -- the number of byte to write / read
// 	"index"  -- the file id in which write / read
//----------------------------------------------------------------------

int FileSystem::WriteUser(const char *buffer, int size, int index) {
    int value, sizeToExtend;
    BitMap *freeMap;
    OpenFile *file;
    FileHeader *fileHdr;

    DEBUG('f', "\nWRITE USER \n");

    if (index >= MaxOpenedFiles) {
        DEBUG('f', "Opened file %d out of range\n", index);
        return -1;
    }

    if (!openedFileMap->Test(index)) {
        DEBUG('f', "File index %d isn't an opened file\n");
        return -1;
    }

    openedFiles[index].mutex->Acquire();

    freeMapMutex->Acquire();
    freeMap = new BitMap(NumSectors);
    freeMap->FetchFrom(freeMapFile);

    fileHdr = new FileHeader;
    fileHdr->FetchFrom(openedFiles[index].id);

    sizeToExtend = openedFiles[index].object->GetSeek() + size - fileHdr->GetNumBytes();

    if (sizeToExtend > 0 && fileHdr->Extend(freeMap, sizeToExtend) == FALSE) {
        DEBUG('j', "Need to extend file size and not enough space on the disk\n");
        delete fileHdr;
        delete freeMap;
        freeMapMutex->Release();
        openedFiles[index].mutex->Release();
        return -1;
    }

    freeMap->WriteBack(freeMapFile); // flush changes to disk
    fileHdr->WriteBack(openedFiles[index].id);
    delete fileHdr;
    delete freeMap;
    freeMapMutex->Release();

    file = new OpenFile(openedFiles[index].id); // File header changed,
                                                // so we need to reload the open file
    fileHdr = new FileHeader;
    fileHdr->FetchFrom(openedFiles[index].id);

    delete fileHdr;

    // set the seek position of the new file with the old file seek position
    file->Seek(openedFiles[index].object->GetSeek());
    value = file->Write(buffer, size);
    openedFiles[index].object = file;
    openedFiles[index].mutex->Release();
    return value;
}

int FileSystem::ReadUser(char *buffer, int size, int index) {
    int value;

    if (index >= MaxOpenedFiles) {
        DEBUG('f', "Opened file %d out of range\n", index);
        return -1;
    }

    if (!openedFileMap->Test(index)) {
        DEBUG('f', "File index %d isn't an opened file\n");
        return -1;
    }

    openedFiles[index].mutex->Acquire();
    value = openedFiles[index].object->Read(buffer, size);
    openedFiles[index].mutex->Release();
    return value;
}

//----------------------------------------------------------------------
// FileSystem::SeekUser
//  Seek at a position (modulo the file length)
//
// 	"index"   -- the file id in which seek
// 	"nbBytes" -- the number of byte to seek
//----------------------------------------------------------------------

int FileSystem::SeekUser(int index, int nbBytes) {
    if (index >= MaxOpenedFiles) {
        DEBUG('f', "Opened file %d out of range\n", index);
        return -1;
    }

    if (!openedFileMap->Test(index)) {
        DEBUG('f', "File index %d isn't an opened file\n");
        return -1;
    }

    openedFiles[index].mutex->Acquire();
    openedFiles[index].object->Seek(nbBytes % openedFiles[index].object->Length());
    openedFiles[index].mutex->Release();
    return 0;
}

//----------------------------------------------------------------------
// FileSystem::Remove
// 	Delete a file from the file system.  This requires:
//	    Remove it from the directory
//	    Delete the space for its header
//	    Delete the space for its data blocks
//	    Write changes to directory, bitmap back to disk
//
//	Return TRUE if the file was deleted, FALSE if the file wasn't
//	in the file system.
//
//	"name" -- the text name of the file to be removed
//----------------------------------------------------------------------

bool FileSystem::Remove(const char *name) {
    Directory *directory;
    BitMap *freeMap;
    FileHeader *fileHdr;
    int sector, i;

    directoryMutex->Acquire();
    directory = new Directory(NumDirEntries);
    directory->FetchFrom(directoryFile);
    sector = directory->Find(name);
    if (sector == -1) {
        delete directory;
        directoryMutex->Release();
        return FALSE; // file not found
    }

    fileHdr = new FileHeader;
    fileHdr->FetchFrom(sector);

    if (!fileHdr->IsDataFile()) {
        delete directory;
        delete fileHdr;
        directoryMutex->Release();
        return FALSE;
    }

    // Checking if this same file is already opened in the open fd table
    openedFileMutex->Acquire();
    for (i = 0; i < MaxOpenedFiles; i++) {
        if (openedFileMap->Test(i) && openedFiles[i].id == sector) {
            DEBUG('f', "This %s file is actually opened\n", name);
            delete directory;
            delete fileHdr;
            openedFileMutex->Release();
            directoryMutex->Release();
            return FALSE;
        }
    }
    openedFileMutex->Release();

    freeMap = new BitMap(NumSectors);
    freeMapMutex->Acquire();
    freeMap->FetchFrom(freeMapFile);

    fileHdr->Deallocate(freeMap); // remove data blocks
    freeMap->Clear(sector);       // remove header block
    directory->Remove(name);

    freeMap->WriteBack(freeMapFile);     // flush to disk
    directory->WriteBack(directoryFile); // flush to disk

    delete fileHdr;
    delete directory;
    delete freeMap;
    freeMapMutex->Release();
    directoryMutex->Release();
    return TRUE;
}

//----------------------------------------------------------------------
// FileSystem::RemoveDir
// 	Delete a directory from the file system.  This requires:
//	    Remove it from the directory
//	    Delete the space for its header
//	    Delete the space for its data blocks
//	    Write changes to directory, bitmap back to disk
//
//	Return TRUE if the file was deleted, FALSE if the file wasn't
//	in the file system.
//
//	"name" -- the text name of the file to be removed
//----------------------------------------------------------------------

bool FileSystem::RemoveDir(const char *name) {
    Directory *directory, *toDelete;
    OpenFile *toDeleteFile;
    BitMap *freeMap;
    FileHeader *fileHdr;
    int sector;

    directory = new Directory(NumDirEntries);
    directoryMutex->Acquire();
    directory->FetchFrom(directoryFile);
    sector = directory->Find(name);

    if (sector == -1) {
        DEBUG('f', "File %s doesn't exist\n", name);
        delete directory;
        directoryMutex->Release();
        return FALSE;
    }

    fileHdr = new FileHeader;
    fileHdr->FetchFrom(sector);

    if (!fileHdr->IsDirectory() || fileHdr->IsRoot()) {
        DEBUG('f', "The file %s is not a directory\n", name);
        delete directory;
        delete fileHdr;
        directoryMutex->Release();
        return FALSE;
    }

    toDelete = new Directory(NumDirEntries);
    toDeleteFile = new OpenFile(sector);
    toDelete->FetchFrom(toDeleteFile);

    if (!toDelete->IsEmpty()) {
        DEBUG('f', "The directory %s is not empty\n", name);
        delete directory;
        delete fileHdr;
        delete toDelete;
        directoryMutex->Release();
        return FALSE;
    }

    freeMap = new BitMap(NumSectors);
    freeMapMutex->Acquire();
    freeMap->FetchFrom(freeMapFile);

    fileHdr->Deallocate(freeMap); // remove data blocks
    freeMap->Clear(sector);       // remove header block
    directory->Remove(name);

    freeMap->WriteBack(freeMapFile);     // flush to disk
    directory->WriteBack(directoryFile); // flush to disk

    delete directory;
    delete fileHdr;
    delete toDelete;
    delete freeMap;
    freeMapMutex->Release();
    directoryMutex->Release();
    return TRUE;
}

//----------------------------------------------------------------------
// FileSystem::ParsePath
//  Parse the a path name to an array of each subdirectory separated by '/'
//
//	"pathName" -- the path name of the directory we want to move in
//
//	Return:
//	    A structure with the pathName parsed of each directory and the number of directories
//----------------------------------------------------------------------

PathParsed *FileSystem::ParsePath(char *pathName) {
    int i, nbFolders, nbWord;
    char *word;
    char **path;

    // Find the number of words in name
    nbWord = 1;
    for (i = 0; pathName[i] != '\0'; i++) {
        if (pathName[i] == '/') {
            nbWord++;
        }
    }

    path = new char *[nbWord];

    if (i == 0) { // If the path is empty
        return NULL;
    }

    // Cut the path name
    word = strtok(pathName, "/");
    nbFolders = 0;
    while (word != NULL) {
        DEBUG('f', "The word %s has been cut\n", word);
        path[nbFolders] = word;
        word = strtok(NULL, "/");
        nbFolders++;
    }

    PathParsed *pathParsed = new PathParsed;
    pathParsed->path = path;
    pathParsed->nbFolders = nbFolders;
    return pathParsed;
}

//----------------------------------------------------------------------
// FileSystem::ChangeDirRec
//  Change the current directory to the next one.
//
//	"from" -- the directory we are currently inside
//	"paths" -- all the path we want to go inside
//	"len" -- the number of directory in paths
//	"to" -- the index of the folder we want to go inside
//
//	Return:
//	    True if we succefully pass through every directory of paths
//----------------------------------------------------------------------

bool FileSystem::ChangeDirRec(OpenFile *from, char **paths, int len, int to) {
    int sector;

    Directory *directory = new Directory(NumDirEntries);
    directory->FetchFrom(from);

    if (to >= len) {
        return TRUE;
    }

    if ((sector = directory->Find(paths[to])) < 0) { // Check if the directory doesn't exist
        DEBUG('f', "The directory %s doesn't exist\n", paths[to]);
        delete directory;
        return FALSE; // file not found
    } else {

        FileHeader *hdr = new FileHeader;
        hdr->FetchFrom(sector);
        if (!hdr->IsDirectory() && !hdr->IsRoot()) { // Check if it is not a directory
            DEBUG('f', "The file %s is not a directory\n", paths[to]);
            delete directory;
            return FALSE; // file not found
        }

        DirectorySector = sector;
        OpenFile *newDirectoryFile = new OpenFile(DirectorySector);

        delete directory;
        delete hdr;

        directoryFile = newDirectoryFile;
        DirectorySector = sector;

        return ChangeDirRec(newDirectoryFile, paths, len, to + 1);
    }
}

//----------------------------------------------------------------------
// FileSystem::ChangeDir
//  Change the current directory to the wanted one.
//
//	"name" -- the path to the directory we want to change to
//
//	Return:
//	    True if we change to the wanted directory
//----------------------------------------------------------------------

bool FileSystem::ChangeDir(char *name) {
    int tmpSector;
    bool success;
    OpenFile *tmpFile;
    PathParsed *pathParsed;

    if ((pathParsed = ParsePath(name)) == NULL) {
        return FALSE;
    }

    directoryMutex->Acquire();
    // Keep the current directory in case if ChangeDirRec fails
    tmpFile = new OpenFile(DirectorySector);
    tmpSector = DirectorySector;

    // Realize the changes of directories
    success = ChangeDirRec(directoryFile, pathParsed->path, pathParsed->nbFolders, 0);

    if (success == FALSE) {
        DEBUG('f', "cd didn't work\n");
        delete directoryFile;
        directoryFile = tmpFile;
        DirectorySector = tmpSector;
    } else {
        DEBUG('f', "cd work\n");
        delete tmpFile;
    }

    directoryMutex->Release();
    delete pathParsed;
    return success;
}

//----------------------------------------------------------------------
// FileSystem::FileExist
// 	Return if the given file exist in the current directory
//----------------------------------------------------------------------

bool FileSystem::FileExists(const char *name) {
    Directory *directory = new Directory(NumDirEntries);
    int sector;

    directoryMutex->Acquire();
    directory->FetchFrom(directoryFile);
    sector = directory->Find(name);
    directoryMutex->Release();
    delete directory;
    return sector != -1;
}

//----------------------------------------------------------------------
// FileSystem::GetFileSize
//  Return the size of the given file
//----------------------------------------------------------------------

int FileSystem::GetFileSize(const char *name) {
    Directory *directory = new Directory(NumDirEntries);
    int sector, size;

    directoryMutex->Acquire();
    directory->FetchFrom(directoryFile);
    sector = directory->Find(name);

    if (sector == -1){
        directoryMutex->Release();
        return -1;
    }

    FileHeader *hdr = new FileHeader;
    hdr->FetchFrom(sector);
    size = hdr->FileLength();

    delete hdr;
    delete directory;
    directoryMutex->Release();

    return size;
}

//----------------------------------------------------------------------
// FileSystem::IsDataFile
//  Return if the given file is a data file
//----------------------------------------------------------------------

bool FileSystem::IsDataFile(const char *name) {
    Directory *directory = new Directory(NumDirEntries);
    int sector;
    bool isData;

    directoryMutex->Acquire();
    directory->FetchFrom(directoryFile);
    sector = directory->Find(name);

    if (sector == -1){
        directoryMutex->Release();
        return FALSE;
    }

    FileHeader *hdr = new FileHeader;
    hdr->FetchFrom(sector);
    isData = hdr->IsDataFile();

    delete hdr;
    delete directory;
    directoryMutex->Release();

    return isData;
}

//----------------------------------------------------------------------
// FileSystem::List
// 	List all the files in the file system directory.
//----------------------------------------------------------------------

void FileSystem::List() {
    Directory *directory = new Directory(NumDirEntries);

    directoryMutex->Acquire();
    directory->FetchFrom(directoryFile);
    directory->List();
    directoryMutex->Release();

    delete directory;
}

//----------------------------------------------------------------------
// FileSystem::Print
// 	Print everything about the file system:
//	  the contents of the bitmap
//	  the contents of the directory
//	  for each file in the directory,
//	      the contents of the file header
//	      the data in the file
//----------------------------------------------------------------------

void FileSystem::Print() {
    FileHeader *bitHdr = new FileHeader;
    FileHeader *dirHdr = new FileHeader;
    BitMap *freeMap = new BitMap(NumSectors);
    Directory *directory = new Directory(NumDirEntries);

    printf("Bit map file header:\n");
    bitHdr->FetchFrom(FreeMapSector);
    bitHdr->Print();

    printf("Directory file header:\n");
    dirHdr->FetchFrom(DirectorySector);
    dirHdr->Print();

    freeMapMutex->Acquire();
    freeMap->FetchFrom(freeMapFile);
    freeMap->Print();
    freeMapMutex->Release();

    directoryMutex->Acquire();
    directory->FetchFrom(directoryFile);
    directory->Print();
    directoryMutex->Release();

    delete bitHdr;
    delete dirHdr;
    delete freeMap;
    delete directory;
}

//----------------------------------------------------------------------
// FileSystem::PrintDirectory
// 	Print everything about the Directory:
//	  the contents of the directory
//	  for each file in the directory,
//	      the contents of the file header
//	      the data in the file
//----------------------------------------------------------------------

void FileSystem::PrintDirectory() {
    Directory *directory = new Directory(NumDirEntries);
    
    directoryMutex->Acquire();
    directory->FetchFrom(directoryFile);
    printf("\nInformation of the directory of sector %d:\n", DirectorySector);
    printf("Directory files:\n");
    directory->List();
    directory->Print();
    directoryMutex->Release();
    
    delete directory;
}
