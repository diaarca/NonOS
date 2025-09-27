// filesys.h
//	Data structures to represent the Nachos file system.
//
//	A file system is a set of files stored on disk, organized
//	into directories.  Operations on the file system have to
//	do with "naming" -- creating, opening, and deleting files,
//	given a textual file name.  Operations on an individual
//	"open" file (read, write, close) are to be found in the OpenFile
//	class (openfile.h).
//
//	We define two separate implementations of the file system.
//	The "STUB" version just re-defines the Nachos file system
//	operations as operations on the native UNIX file system on the machine
//	running the Nachos simulation.  This is provided in case the
//	multiprogramming and virtual memory assignments (which make use
//	of the file system) are done before the file system assignment.
//
//	The other version is a "real" file system, built on top of
//	a disk simulator.  The disk is simulated using the native UNIX
//	file system (in a file named "DISK").
//
//	In the "real" implementation, there are two key data structures used
//	in the file system.  There is a single "root" directory, listing
//	all of the files in the file system; unlike UNIX, the baseline
//	system does not provide a hierarchical directory structure.
//	In addition, there is a bitmap for allocating
//	disk sectors.  Both the root directory and the bitmap are themselves
//	stored as files in the Nachos file system -- this causes an interesting
//	bootstrap problem when the simulated disk is initialized.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#ifndef FS_H
#define FS_H

#include "bitmap.h"
#include "copyright.h"
#include "openfile.h"
#include "synch.h"

#define MaxOpenedFiles 10

#ifdef FILESYS_STUB // Temporarily implement file system calls as
                    // calls to UNIX, until the real file system
                    // implementation is available
class FileSystem {
  public:
    FileSystem(bool format) {}

    bool Create(const char *name, int initialSize) {
        int fileDescriptor = OpenForWrite(name);

        if (fileDescriptor == -1)
            return FALSE;
        Close(fileDescriptor);
        return TRUE;
    }

    OpenFile *Open(char *name) {
        int fileDescriptor = OpenForReadWrite(name, FALSE);

        if (fileDescriptor == -1)
            return NULL;
        return new OpenFile(fileDescriptor);
    }

    bool Remove(char *name) { return Unlink(name) == 0; }
};

#else // FILESYS

typedef struct {
    OpenFile *object;
    int id;
    Lock *mutex;
} UserFile;

typedef struct {
    char **path;
    int nbFolders;
} PathParsed;

class FileSystem {
  public:
    FileSystem(bool format); // Initialize the file system. Must be called
                             // *after* "synchDisk" has been initialized. If "format", there is
                             // nothing on the disk, so initialize the directory and the bitmap of
                             // free blocks.
    bool Create(const char *name, int initialSize); // Create a file (UNIX create)
    bool CreateDir(const char *name);               // Create a directory (UNIX mkdir)

    OpenFile *Open(const char *name); // Open a file (kernel level)
    int OpenUser(const char *name);   // Open a file (UNIX open at user level)
    int CloseUser(int index);         // Close a file (UNIX close)

    int WriteUser(const char *buffer, int size, int index); // Write in a file (UNIX write)
    int ReadUser(char *buffer, int size, int index);        // Read in a file (UNIX read)
                                                     // return the read size
    int SeekUser(int index, int nbBytes); // Seek at a position in a file (modulo the file size)

    bool Remove(const char *name);    // Delete a file (UNIX unlink)
    bool RemoveDir(const char *name); // Delete a directory (UNIX unlink)

    PathParsed *ParsePath(char *pathName); // Parse a path name
    bool ChangeDir(char *name);                        // Change the current directory (UNIX cd)
    bool ChangeDirRec(OpenFile *from,
                      char **paths,
                      int len,
                      int to); // Change
                               // the current directory including path names

    bool FileExists(const char *name); // Check if a file exists in the current directory
    int GetFileSize(const char *name); // Return the size of a file
    bool IsDataFile(const char *name); // Check if a file is a data file

    void List();  // List all the files in the file system
    void Print(); // List all the files and their contents
    void PrintDirectory();
  private:
    OpenFile *freeMapFile; // Bit map of free disk blocks, represented as a file
    Lock *freeMapMutex;
    OpenFile *directoryFile; // Current directory
    Lock *directoryMutex;
    int DirectorySector;   // Sector of the current directory
    BitMap *openedFileMap; // Bit map of the opened files
    Lock *openedFileMutex;
    UserFile openedFiles[MaxOpenedFiles]; // Static table of opened files
};

#endif // FILESYS
#endif // FS_H
